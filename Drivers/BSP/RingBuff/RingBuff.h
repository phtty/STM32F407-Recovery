#ifndef __RINGBUFFER_H__
#define __RINGBUFFER_H__

#include <string.h>
#include <stdint.h>

#define BUFFER_SIZE (1024 * 8) // 必须是2的幂

// 环形缓冲区数据结构
typedef struct RB_Struct {
    uint8_t data[BUFFER_SIZE];
    volatile uint32_t read_index;  // 读指针
    volatile uint32_t write_index; // 写指针
} RingBuff_t;

extern RingBuff_t ringbuf;

uint8_t BSP_RB_IsEmpty(const RingBuff_t *fifo);
uint8_t BSP_RB_IsFull(const RingBuff_t *fifo);
uint32_t BSP_RB_GetAvailable(const RingBuff_t *fifo);
uint32_t BSP_RB_GetFreeSpace(const RingBuff_t *fifo);
uint8_t BSP_RB_FreeBuff(RingBuff_t *fifo);
uint8_t BSP_RB_PutByte(RingBuff_t *fifo, uint8_t byte);
uint32_t BSP_RB_PutByte_Bulk(RingBuff_t *fifo, const uint8_t *data, uint32_t len);
uint8_t BSP_RB_GetByte(RingBuff_t *fifo, uint8_t *byte);
uint32_t BSP_RB_GetByte_Bulk(RingBuff_t *fifo, uint8_t *data, uint32_t len);
uint8_t BSP_RB_PeekByte(const RingBuff_t *fifo, uint32_t offset, uint8_t *byte);
uint32_t BSP_RB_PeekBlock(const RingBuff_t *fifo, uint32_t offset, uint8_t *dest, uint32_t len);
uint32_t BSP_RB_GetContiguousLength(const RingBuff_t *fifo, uint32_t offset);
uint32_t BSP_RB_SkipBytes(RingBuff_t *fifo, uint32_t len);

#endif // !__RINGBUFFER_H__