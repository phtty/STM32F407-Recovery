#include "cmd.h"
#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"
#include "rtc.h"
#include "crc.h"
#include "iwdg.h"
#include "cascade.h"

#define U8_LEN(x)  ((x) * sizeof(uint32_t))
#define U32_LEN(y) ((y) / sizeof(uint32_t))

#define FW_BLOCK_WORDS (256U)
#define FW_BLOCK_BYTES (1024U)
#define FW_MAX_SIZE    (768U * 1024U)
#define FW_MAX_FRAMES  (FW_MAX_SIZE / FW_BLOCK_BYTES)

static void cmd_Test_00(IAP_Frame_t *IAP_Data);
static void cmd_ReportIp_01(IAP_Frame_t *IAP_Data);
static void cmd_ForceModifyIP_02(IAP_Frame_t *IAP_Data);
static void cmd_ReportFirmwareStatus_03(IAP_Frame_t *IAP_Data);
static void cmd_PrepareUpgrade_04(IAP_Frame_t *IAP_Data);
static void cmd_SendUpgradePackage_05(IAP_Frame_t *IAP_Data);
static void cmd_EnterRecoveryMode_06(IAP_Frame_t *IAP_Data);
static void cmd_Restart_07(IAP_Frame_t *IAP_Data);
static void cmd_Reserved_08(IAP_Frame_t *IAP_Data);
static void cmd_Reserved_09(IAP_Frame_t *IAP_Data);
static void cmd_Reserved_0A(IAP_Frame_t *IAP_Data);
static void cmd_Reserved_0B(IAP_Frame_t *IAP_Data);
static void cmd_TrigTestKey_0C(IAP_Frame_t *IAP_Data);

static HAL_StatusTypeDef write_fw(uint32_t addr_offset, const uint32_t *data, uint16_t word_len);

const pfcmd_Functions cmd_Functions[] = {
    cmd_Test_00,
    cmd_ReportIp_01,
    cmd_ForceModifyIP_02,
    cmd_ReportFirmwareStatus_03,
    cmd_PrepareUpgrade_04,
    cmd_SendUpgradePackage_05,
    cmd_EnterRecoveryMode_06,
    cmd_Restart_07,
    cmd_Reserved_08,
    cmd_Reserved_09,
    cmd_Reserved_0A,
    cmd_Reserved_0B,
    cmd_TrigTestKey_0C,
};

/* ---- 收包会话 ---- */
static uint32_t s_fw_size;       /* 本次固件字节数 */
static uint32_t s_fw_crc_exp;    /* 主机给的整包 CRC32 */
static uint32_t s_total_frames;  /* 总包数 */
static uint8_t s_bitmap[FW_MAX_FRAMES / 8U];
static bool s_receiving;
static uint32_t s_last_missing[256];
static uint32_t s_last_missing_cnt;
static uint32_t s_missing_tmp[256]; /* 1KB：不放在 2KB 栈上 */

/**
 * @brief 构造应答帧并发送（上位机方向）
 */
static void cmd_SendReData(uint32_t ReSeq, uint32_t ReCmd, uint32_t ReLen, const uint32_t *ReData)
{
    static uint32_t ReBuff[FRAME_MAX_LEN];

    IAP_Frame_t *pIAP_ReTmp = (IAP_Frame_t *)&(ReBuff);
    pIAP_ReTmp->head        = FRAME_HEAD;
    pIAP_ReTmp->seq         = ReSeq;
    pIAP_ReTmp->cmd         = ReCmd;
    pIAP_ReTmp->len         = ReLen;

    if (ReLen != 0U && ReData != NULL)
        memcpy(pIAP_ReTmp->data_crc, ReData, U8_LEN(ReLen));

    pIAP_ReTmp->data_crc[ReLen] = HAL_CRC_Calculate(&hcrc, (uint32_t *)pIAP_ReTmp, 4U + ReLen);

    if (ReCmd == rtn_cmd01 || ReCmd == rtn_cmd02)
        udp_send_data(udp_pcb, boardcast, (uint8_t *)pIAP_ReTmp, sizeof(IAP_Frame_t) + U8_LEN(ReLen) + sizeof(uint32_t));
    else
        udp_send_data(udp_pcb, unicast, (uint8_t *)pIAP_ReTmp, sizeof(IAP_Frame_t) + U8_LEN(ReLen) + sizeof(uint32_t));
}

