#ifndef LWIP_APP_UDP_CONN_H
#define LWIP_APP_UDP_CONN_H

#include "lwip/opt.h"
#include "lwip/api.h"
#include "lwip/udp.h"
#include <stdbool.h>

typedef enum {
    unicast,
    boardcast,
} udp_sendmode_t;

extern struct udp_pcb *udp_pcb;
extern bool udp_rx_flag;

// 定义默认端口
#define UDP_DEFAULT_PORT (10011U)

void udp_conn_init(void);
void udp_send_data(struct udp_pcb *pcb, udp_sendmode_t mode, uint8_t *data, uint16_t len);

#endif // !LWIP_APP_UDP_CONN_H