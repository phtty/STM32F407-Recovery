#include "protocol.h"
#include "crc.h"
#include "udp_conn.h"
#include "config_info.h"
#include "cmd.h"

uint8_t ptcl_buff[FRAME_MAX_LEN * 4] = {0};

const uint8_t frame_len[] = {0, 0, 4, 0, 1, 0, 0, 0};

/**
 * @brief 将缓冲区中的数据进行协议解析
 *
 */
void handle_protocol(void)
{
    if (!udp_rx_flag)
        return;
    udp_rx_flag = false;

    // 解析缓冲区的数据直到缓冲区有效内容长度小于1字节
    while (BSP_RB_GetAvailable(&ringbuf) >= 4) {
        uint32_t frame_len = 0;
        uint8_t cmd_num    = 0;

        IAP_FrameSta_t status = check_frame_validity(&ringbuf, &frame_len, &cmd_num);

        if (status == iap_frame_rdy) {
            BSP_RB_GetByte_Bulk(&ringbuf, ptcl_buff, frame_len);
            cmd_Functions[cmd_num]((IAP_Frame_t *)ptcl_buff);

        } else if (status == iap_frame_wait) { // 数据包不完整
            break;

        } else { // 确认为脏数据或伪造包，滑动1字节继续查找
            BSP_RB_SkipBytes(&ringbuf, 1);
        }
    }
}

/**
 * @brief 检查帧格式
 *
 * @param buff 环形缓冲区指针
 * @param total_len 帧长度
 * @param cmd_num 识别到的指令码内容
 * @return true 合法帧
 * @return false 非法帧
 */
IAP_FrameSta_t check_frame_validity(const RingBuff_t *buff, uint32_t *total_len, uint8_t *cmd_num)
{
    uint32_t available = BSP_RB_GetAvailable(buff);

    // 基础长度检查
    if (available < 4)
        return iap_frame_wait;

    // 帧头检查
    uint32_t head = 0;
    BSP_RB_PeekBlock(buff, 0, (uint8_t *)&head, 4);
    if (head != FRAME_HEAD)
        return iap_frame_fake; // 开头就不是5A，直接标记为fake触发跳过

    // 长度合法性初步检查 (防止恶意大包或伪造包)
    uint32_t payload_len = 0;
    if (available >= (FRAME_LEN_OFFSET + 1) * 4) {
        BSP_RB_PeekBlock(buff, FRAME_LEN_OFFSET * 4, (uint8_t *)&payload_len, 4);

        if (payload_len > 256)
            return iap_frame_fake; // 协议规定最大256，超过范围则为伪造帧头

    } else {
        return iap_frame_wait; // 连长度字段都还没收到
    }

    uint32_t full_bytes = (payload_len + FRAME_MIN_LEN) * 4;

    // 数据完整性检查
    if (available < full_bytes) {
        // --- 关键点：在等待期间，扫描剩余数据里是否有新的帧头 ---
        // 如果在当前半截包的范围内发现了新帧头，说明当前这个头是“伪造”或者“残缺”的
        for (uint32_t i = 1; i <= available - 4; i++) {
            uint32_t next_head = 0;
            BSP_RB_PeekBlock(buff, i, (uint8_t *)&next_head, 4);

            if (next_head == FRAME_HEAD)
                return iap_frame_fake;
        }
        return iap_frame_wait;
    }

    // CRC最终验证
    static uint32_t tmp_buff[FRAME_MAX_LEN];
    BSP_RB_PeekBlock(buff, 0, (uint8_t *)tmp_buff, full_bytes);
    IAP_Frame_t *ptemp = (IAP_Frame_t *)tmp_buff;

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)ptemp, ptemp->len + 4);
    if (crc != ptemp->data_crc[ptemp->len]) {
        return iap_frame_fake; // 长度够了但校验不过，判定为伪造或损坏
    }

    // 验证通过
    *total_len = full_bytes;
    *cmd_num   = ptemp->cmd & 0xFF;
    return iap_frame_rdy;
}
