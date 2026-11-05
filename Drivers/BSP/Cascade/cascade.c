/**
 * @file    cascade.c
 * @brief   Recovery 侧主从协同：从卡状态缓存、内部 485 命令应答、主卡转发状态机
 *
 * 角色来自配置区 role 字段（main app 在进 Recovery 前写入）：
 *   role_master：收主机固件 → 校验 → 按 1KB 块读回自己的 Flash → 一帧一答转发从卡
 *   role_slave ：只应答内部 0x10~0x15，不主动发帧
 *
 * 主卡状态机每次 cascade_poll() 只推进一个 485 事务，保证 0x03 还能插进来响应。
 */

#include "cascade.h"
#include "cascade_fsm.h"
#include "board_config.h"
#include "config_info.h"
#include "rs485.h"
#include "rtc.h"
#include "crc.h"
#include "iwdg.h"

#define CMD_QUERY_STATUS (0x00014C10U)
#define CMD_PREPARE      (0x00014C11U)
#define CMD_WRITE_BLOCK  (0x00014C12U)
#define CMD_VERIFY       (0x00014C13U)
#define CMD_ENTER_RCVRY  (0x00014C14U)
#define CMD_RESTART      (0x00014C15U)

#define REPLY(cmd)       (0x0000BC00U | ((cmd) & 0xFFU))

#define SYNC_BLOCK_BYTES (1024U)
#define FLASH_SECTOR_SIZE (0x20000U)

/* 0x13 从卡要先把 update_sta 落盘再回 ACK，内部会擦写 16KB config 扇区，
   实测远超普通 100ms 事务超时；这里单独放宽，避免“从卡已校验成功、主卡却判失败”。 */
#define VERIFY_TIMEOUT_MS (3000U)

static cascade_status_t s_slave;
static uint32_t s_next_query_tick;

/* 转发会话数据：状态本身只表示"走到哪一步"，这些是动作要用的上下文 */
static sync_state_t s_sync_state;
static uint32_t s_sync_size;
static uint32_t s_sync_crc;
static uint32_t s_sync_block;   /* 1-based */
static uint32_t s_sync_blocks;  /* total blocks */
static uint8_t s_block_retry;
static uint32_t s_sync_wait_tick;
static bool s_last_acked;       /* 最近一次 485 事务有没有应答，失败态用它记 online */

static uint32_t s_reply_buff[FRAME_MAX_LEN];

static void _set_slave_unknown(void)
{
    /* 只把升级状态标成 unknown：从卡刚才是否在线仍然是有效信息，
       0x04 之后紧接着的 0x03 不该突然报“从卡离线” */
    s_slave.update_sta = 3;
    s_slave.tick       = HAL_GetTick();
}

bool cascade_is_master(void)
{
    volatile const SysInfo_t *cfg = (volatile const SysInfo_t *)ADDR_CONFIG_SECTOR;
    if (!Is_Config_Integrity(cfg))
        return true; /* 配置无效时按主卡处理，避免两张卡都沉默 */
    return cfg->role == role_master;
}

void cascade_init(void)
{
    s_slave.valid      = false;
    s_slave.online     = false;
    s_slave.mode       = 2;
    s_slave.size       = 0;
    s_slave.crc32      = 0;
    s_slave.update_sta = 3;
    s_slave.tick       = 0;
    s_next_query_tick  = 0;

    s_sync_state  = SYNC_IDLE;
    s_sync_size   = 0;
    s_sync_crc    = 0;
    s_sync_block  = 1;
    s_sync_blocks = 0;
    s_block_retry = 0;
    s_last_acked  = false;
}

const cascade_status_t *cascade_status(void)
{
    return &s_slave;
}

bool cascade_is_syncing(void)
{
    return sync_state_active(s_sync_state);
}

void cascade_slave_mark_unknown(void)
{
    s_sync_state = SYNC_IDLE;
    _set_slave_unknown();
    /* 主卡正在收主机固件：这段时间不主动查从卡，避免 3 次重试把主循环拖住 */
    s_next_query_tick = HAL_GetTick() + 60000U;
}

