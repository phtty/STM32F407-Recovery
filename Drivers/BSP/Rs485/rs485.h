#ifndef DRIVERS_BSP_RS485_RS485_H
#define DRIVERS_BSP_RS485_RS485_H

#include "main.h"
#include "protocol.h"
#include <stdbool.h>

/* 内部 485 命令码：主卡→从卡 0x00014Cxx，从卡→主卡 0x0000BCxx */
#define RS485_CMD_TO_SLAVE   (0x0000004CU)
#define RS485_CMD_FROM_SLAVE (0x000000BCU)

#define RS485_TIMEOUT_MS (100U)
#define RS485_RETRIES    (3U)

void rs485_init(void);
void rs485_on_rx_byte(uint8_t byte);
RingBuff_t *rs485_rx_ring(void);
void rs485_flush_rx(void);
bool rs485_send_frame(uint32_t cmd, uint32_t seq, const uint32_t *data, uint16_t len_words);
IAP_FrameSta_t rs485_fetch_frame(IAP_Frame_t *out);
uint32_t rs485_reply_cmd(uint32_t req_cmd);
bool rs485_transact(uint32_t cmd, uint32_t seq, const uint32_t *data, uint16_t len_words,
                    IAP_Frame_t *reply, uint32_t timeout_ms, uint8_t retries);

#endif // !DRIVERS_BSP_RS485_RS485_H
