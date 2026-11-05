#ifndef DRIVERS_BSP_CMD_H
#define DRIVERS_BSP_CMD_H

#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"
#include "board_config.h"

#define FLAG_FORCE_UPDATE (uint32_t)(0x0000DEADU)
#define FLASH_SECTOR_SIZE (0x20000U)

/* 板级能力：默认取 board_config.h；EIDE 两套 target 也可以直接 -DIAP_HW_ID/-DIAP_CAPS 覆盖 */
#define IAP_CAP_SLAVE   (0x00000001U)
#define IAP_CAP_FACTORY (0x00000002U)

#ifndef IAP_CAPS
#define IAP_CAPS RECOVERY_IAP_CAPS
#endif
#ifndef IAP_HW_ID
#define IAP_HW_ID RECOVERY_IAP_HW_ID
#endif

/* 上位机命令表长度：0x00~0x0C，0x08~0x0B 保留 */
#define CMD_HOST_COUNT (13U)

typedef enum {
    rtn_cmd00 = (uint32_t)0x0000b400U,
    rtn_cmd01 = (uint32_t)0x0000b401U,
    rtn_cmd02 = (uint32_t)0x0000b402U,
    rtn_cmd03 = (uint32_t)0x0000b403U,
    rtn_cmd04 = (uint32_t)0x0000b404U,
    rtn_cmd05 = (uint32_t)0x0000b405U,
    rtn_cmd06 = (uint32_t)0x0000b406U,
    rtn_cmd07 = (uint32_t)0x0000b407U,
    rtn_cmd0C = (uint32_t)0x0000b40CU,
} rtn_cmd_t;

typedef void (*pfcmd_Functions)(IAP_Frame_t *);
extern const pfcmd_Functions cmd_Functions[];

#endif // !DRIVERS_BSP_CMD_H