/* ================================================================
 *  主卡侧：查询 / 转发
 * ================================================================ */

static void _query_status(void)
{
    if (s_sync_state == SYNC_FAILED)
        return; /* 失败状态保持到主机重新 0x04 或重新 0x06 */

    if (!rs485_transact(CMD_QUERY_STATUS, 0, NULL, 0, (IAP_Frame_t *)s_reply_buff,
                        RS485_TIMEOUT_MS, RS485_RETRIES)) {
        s_slave.online = false;
        return;
    }

    IAP_Frame_t *r = (IAP_Frame_t *)s_reply_buff;
    if (r->len != 4U)
        return;

    s_slave.mode       = (uint8_t)r->data_crc[0];
    s_slave.size       = r->data_crc[1];
    s_slave.crc32      = r->data_crc[2];
    s_slave.update_sta = r->data_crc[3];
    if (s_slave.update_sta > 2U)
        s_slave.update_sta = 3U;
    s_slave.online = true;
    s_slave.valid  = true;
    s_slave.tick   = HAL_GetTick();
}

/* ---- 状态动作：每个状态一次 poll 最多做一个 485 事务，返回事件 ---- */

/** @brief 进入 prepare：从卡可能刚从 main app 复位，给它 1.5s 起 Recovery */
static void _enter_prepare(void)
{
    s_sync_wait_tick = HAL_GetTick() + 1500U;
}

/** @brief 进入 blocks：复位块号/重试次数，0x03 显示 slave_update=1 */
static void _enter_blocks(void)
{
    s_slave.update_sta = 1;
    s_sync_block       = 1;
    s_block_retry      = 0;
}

/** @brief 进入 failed：锁住 slave_update=2，暂停后台 0x10 查询 */
static void _enter_failed(void)
{
    s_slave.online     = s_last_acked;
    s_slave.update_sta = 2;
    s_next_query_tick  = HAL_GetTick();
}

/** @brief 协议 6.3 第 1 步：确保从卡在 Recovery */
static sync_event_t _run_enter(void)
{
    s_last_acked = rs485_transact(CMD_ENTER_RCVRY, 0, NULL, 0, (IAP_Frame_t *)s_reply_buff,
                                  RS485_TIMEOUT_MS, RS485_RETRIES);
    return s_last_acked ? SYNC_EV_ACK : SYNC_EV_FAIL;
}

/** @brief 0x11：从卡擦 main app 区，擦完才应答，所以用 15s 专用超时且不重试 */
static sync_event_t _run_prepare(void)
{
    if ((int32_t)(HAL_GetTick() - s_sync_wait_tick) < 0)
        return SYNC_EV_WAIT;

    uint32_t req[2] = {s_sync_size, s_sync_crc};
    IAP_Frame_t *r  = (IAP_Frame_t *)s_reply_buff;

    s_last_acked = rs485_transact(CMD_PREPARE, 0, req, 2, r, 15000U, 0U);
    if (s_last_acked && r->len == 1U && r->data_crc[0] == 0U)
        return SYNC_EV_ACK;
    return SYNC_EV_FAIL;
}

/** @brief 0x12：一次发一块；成功 block++，失败累计 3 次转 fail */
static sync_event_t _run_blocks(void)
{
    uint32_t offset = (s_sync_block - 1U) * SYNC_BLOCK_BYTES;
    uint32_t remain = s_sync_size - offset;
    uint32_t words  = (remain >= SYNC_BLOCK_BYTES ? SYNC_BLOCK_BYTES : remain) / 4U;
    const uint32_t *data = (const uint32_t *)(ADDR_MAIN_APP + offset);
    IAP_Frame_t *r       = (IAP_Frame_t *)s_reply_buff;

    s_last_acked = rs485_transact(CMD_WRITE_BLOCK, s_sync_block, data, (uint16_t)words, r,
                                  RS485_TIMEOUT_MS, RS485_RETRIES);
    if (s_last_acked && r->len == 1U && r->data_crc[0] == 0U) {
        s_block_retry = 0;
        s_sync_block++;
        return (s_sync_block > s_sync_blocks) ? SYNC_EV_DONE : SYNC_EV_ACK;
    }

    s_block_retry++;
    return (s_block_retry >= 3U) ? SYNC_EV_FAIL : SYNC_EV_RETRY;
}

