#ifndef DRIVERS_BSP_PROTOCOL_PROTOCOL_H
#define DRIVERS_BSP_PROTOCOL_PROTOCOL_H

#include "main.h"
#include "RingBuffer.h"

__attribute__((packed)) typedef struct frame_struct {
    uint32_t head;       // 帧头
    uint32_t seq;        // 帧序号
    uint32_t cmd;        // 命令字
    uint32_t len;        // 数据长度（data长度）(单位:word,1word=4byte)
    uint32_t data_crc[]; // 柔性数组，数据区
} IAP_Frame_t;

#endif // !DRIVERS_BSP_PROTOCOL_PROTOCOL_H
