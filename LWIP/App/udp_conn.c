#include "udp_conn.h"

#include "lwip/err.h"
#include "lwip/pbuf.h"
#include "RingBuff.h"
#include <string.h>
#include <stdbool.h>

// 声明一个UDP控制块指针
struct udp_pcb *udp_pcb;
bool udp_rx_flag     = false;
uint16_t remote_port = UDP_DEFAULT_PORT;

/**
 * @brief UDP接收回调函数
 * @param arg 用户自定义参数（在udp_recv时传入）
 * @param pcb 当前的UDP控制块
 * @param p 接收到的数据包指针，注意用完必须释放
 * @param addr 发送方的IP地址
 * @param port 发送方的端口号
 */
static void udp_receive_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
    // 检查数据是否有效
    if (p != NULL) {
        // 获取数据指针和长度
        uint8_t *recv_data = (uint8_t *)p->payload;
        uint16_t recv_len  = p->len;

        udp_connect(pcb, addr, port);
        remote_port = port;
        udp_rx_flag = true;

        BSP_RB_PutByte_Bulk(&ringbuf, recv_data, recv_len);

        // 释放pbuf缓冲区，否则会导致内存泄漏
        pbuf_free(p);
    }
}

/**
 * @brief 初始化UDP连接
 */
void udp_conn_init(void)
{
    err_t err;

    // 创建一个新的UDP PCB
    udp_pcb = udp_new();
    if (udp_pcb == NULL) { // 创建失败
        return;
    }

    // 绑定到本地端口
    err = udp_bind(udp_pcb, IP_ADDR_ANY, UDP_DEFAULT_PORT);
    if (err != ERR_OK) { // 绑定失败
        udp_remove(udp_pcb);
        return;
    }

    // 开启广播支持
    ip_set_option(udp_pcb, SOF_BROADCAST);

    // 设置接收回调函数
    udp_recv(udp_pcb, udp_receive_callback, NULL);
}

/**
 * @brief udp发送数据
 *
 * @param pcb udp协议控制块
 * @param mode 广播或单播
 * @param data 待发送的数据
 * @param len 发送数据的长度
 */
void udp_send_data(struct udp_pcb *pcb, udp_sendmode_t mode, uint8_t *data, uint16_t len)
{
    // 分配pbuf，PBUF_TRANSPORT 表示预留传输层(UDP)头部空间，LwIP会自动处理
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) { // 内存分配失败
        return;
    }

    // 将数据装在到pbuf中
    memcpy(p->payload, data, len);

    // 发送数据
    if (mode == unicast) { // 单播发送
        udp_send(pcb, p);

    } else if (mode == boardcast) { // 广播发送
        udp_sendto(pcb, p, IP_ADDR_BROADCAST, remote_port);
    }

    // 释放发送用的pbuf
    pbuf_free(p);
}