/** @brief 0x13：从卡算整包 CRC；一致才置 slave_update=0 */
static sync_event_t _run_verify(void)
{
    uint32_t req[2] = {s_sync_size, s_sync_crc};
    IAP_Frame_t *r  = (IAP_Frame_t *)s_reply_buff;

    s_last_acked = rs485_transact(CMD_VERIFY, 0, req, 2, r, VERIFY_TIMEOUT_MS, 1U);
    if (s_last_acked && r->len == 2U && r->data_crc[0] == 0U && r->data_crc[1] == s_sync_crc) {
        s_slave.online     = true;
        s_slave.update_sta = 0;
        s_slave.size       = s_sync_size;
        s_slave.crc32      = s_sync_crc;
        s_next_query_tick  = HAL_GetTick() + 1000U;
        return SYNC_EV_DONE;
    }
    return SYNC_EV_FAIL;
}

/* ---- 状态表：状态 → 本次 poll 做哪个事务 / 进入该状态时复位什么 ---- */
typedef sync_event_t (*sync_run_fn_t)(void);
typedef void (*sync_enter_fn_t)(void);

typedef struct {
    sync_state_t state;
    sync_run_fn_t run;
    sync_enter_fn_t on_enter;
} sync_state_def_t;

static const sync_state_def_t s_sync_defs[] = {
    [SYNC_IDLE]    = {SYNC_IDLE, NULL, NULL},
    [SYNC_ENTER]   = {SYNC_ENTER, _run_enter, NULL},
    [SYNC_PREPARE] = {SYNC_PREPARE, _run_prepare, _enter_prepare},
    [SYNC_BLOCKS]  = {SYNC_BLOCKS, _run_blocks, _enter_blocks},
    [SYNC_VERIFY]  = {SYNC_VERIFY, _run_verify, NULL},
    [SYNC_FAILED]  = {SYNC_FAILED, NULL, _enter_failed},
};

static const sync_state_def_t *_sync_def(sync_state_t state)
{
    if (state >= SYNC_STATE_COUNT)
        return NULL;
    return &s_sync_defs[state];
}

/** @brief 一次 poll 推进一步：动作返回事件 → 查转移表 → 执行目标状态的进入动作 */
static void _sync_step(void)
{
    const sync_state_def_t *def = _sync_def(s_sync_state);
    if (def == NULL || def->run == NULL)
        return;

    sync_event_t event = def->run();
    sync_state_t next  = sync_fsm_next(s_sync_state, event);
    if (next == s_sync_state)
        return;

    s_sync_state = next;

    const sync_state_def_t *next_def = _sync_def(next);
    if (next_def != NULL && next_def->on_enter != NULL)
        next_def->on_enter();
}

void cascade_start_sync(uint32_t size, uint32_t crc)
{
    s_sync_size   = size;
    s_sync_crc    = crc;
    s_sync_block  = 1;
    s_sync_blocks = (size + SYNC_BLOCK_BYTES - 1U) / SYNC_BLOCK_BYTES;
    s_block_retry = 0;
    s_last_acked  = false;
    s_sync_state  = SYNC_ENTER;

    s_slave.online     = true;
    s_slave.mode       = 1;
    s_slave.size       = size;
    s_slave.crc32      = crc;
    s_slave.update_sta = 1;
    s_slave.tick       = HAL_GetTick();
}

void cascade_refresh_if_stale(void)
{
    if ((RECOVERY_IAP_CAPS & 0x00000001U) == 0U)
        return;
    if (!cascade_is_master() || cascade_is_syncing())
        return;

    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - s_slave.tick) >= 1000)
        _query_status();
}

void cascade_poll(void)
{
    if ((RECOVERY_IAP_CAPS & 0x00000001U) == 0U)
        return; /* 单卡板没有从卡，不查也不转发 */

    if (!cascade_is_master())
        return;

    if (s_sync_state == SYNC_FAILED)
        return; /* 等主机重新 0x04/0x06，期间不查、不推进 */

    if (sync_state_active(s_sync_state)) {
        _sync_step();
        HAL_IWDG_Refresh(&hiwdg);
        return;
    }

    uint32_t now = HAL_GetTick();
    if ((int32_t)(now - s_next_query_tick) < 0)
        return;

    s_next_query_tick = now + 1000U;
    _query_status();
}