static void _read_config(SysInfo_t *cfg)
{
    memcpy(cfg, (const void *)ADDR_CONFIG_SECTOR, sizeof(SysInfo_t));
}

static bool _bit_get(uint32_t idx)
{
    if (idx >= FW_MAX_FRAMES)
        return false;
    return (s_bitmap[idx / 8U] & (uint8_t)(1U << (idx % 8U))) != 0U;
}

static void _bit_set(uint32_t idx)
{
    if (idx < FW_MAX_FRAMES)
        s_bitmap[idx / 8U] |= (uint8_t)(1U << (idx % 8U));
}

/**
 * @brief 查询模式/能力（0x00）
 */
static void cmd_Test_00(IAP_Frame_t *IAP_Data)
{
    if (IAP_Data->len < 1U)
        return;
    if (IAP_Data->data_crc[0] != CONFIG_MAGIC)
        return;

    uint32_t ReData[4] = {CONFIG_MAGIC, 1U /* Recovery */, IAP_CAPS, IAP_HW_ID};
    cmd_SendReData(IAP_Data->seq, rtn_cmd00, U32_LEN(sizeof(ReData)), ReData);
}

/**
 * @brief 报告 IP 地址（0x01）
 */
static void cmd_ReportIp_01(IAP_Frame_t *IAP_Data)
{
    SysInfo_t config_info = {0};
    _read_config(&config_info);

    uint32_t ReData[4] = {0};
    ReData[0]          = (uint32_t)config_info.net_cfg.ip[0] << 24 | (uint32_t)config_info.net_cfg.ip[1] << 16 |
                         (uint32_t)config_info.net_cfg.ip[2] << 8 | config_info.net_cfg.ip[3];
    ReData[1]          = (uint32_t)config_info.net_cfg.mask[0] << 24 | (uint32_t)config_info.net_cfg.mask[1] << 16 |
                         (uint32_t)config_info.net_cfg.mask[2] << 8 | config_info.net_cfg.mask[3];
    ReData[2]          = (uint32_t)config_info.net_cfg.gw[0] << 24 | (uint32_t)config_info.net_cfg.gw[1] << 16 |
                         (uint32_t)config_info.net_cfg.gw[2] << 8 | config_info.net_cfg.gw[3];
    ReData[3]          = config_info.net_cfg.port;

    cmd_SendReData(IAP_Data->seq, rtn_cmd01, U32_LEN(sizeof(ReData)), ReData);
}

/**
 * @brief 强制修改 IP 地址（0x02）
 */
static void cmd_ForceModifyIP_02(IAP_Frame_t *IAP_Data)
{
    if (IAP_Data->len < 4U)
        return;

    SysInfo_t config_info = {0};
    _read_config(&config_info);

    config_info.net_cfg.ip[0]   = (uint8_t)(IAP_Data->data_crc[0] >> 24);
    config_info.net_cfg.ip[1]   = (uint8_t)(IAP_Data->data_crc[0] >> 16);
    config_info.net_cfg.ip[2]   = (uint8_t)(IAP_Data->data_crc[0] >> 8);
    config_info.net_cfg.ip[3]   = (uint8_t)(IAP_Data->data_crc[0]);
    config_info.net_cfg.mask[0] = (uint8_t)(IAP_Data->data_crc[1] >> 24);
    config_info.net_cfg.mask[1] = (uint8_t)(IAP_Data->data_crc[1] >> 16);
    config_info.net_cfg.mask[2] = (uint8_t)(IAP_Data->data_crc[1] >> 8);
    config_info.net_cfg.mask[3] = (uint8_t)(IAP_Data->data_crc[1]);
    config_info.net_cfg.gw[0]   = (uint8_t)(IAP_Data->data_crc[2] >> 24);
    config_info.net_cfg.gw[1]   = (uint8_t)(IAP_Data->data_crc[2] >> 16);
    config_info.net_cfg.gw[2]   = (uint8_t)(IAP_Data->data_crc[2] >> 8);
    config_info.net_cfg.gw[3]   = (uint8_t)(IAP_Data->data_crc[2]);
    config_info.net_cfg.port    = IAP_Data->data_crc[3];

    Edit_Config_Info(&config_info);

    cmd_SendReData(IAP_Data->seq, rtn_cmd02, 0, NULL);
}

