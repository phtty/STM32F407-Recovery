#include "protocol.h"
#include "crc.h"
#include "udp_conn.h"
#include "config_info.h"
#include "cmd.h"
#include "cascade.h"
#include "rs485.h"

uint8_t ptcl_buff[FRAME_MAX_LEN * 4] = {0};

IAP_CmdDir_t iap_cmd_dir(uint32_t cmd)
{
    uint32_t dir  = (cmd >> 8) & 0xFFU;
    uint32_t addr = (cmd >> 16) & 0xFFFFU;

    /* 上位机命令地址恒为 0；内部命令地址 1=从卡，应答地址 0=主卡 */
    if (dir == FRAME_DIR_HOST && addr == 0U)
        return iap_dir_host;
    if (dir == FRAME_DIR_TO_SLAVE && addr == 1U)
        return iap_dir_to_slave;
    if (dir == FRAME_DIR_FROM_SLAVE && addr == 0U)
        return iap_dir_from_slave;
    return iap_dir_none;
}

/** @brief 上位机命令：按低字节查命令表，越界直接丢 */
static void _dispatch_host(IAP_Frame_t *f)
{
    uint32_t id = f->cmd & 0xFFU;
    if (id < CMD_HOST_COUNT && cmd_Functions[id] != NULL)
        cmd_Functions[id](f);
}

/** @brief 从一帧的环形缓冲里连续取帧并分发 */
static void _drain(const RingBuff_t *rb, bool from_udp)
{
    while (BSP_RB_GetAvailable(rb) >= 4U) {
        uint32_t full_len = 0;
        uint32_t cmd      = 0;

        IAP_FrameSta_t st = check_frame_validity(rb, &full_len, &cmd);
        if (st == iap_frame_wait)
            break;
        if (st != iap_frame_rdy) {
            BSP_RB_SkipBytes((RingBuff_t *)rb, 1);
            continue;
        }

        BSP_RB_GetByte_Bulk((RingBuff_t *)rb, ptcl_buff, full_len);
        IAP_Frame_t *f  = (IAP_Frame_t *)ptcl_buff;
        IAP_CmdDir_t dir = iap_cmd_dir(f->cmd);

        if (from_udp) {
            /* 网口只接受上位机命令；内部命令混进来一律忽略 */
            if (dir == iap_dir_host)
                _dispatch_host(f);
        } else {
            /* 485 只接受内部命令：主卡收应答，从卡收请求 */
            if (dir == iap_dir_to_slave)
                cascade_handle_request(f);
            else if (dir == iap_dir_from_slave)
                cascade_handle_reply(f);
        }
    }
}

/**
 * @brief 将缓冲区中的数据进行协议解析
 *
 * 网口和内部 485 各有一个环形缓冲；两者互不阻塞：
 * 485 事务在 cascade_poll() 里主动发起，这里只负责把已到达的帧分发掉。
 */
void handle_protocol(void)
{
    if (udp_rx_flag) {
        udp_rx_flag = false;
        _drain(&ringbuf, true);
    }

    _drain(rs485_rx_ring(), false);
}

/**
 * @brief 检查帧格式
 *
 * @param buff 环形缓冲区指针
 * @param total_len 帧长度（字节）
 * @param cmd 完整命令码（含方向字节）
 * @return IAP_FrameSta_t 帧状态
 */
IAP_FrameSta_t check_frame_validity(const RingBuff_t *buff, uint32_t *total_len, uint32_t *cmd)
{
    uint32_t available = BSP_RB_GetAvailable(buff);

    // 基础长度检查
    if (available < 4U)
        return iap_frame_wait;

    // 帧头检查
    uint32_t head = 0;
    BSP_RB_PeekBlock(buff, 0, (uint8_t *)&head, 4);
    if (head != FRAME_HEAD)
        return iap_frame_fake; // 开头就不是 5A，直接标记为 fake 触发跳过

    // 长度合法性初步检查（防止恶意大包或伪造包）
    uint32_t payload_len = 0;
    if (available >= (FRAME_LEN_OFFSET + 1U) * 4U) {
        BSP_RB_PeekBlock(buff, FRAME_LEN_OFFSET * 4U, (uint8_t *)&payload_len, 4);

        if (payload_len > 256U)
            return iap_frame_fake; // 协议规定最大 256，超过范围则为伪造帧头
    } else {
        return iap_frame_wait; // 连长度字段都还没收到
    }

    uint32_t full_bytes = (payload_len + FRAME_MIN_LEN) * 4U;

    // 数据完整性检查
    if (available < full_bytes) {
        // 等待期间扫描剩余数据里是否有新的帧头：有则当前这个是残包/伪头
        for (uint32_t i = 1; i + 4U <= available; i++) {
            uint32_t next_head = 0;
            BSP_RB_PeekBlock(buff, i, (uint8_t *)&next_head, 4);
            if (next_head == FRAME_HEAD)
                return iap_frame_fake;
        }
        return iap_frame_wait;
    }

    // CRC 最终验证
    static uint32_t tmp_buff[FRAME_MAX_LEN];
    BSP_RB_PeekBlock(buff, 0, (uint8_t *)tmp_buff, full_bytes);
    IAP_Frame_t *ptemp = (IAP_Frame_t *)tmp_buff;

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)ptemp, ptemp->len + 4U);
    if (crc != ptemp->data_crc[ptemp->len])
        return iap_frame_fake; // 长度够了但校验不过，判定为伪造或损坏

    // 验证通过
    *total_len = full_bytes;
    if (cmd != NULL)
        *cmd = ptemp->cmd;
    return iap_frame_rdy;
}