bool cascade_ensure_slave_recovery(void)
{
    if ((RECOVERY_IAP_CAPS & 0x00000001U) == 0U)
        return false;
    if (!cascade_is_master())
        return false;
    return rs485_transact(CMD_ENTER_RCVRY, 0, NULL, 0, (IAP_Frame_t *)s_reply_buff,
                          RS485_TIMEOUT_MS, RS485_RETRIES);
}

bool cascade_restart_slave(uint32_t delay_ms)
{
    if ((RECOVERY_IAP_CAPS & 0x00000001U) == 0U)
        return false;
    if (!cascade_is_master())
        return false;
    return rs485_transact(CMD_RESTART, 0, &delay_ms, 1, (IAP_Frame_t *)s_reply_buff,
                          RS485_TIMEOUT_MS, RS485_RETRIES);
}

void cascade_handle_reply(IAP_Frame_t *f)
{
    if (f->cmd == REPLY(CMD_QUERY_STATUS) && f->len == 4U) {
        s_slave.mode       = (uint8_t)f->data_crc[0];
        s_slave.size       = f->data_crc[1];
        s_slave.crc32      = f->data_crc[2];
        s_slave.update_sta = f->data_crc[3];
        if (s_slave.update_sta > 2U)
            s_slave.update_sta = 3U;
        s_slave.online = true;
        s_slave.valid  = true;
        s_slave.tick   = HAL_GetTick();
    }
}

/* ================================================================
 *  从卡侧：应答内部请求
 * ================================================================ */

static void _reply_status(uint32_t cmd, uint32_t seq, uint32_t status)
{
    rs485_send_frame(REPLY(cmd), seq, &status, 1);
}

static void _slave_prepare(IAP_Frame_t *f)
{
    if (f->len != 2U) {
        _reply_status(CMD_PREPARE, f->seq, 3);
        return;
    }

    uint32_t size = f->data_crc[0];
    uint32_t crc  = f->data_crc[1];
    if (size == 0U || size > (768U * 1024U) || (size & 3U) != 0U) {
        _reply_status(CMD_PREPARE, f->seq, 3);
        return;
    }

    SysInfo_t cfg = {0};
    memcpy(&cfg, (const void *)ADDR_CONFIG_SECTOR, sizeof(cfg));
    cfg.app_info.size = size;
    cfg.app_info.crc32 = crc;
    cfg.update_sta    = updating;
    cfg.role          = role_slave;
    Edit_Config_Info(&cfg);

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    /* 逐扇区擦 + 扇区之间刷 IWDG，避免 6 个 128KB 扇区把 5s 看门狗拖死 */
    uint32_t sectors     = (size + FLASH_SECTOR_SIZE - 1U) / FLASH_SECTOR_SIZE;
    HAL_StatusTypeDef st = HAL_OK;
    for (uint32_t i = 0; i < sectors; i++) {
        FLASH_EraseInitTypeDef erase = {
            .TypeErase    = FLASH_TYPEERASE_SECTORS,
            .Sector       = 6U + i,
            .NbSectors    = 1,
            .VoltageRange = FLASH_VOLTAGE_RANGE_3,
        };
        uint32_t sector_error = 0;
        st = HAL_FLASHEx_Erase(&erase, &sector_error);
        HAL_IWDG_Refresh(&hiwdg);
        if (st != HAL_OK)
            break;
    }
    HAL_FLASH_Lock();

    _reply_status(CMD_PREPARE, f->seq, (st == HAL_OK) ? 0U : 1U);
}