/**
 * @brief 查询主卡+从卡状态（0x03）
 */
static void cmd_ReportFirmwareStatus_03(IAP_Frame_t *IAP_Data)
{
    SysInfo_t config_info = {0};
    _read_config(&config_info);

    cascade_refresh_if_stale();
    const cascade_status_t *slave = cascade_status();

    uint32_t ReData[8] = {0};
    ReData[0]          = config_info.app_info.size;
    ReData[1]          = config_info.app_info.crc32;
    ReData[2]          = s_receiving ? 1U : config_info.update_sta;

    if (slave->online) {
        ReData[3] = 1U;
        ReData[4] = slave->mode;
        ReData[5] = slave->size;
        ReData[6] = slave->crc32;
        ReData[7] = slave->update_sta;
    } else {
        ReData[3] = 0U;
        ReData[4] = 2U;
        ReData[5] = 0U;
        ReData[6] = 0U;
        ReData[7] = 3U;
    }

    cmd_SendReData(IAP_Data->seq, rtn_cmd03, U32_LEN(sizeof(ReData)), ReData);
}

/**
 * @brief 准备升级（0x04）：只擦主卡 main app 区
 */
static void cmd_PrepareUpgrade_04(IAP_Frame_t *IAP_Data)
{
    uint32_t status = 3U;

    do {
        if (IAP_Data->len != 2U)
            break;

        uint32_t fw_size = IAP_Data->data_crc[0];
        uint32_t fw_crc  = IAP_Data->data_crc[1];
        if (fw_size == 0U || fw_size > FW_MAX_SIZE || (fw_size & 3U) != 0U)
            break;
        if (!cascade_is_master())
            break;
        if (s_receiving || cascade_is_syncing()) {
            status = 2U;
            break;
        }

        HAL_FLASH_Unlock();
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

        /* 一次只擦一个扇区并在扇区之间刷 IWDG：128KB 扇区擦除最长 2s，
           6 个扇区连着擦会超过 5s 的看门狗窗口。 */
        uint32_t sectors = (fw_size + FLASH_SECTOR_SIZE - 1U) / FLASH_SECTOR_SIZE;
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

        if (st != HAL_OK) {
            status = 1U;
            break;
        }

        SysInfo_t cfg      = {0};
        _read_config(&cfg);
        cfg.app_info.size  = fw_size;
        cfg.app_info.crc32 = 0xFFFFFFFFU;
        cfg.update_sta     = updating;
        cfg.role           = role_master;
        Edit_Config_Info(&cfg);

        memset(s_bitmap, 0, sizeof(s_bitmap));
        s_fw_size        = fw_size;
        s_fw_crc_exp     = fw_crc;
        s_total_frames   = (fw_size + FW_BLOCK_BYTES - 1U) / FW_BLOCK_BYTES;
        s_last_missing_cnt = 0;
        s_receiving      = true;

        cascade_slave_mark_unknown();
        status = 0U;
    } while (0);

    cmd_SendReData(IAP_Data->seq, rtn_cmd04, 1, &status);
}

/**
 * @brief 发送升级包（0x05）：只在最后一包回缺失列表
 */
