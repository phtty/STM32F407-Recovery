#include "protocol.h"
#include "crc.h"
#include "udp_conn.h"
#include "config_info.h"

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

    // 解析缓冲区的数据直到缓冲区有效内容长度小于最短有效帧长度
    while (BSP_RB_GetAvailable(&ringbuf) >= FRAME_MIN_LEN * 4) {
        uint32_t data_len = 0;
        uint8_t cmd_num   = 0;

        if (check_frame_validity(&ringbuf, &data_len, &cmd_num)) {
            BSP_RB_GetByte_Bulk(&ringbuf, ptcl_buff, data_len + FRAME_MIN_LEN * 4);
            // cmd_function[cmd_num]((IAP_Frame_t *)ptcl_buff);

        } else { // 解析失败，跳过1字节
            BSP_RB_SkipBytes(&ringbuf, 1);
        }

        // 若缓冲区已满，则清空缓冲区
        if (BSP_RB_IsFull(&ringbuf))
            BSP_RB_FreeBuff(&ringbuf);
    }
}

/**
 * @brief 检查帧格式
 *
 * @param buff 环形缓冲区指针
 * @param data_len 识别到的数据部分长度
 * @param cmd_num 识别到的指令码内容
 * @return true 合法帧
 * @return false 非法帧
 */
bool check_frame_validity(const RingBuff_t *buff, uint32_t *data_len, uint8_t *cmd_num)
{
    static uint8_t tmp_buff[FRAME_MAX_LEN * 4] = {0};

    // 验证帧合法性只能通过peek操作进行
    uint32_t temp_len = 0;
    BSP_RB_PeekBlock(buff, FRAME_LEN_OFFSET, (uint8_t *)(&temp_len), sizeof(temp_len));
    BSP_RB_PeekBlock(buff, 0, tmp_buff, temp_len + FRAME_MIN_LEN * 4);

    // 通过协议结构体去访问这个帧的所有内容
    IAP_Frame_t *ptemp = (IAP_Frame_t *)(&tmp_buff);

    if (ptemp->head != FRAME_HEAD) // 检查帧头
        return false;

    if ((pConfig->update_sta != updating) && (ptemp->seq != 0)) // 检查序列号
        return false;

    // 检查命令码
    uint8_t cmd     = ptemp->cmd & 0xff;
    uint8_t cmd_dir = (ptemp->cmd >> 8) & 0xff;
    if (cmd_dir != 0x4B)
        return false;
    if ((cmd < 0x01) || (cmd > 0x07))
        return false;
    if ((ptemp->len != frame_len[cmd]) && (cmd != 0x05))
        return false;

    // 检查CRC校验码
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)ptemp, ptemp->len + 4);
    if (crc != ptemp->data_crc[ptemp->len])
        return false;

    // 合法帧，返回解析到的数据部分长度和命令码
    *data_len = ptemp->len + FRAME_MIN_LEN * 4;
    *cmd_num  = cmd;

    return true;
}
