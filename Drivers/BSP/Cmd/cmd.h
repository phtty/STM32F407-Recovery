#ifndef DRIVERS_BSP_CMD_H
#define DRIVERS_BSP_CMD_H

#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"

typedef void (*pfcmd_Functions)(IAP_Frame_t *IAP_Data);
extern pfcmd_Functions cmd_Functions[];

#endif // !DRIVERS_BSP_CMD_H