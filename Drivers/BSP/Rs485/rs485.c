/**
 * @file    rs485.c
 * @brief   Recovery 侧内部 485 总线：USART1 + PA8 方向控制 + 一帧一答
 *
 * 与显示级联共用同一条 485 总线，升级期间由主卡独占。帧格式与上位机协议一致
 * （IAP_Frame_t），只是命令码换成内部码：
 *   主卡→从卡 0x00014Cxx
 *   从卡→主卡 0x0000BCxx
 *
 * 约束：
 *   - 每次只发一帧，等到应答或 100ms 超时才发下一帧；
 *   - 最多重试 3 次，仍失败由上层置 slave_update=2；
 *   - 收发都刷新 IWDG，避免 1024 字节长帧把看门狗拖死。
 */

#include "rs485.h"
#include "usart.h"
#include "iwdg.h"
#include "crc.h"

static RingBuff_t s_rx;
static uint32_t s_tx[FRAME_MAX_LEN];
static uint32_t s_rx_tmp[FRAME_MAX_LEN];

static void _dir_tx(bool tx)
{
    HAL_GPIO_WritePin(RS485_RE_GPIO_Port, RS485_RE_Pin, tx ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void rs485_init(void)
{
    _dir_tx(false);
    BSP_RB_FreeBuff(&s_rx);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void rs485_on_rx_byte(uint8_t byte)
{
    BSP_RB_PutByte(&s_rx, byte);
}

RingBuff_t *rs485_rx_ring(void)
{
    return &s_rx;
}

void rs485_flush_rx(void)
{
    BSP_RB_FreeBuff(&s_rx);
}

uint32_t rs485_reply_cmd(uint32_t req_cmd)
{
    return ((uint32_t)RS485_CMD_FROM_SLAVE << 8) | (req_cmd & 0xFFU);
}

bool rs485_send_frame(uint32_t cmd, uint32_t seq, const uint32_t *data, uint16_t len_words)
{
    if (len_words > 256U)
        return false;

    IAP_Frame_t *f = (IAP_Frame_t *)s_tx;
    f->head        = FRAME_HEAD;
    f->seq         = seq;
    f->cmd         = cmd;
    f->len         = len_words;
    if (len_words != 0U && data != NULL)
        memcpy(f->data_crc, data, (uint32_t)len_words * sizeof(uint32_t));
    f->data_crc[len_words] = HAL_CRC_Calculate(&hcrc, s_tx, 4U + len_words);

    uint32_t bytes = (5U + len_words) * sizeof(uint32_t);

    _dir_tx(true);
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart1, (uint8_t *)s_tx, (uint16_t)bytes, 200U);

    /* 等最后一字节真正移出再释放总线；给个上界，UART 异常时不能死等 */
    uint32_t t0 = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_TC) == RESET &&
           (HAL_GetTick() - t0) < 20U) {
    }
    _dir_tx(false);

    return st == HAL_OK;
}

IAP_FrameSta_t rs485_fetch_frame(IAP_Frame_t *out)
{
    while (BSP_RB_GetAvailable(&s_rx) >= 4U) {
        uint32_t full_len = 0;
        uint32_t cmd      = 0;

        IAP_FrameSta_t st = check_frame_validity(&s_rx, &full_len, &cmd);
        if (st == iap_frame_rdy) {
            BSP_RB_GetByte_Bulk(&s_rx, (uint8_t *)out, full_len);
            return st;
        }
        if (st == iap_frame_wait)
            return st;
        BSP_RB_SkipBytes(&s_rx, 1);
    }
    return iap_frame_wait;
}

bool rs485_transact(uint32_t cmd, uint32_t seq, const uint32_t *data, uint16_t len_words,
                    IAP_Frame_t *reply, uint32_t timeout_ms, uint8_t retries)
{
    uint32_t expect = rs485_reply_cmd(cmd);

    for (uint8_t attempt = 0; attempt <= retries; attempt++) {
        rs485_flush_rx();
        if (!rs485_send_frame(cmd, seq, data, len_words))
            continue;

        uint32_t deadline = HAL_GetTick() + timeout_ms;
        while ((int32_t)(HAL_GetTick() - deadline) < 0) {
            HAL_IWDG_Refresh(&hiwdg);

            IAP_Frame_t *f = (IAP_Frame_t *)s_rx_tmp;
            if (rs485_fetch_frame(f) != iap_frame_rdy)
                continue;

            if (f->cmd == expect) {
                if (reply != NULL) {
                    uint32_t bytes = (5U + f->len) * sizeof(uint32_t);
                    memcpy(reply, f, bytes);
                }
                return true;
            }
            /* 不是本次事务的应答：丢掉，继续等 */
        }
    }
    return false;
}
