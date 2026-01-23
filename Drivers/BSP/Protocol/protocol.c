#include "protocol.h"
#include "crc.h"
#include "udp_conn.h"
#include "config_info.h"

uint8_t ptcl_buff[2048] = {0};

const uint8_t frame_len[] = {0, 4, 0, 1, 0, 0, 0};

void handle_protocol(void)
{
    if (!udp_rx_flag)
        return;
    udp_rx_flag = false;

    while (BSP_RB_GetAvailable(&ringbuf) >= FRAME_MIN_LEN) {
        uint32_t data_len = 0;
        uint8_t cmd_num   = 0;

        if (check_frame_validity(&ringbuf, &data_len, &cmd_num)) {
            BSP_RB_GetByte_Bulk(&ringbuf, ptcl_buff, data_len);
            // cmd_function[cmd_num]((IAP_Frame_t *)ptcl_buff);

        } else {
            BSP_RB_SkipBytes(&ringbuf, 1);
        }

        if (BSP_RB_IsFull(&ringbuf))
            BSP_RB_FreeBuff(&ringbuf);
    }
}

bool check_frame_validity(const RingBuffer *buff, uint32_t *data_len, uint8_t *cmd_num)
{
    static uint8_t tmp_buff[FRAME_MAX_LEN] = {0};

    uint32_t temp_len = 0;
    BSP_RB_PeekBlock(buff, FRAME_LEN_OFFSET, (uint8_t *)(&temp_len), sizeof(temp_len));
    BSP_RB_PeekBlock(buff, 0, tmp_buff, temp_len);

    IAP_Frame_t *ptemp = (IAP_Frame_t *)(&tmp_buff);

    if (ptemp->head != FRAME_HEAD)
        return false;

    if ((pConfig->update_sta != updating) && (ptemp->seq != 0))
        return false;

    uint8_t cmd     = ptemp->cmd & 0xff;
    uint8_t cmd_dir = (ptemp->cmd >> 8) & 0xff;
    if (cmd_dir != 0x4B)
        return false;
    if ((cmd < 0x01) || (cmd > 0x07))
        return false;
    if ((ptemp->len != frame_len[cmd]) && (cmd != 0x05))
        return false;

    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)ptemp, ptemp->len + 4);
    if (crc != ptemp->data_crc[ptemp->len])
        return false;

    *data_len = ptemp->len;
    *cmd_num  = cmd;

    return true;
}
