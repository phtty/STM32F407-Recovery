#ifndef DRIVERS_BSP_PROTOCOL_PROTOCOL_H
#define DRIVERS_BSP_PROTOCOL_PROTOCOL_H

#include "main.h"
#include "RingBuff.h"
#include <stdbool.h>

#define FRAME_MIN_LEN     (5U)
#define FRAME_MAX_LEN     (5U + 256U)

#define FRAME_HEAD        (0x5A5A5A5AU)
#define FRAME_HEAD_OFFSET (0U)
#define FRAME_SEQ_OFFSET  (1U)
#define FRAME_CMD_OFFSET  (2U)
#define FRAME_LEN_OFFSET  (3U)

typedef struct frame_struct {
    uint32_t head;
    uint32_t seq;
    uint32_t cmd;
    uint32_t len;
    uint32_t data_crc[];
} IAP_Frame_t;

void handle_protocol(void);
bool check_frame_validity(const RingBuff_t *buff, uint32_t *data_len, uint8_t *cmd_num);

#endif // !DRIVERS_BSP_PROTOCOL_PROTOCOL_H
