#ifndef DRIVERS_BSP_CMD_H
#define DRIVERS_BSP_CMD_H

#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"

#define FLAG_FORCE_UPDATE      (uint32_t)(0x0000DEADU)
#define FIRMWARE_MAX_FRAME_NUM (768U)
#define FLASH_SECTOR_SIZE      (0x20000U)

#define FIRMWARE_MAXLEN        (FIRMWARE_MAX_FRAME_NUM * 256U)
#define BITMAP_SIZE            (FIRMWARE_MAX_FRAME_NUM / 8)

typedef enum {
    rtn_cmd01 = (uint32_t)0x0000b401U,
    rtn_cmd02 = (uint32_t)0x0000b402U,
    rtn_cmd03 = (uint32_t)0x0000b403U,
    rtn_cmd04 = (uint32_t)0x0000b404U,
    rtn_cmd05 = (uint32_t)0x0000b405U,
    rtn_cmd06 = (uint32_t)0x0000b406U,
    rtn_cmd07 = (uint32_t)0x0000b407U,
} rtn_cmd_t;

extern bool timeout_flag;
extern uint8_t timeout_cnt;
extern uint8_t bitmap[BITMAP_SIZE];
extern uint16_t frame_cnt;

typedef void (*pfcmd_Functions)(IAP_Frame_t *);
extern const pfcmd_Functions cmd_Functions[];

#endif // !DRIVERS_BSP_CMD_H