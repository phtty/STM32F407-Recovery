#ifndef DRIVERS_BSP_CASCADE_CASCADE_H
#define DRIVERS_BSP_CASCADE_CASCADE_H

#include "protocol.h"
#include <stdbool.h>

/** @brief 从卡状态缓存（主卡侧） */
typedef struct {
    bool valid;          /**< 是否读到过有效应答 */
    bool online;         /**< 本次查询是否收到应答 */
    uint8_t mode;        /**< 0=main app，1=Recovery，2=unknown */
    uint32_t size;       /**< 从卡 main app 字节长度 */
    uint32_t crc32;      /**< 从卡 main app CRC32 */
    uint32_t update_sta; /**< 0=updated，1=updating，2=failed，3=unknown */
    uint32_t tick;       /**< 最近一次刷新的系统节拍 */
} cascade_status_t;

void cascade_init(void);
void cascade_poll(void);
void cascade_handle_request(IAP_Frame_t *f);
void cascade_handle_reply(IAP_Frame_t *f);
const cascade_status_t *cascade_status(void);
bool cascade_is_master(void);
bool cascade_is_syncing(void);

/** @brief 主卡 CRC 通过后启动从卡同步（异步，由 cascade_poll 推进） */
void cascade_start_sync(uint32_t size, uint32_t crc);

/** @brief 把从卡缓存标成 unknown（新一次升级开始） */
void cascade_slave_mark_unknown(void);

/** @brief 让从卡进入 Recovery（内部 0x14），返回是否有应答 */
bool cascade_ensure_slave_recovery(void);

/** @brief 让从卡重启（内部 0x15），返回是否有应答 */
bool cascade_restart_slave(uint32_t delay_ms);

/** @brief 0x03 用：缓存超过 1s 且空闲时刷新一次 */
void cascade_refresh_if_stale(void);

#endif // !DRIVERS_BSP_CASCADE_CASCADE_H