static void _slave_write_block(IAP_Frame_t *f)
{
    volatile const SysInfo_t *pcfg = (volatile const SysInfo_t *)ADDR_CONFIG_SECTOR;
    if (!Is_Config_Integrity(pcfg) || pcfg->update_sta != updating) {
        _reply_status(CMD_WRITE_BLOCK, f->seq, 2);
        return;
    }

    uint32_t seq  = f->seq;
    uint32_t size = pcfg->app_info.size;
    if (seq == 0U || f->len == 0U || f->len > 256U) {
        _reply_status(CMD_WRITE_BLOCK, f->seq, 3);
        return;
    }

    uint32_t offset = (seq - 1U) * SYNC_BLOCK_BYTES;
    if (offset >= size || offset + f->len * 4U > ((size + 3U) & ~3U)) {
        _reply_status(CMD_WRITE_BLOCK, f->seq, 3);
        return;
    }

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    HAL_StatusTypeDef st = HAL_OK;
    for (uint32_t i = 0; i < f->len; i++) {
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ADDR_MAIN_APP + offset + i * 4U,
                               f->data_crc[i]);
        if (st != HAL_OK)
            break;
    }
    HAL_FLASH_Lock();

    _reply_status(CMD_WRITE_BLOCK, f->seq, (st == HAL_OK) ? 0U : 1U);
}

static void _slave_verify(IAP_Frame_t *f)
{
    uint32_t out[2] = {1, 0};
    SysInfo_t cfg   = {0};
    memcpy(&cfg, (const void *)ADDR_CONFIG_SECTOR, sizeof(cfg));

    if (f->len != 2U || !Is_Config_Integrity(&cfg)) {
        out[0] = 1;
        rs485_send_frame(REPLY(CMD_VERIFY), f->seq, out, 2);
        return;
    }

    uint32_t size = f->data_crc[0];
    uint32_t crc  = HAL_CRC_Calculate(&hcrc, (uint32_t *)ADDR_MAIN_APP, (size + 3U) / 4U);
    out[1]        = crc;

    if (size == cfg.app_info.size && crc == f->data_crc[1]) {
        cfg.update_sta     = updated;
        cfg.app_info.crc32 = crc;
        Edit_Config_Info(&cfg);
        out[0] = 0;
    } else {
        out[0] = 1;
    }

    rs485_send_frame(REPLY(CMD_VERIFY), f->seq, out, 2);
}

static void _slave_enter_recovery(IAP_Frame_t *f)
{
    /* 已经在 Recovery：只应答，不复位 */
    _reply_status(CMD_ENTER_RCVRY, f->seq, 0);
}

static void _slave_restart(IAP_Frame_t *f)
{
    uint32_t delay_ms = (f->len >= 1U) ? f->data_crc[0] : 100U;
    if (delay_ms > 5000U)
        delay_ms = 5000U;

    _reply_status(CMD_RESTART, f->seq, 0);
    HAL_Delay(delay_ms < 20U ? 20U : delay_ms);
    HAL_IWDG_Refresh(&hiwdg);
    NVIC_SystemReset();
}

void cascade_handle_request(IAP_Frame_t *f)
{
    if ((RECOVERY_IAP_CAPS & 0x00000001U) == 0U)
        return; /* 单卡板没有从卡逻辑 */
    if (cascade_is_master())
        return; /* 主卡收到 0x4C 只可能是总线回显，不应答 */

    switch (f->cmd & 0xFFU) {
    case 0x10: {
        uint32_t out[4] = {1, 0, 0, 3};
        SysInfo_t cfg   = {0};
        memcpy(&cfg, (const void *)ADDR_CONFIG_SECTOR, sizeof(cfg));
        if (Is_Config_Integrity(&cfg)) {
            out[1] = cfg.app_info.size;
            out[2] = cfg.app_info.crc32;
            out[3] = cfg.update_sta;
        }
        rs485_send_frame(REPLY(CMD_QUERY_STATUS), f->seq, out, 4);
        break;
    }
    case 0x11:
        _slave_prepare(f);
        break;
    case 0x12:
        _slave_write_block(f);
        break;
    case 0x13:
        _slave_verify(f);
        break;
    case 0x14:
        _slave_enter_recovery(f);
        break;
    case 0x15:
        _slave_restart(f);
        break;
    default:
        _reply_status(f->cmd, f->seq, 2);
        break;
    }
}
