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

/* 命令码中间的协议方向字节 */
#define FRAME_DIR_HOST       (0x4BU) // 上位机→设备
#define FRAME_DIR_TO_SLAVE   (0x4CU) // 主卡→从卡（内部 485）
#define FRAME_DIR_FROM_SLAVE (0xBCU) // 从卡→主卡（内部 485）

typedef enum {
    iap_frame_rdy = 0, // 收到完整帧，可以解析
    iap_frame_wait,    // 帧不完整，等后续字节
    iap_frame_fake,    // 伪帧/脏数据，跳过 1 字节继续找帧头
} IAP_FrameSta_t;

/** @brief 帧方向 */
typedef enum {
    iap_dir_none = 0,
    iap_dir_host,       // 0x00004Bxx
    iap_dir_to_slave,   // 0x00014Cxx
    iap_dir_from_slave, // 0x0000BCxx
} IAP_CmdDir_t;

typedef struct iap_frame {
    uint32_t head;
    uint32_t seq;
    uint32_t cmd;
    uint32_t len;
    uint32_t data_crc[];
} IAP_Frame_t;

void handle_protocol(void);
IAP_CmdDir_t iap_cmd_dir(uint32_t cmd);
IAP_FrameSta_t check_frame_validity(const RingBuff_t *buff, uint32_t *total_len, uint32_t *cmd);

#endif // !DRIVERS_BSP_PROTOCOL_PROTOCOL_H
