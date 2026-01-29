#ifndef DRIVERS_BSP_PROTOCOL_PROTOCOL_H
#define DRIVERS_BSP_PROTOCOL_PROTOCOL_H

#include "main.h"
#include "RingBuffer.h"
#include <stdbool.h>

#define FRAME_MIN_LEN     (5U)
#define FRAME_MAX_LEN     (5U + 256U)

#define FRAME_HEAD        (0x5A5A5A5AU)
#define FRAME_HEAD_OFFSET (0U)
#define FRAME_SEQ_OFFSET  (1U)
#define FRAME_CMD_OFFSET  (2U)
#define FRAME_LEN_OFFSET  (3U)

__attribute__((packed)) typedef struct frame_struct {
    uint32_t head;       // 帧头
    uint32_t seq;        // 帧序号
    uint32_t cmd;        // 命令字
    uint32_t len;        // 数据长度（data长度）(单位:word,1word=4byte)
    uint32_t data_crc[]; // 柔性数组，数据区
} IAP_Frame_t;

void handle_protocol(void);
bool check_frame_validity(const RingBuffer *buff, uint32_t *data_len, uint8_t *cmd_num);

#endif // !DRIVERS_BSP_PROTOCOL_PROTOCOL_H