static void cmd_SendUpgradePackage_05(IAP_Frame_t *IAP_Data)
{
    uint32_t seq = IAP_Data->seq;

    /* 收包结束后重复发最后一包：回缓存结果，不重开一轮 */
    if (!s_receiving) {
        if (s_total_frames != 0U && seq == s_total_frames)
            cmd_SendReData(0, rtn_cmd05, s_last_missing_cnt, s_last_missing);
        return;
    }

    if (seq == 0U || seq > s_total_frames || IAP_Data->len == 0U || IAP_Data->len > FW_BLOCK_WORDS)
        return;

    uint32_t offset = (seq - 1U) * FW_BLOCK_BYTES;
    if (offset + IAP_Data->len * 4U > ((s_fw_size + 3U) & ~3U))
        return;

    if (!_bit_get(seq - 1U)) {
        if (write_fw(offset, IAP_Data->data_crc, (uint16_t)IAP_Data->len) == HAL_OK)
            _bit_set(seq - 1U);
        /* 写失败不中止：该包不进 bitmap，最后会出现在缺失列表里 */
    }

    if (seq != s_total_frames)
        return;

    /* 最后一包：统计缺失 */
    uint32_t *missing = s_missing_tmp;
    uint32_t miss_cnt = 0;
    for (uint32_t i = 0; i < s_total_frames; i++) {
        if (!_bit_get(i)) {
            if (miss_cnt < 256U)
                missing[miss_cnt] = i + 1U;
            miss_cnt++;
        }
    }

    if (miss_cnt == 0U) {
        uint32_t crc      = HAL_CRC_Calculate(&hcrc, (uint32_t *)ADDR_MAIN_APP, (s_fw_size + 3U) / 4U);
        SysInfo_t cfg     = {0};
        _read_config(&cfg);
        cfg.app_info.size = s_fw_size;

        if (crc == s_fw_crc_exp) {
            cfg.app_info.crc32 = crc;
            cfg.update_sta     = updated;
            Edit_Config_Info(&cfg);
            s_receiving = false;
            cascade_start_sync(s_fw_size, crc);
        } else {
            cfg.app_info.crc32 = 0xFFFFFFFFU;
            cfg.update_sta     = failed;
            Edit_Config_Info(&cfg);
            s_receiving = false;
        }
    }

    s_last_missing_cnt = (miss_cnt > 256U) ? 256U : miss_cnt;
    memcpy(s_last_missing, missing, s_last_missing_cnt * sizeof(uint32_t));
    cmd_SendReData(0, rtn_cmd05, s_last_missing_cnt, s_last_missing);
}

/**
 * @brief 进入 Recovery（0x06）：主卡已在 Recovery 时只补发从卡 0x14，不复位
 */
static void cmd_EnterRecoveryMode_06(IAP_Frame_t *IAP_Data)
{
    if (cascade_is_master())
        cascade_ensure_slave_recovery();

    cmd_SendReData(IAP_Data->seq, rtn_cmd06, 0, NULL);
}

/**
 * @brief 重启主卡+从卡（0x07）
 */
static void cmd_Restart_07(IAP_Frame_t *IAP_Data)
{
    uint32_t status = 2U;

    do {
        if (s_receiving || cascade_is_syncing())
            break;
        if (!cascade_is_master())
            break;

        /* 主卡自身升级必须已完成；否则复位会回到半升级状态 */
        SysInfo_t cfg = {0};
        _read_config(&cfg);
        if (cfg.update_sta != updated)
            break;

        if (IAP_CAPS & IAP_CAP_SLAVE) {
            const cascade_status_t *sl = cascade_status();
            if (!sl->online || sl->update_sta != 0U)
                break;
            if (!cascade_restart_slave(100U)) {
                status = 1U;
                break;
            }
        }

        status = 0U;
        cmd_SendReData(IAP_Data->seq, rtn_cmd07, 1, &status);
        HAL_Delay(50);
        HAL_IWDG_Refresh(&hiwdg);
        NVIC_SystemReset();
        return;
    } while (0);

    cmd_SendReData(IAP_Data->seq, rtn_cmd07, 1, &status);
}

static void cmd_Reserved_08(IAP_Frame_t *IAP_Data)
{
    (void)IAP_Data;
}

static void cmd_Reserved_09(IAP_Frame_t *IAP_Data)
{
    (void)IAP_Data;
}

static void cmd_Reserved_0A(IAP_Frame_t *IAP_Data)
{
    (void)IAP_Data;
}

static void cmd_Reserved_0B(IAP_Frame_t *IAP_Data)
{
    (void)IAP_Data;
}

/** @brief 0x0C 触发 TEST 键：仅 main app 支持，Recovery 不应答 */
static void cmd_TrigTestKey_0C(IAP_Frame_t *IAP_Data)
{
    (void)IAP_Data;
}

/**
 * @brief 写入固件到 flash（单帧）
 */
static HAL_StatusTypeDef write_fw(uint32_t addr_offset, const uint32_t *data, uint16_t word_len)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    for (uint32_t i = 0; i < word_len; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ADDR_MAIN_APP + addr_offset + i * 4U, data[i]);
        if (status != HAL_OK)
            break;
    }

    HAL_FLASH_Lock();
    return status;
}
