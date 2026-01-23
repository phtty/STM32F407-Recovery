#ifndef DRIVERS_BSP_CONFIG_CONFIG_INFO_H
#define DRIVERS_BSP_CONFIG_CONFIG_INFO_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

// 地址与常量定义
#define ADDR_CONFIG_SECTOR 0x08004000
#define ADDR_RECOVERY_APP  0x08008000
#define ADDR_MAIN_APP      0x08040000

#define CONFIG_MAGIC       0x0d000721

// 网络配置
__attribute__((aligned(4))) typedef struct {
    uint8_t ip[4];
    uint8_t mask[4];
    uint8_t gw[4];
    uint32_t port;
} NetConfig_t;

// main app信息
__attribute__((aligned(4))) typedef struct {
    uint32_t size;  // Main App的字节长度
    uint32_t crc32; // Main App的CRC32值
    char version[32];
} FWInfo_t;

// 应该定义在0x08004000
__attribute__((aligned(4))) typedef struct {
    uint32_t magic;      // 魔数，判断配置区是否有效
    uint32_t update_sta; // 升级状态机
    FWInfo_t app_info;   // main_app状态
    NetConfig_t net_cfg; // 网络配置
    uint32_t config_crc; // 本结构体自身的校验
} SysInfo_t;

// 升级状态机
typedef enum {
    updated  = 0,
    updating = 1,
    failed   = 2,
} UpdateSta_t;

bool Is_Config_Empty(SysInfo_t *info);
void Init_Config_Info(SysInfo_t *info);
HAL_StatusTypeDef EraseConfigInfo(void);
HAL_StatusTypeDef WriteConfigInfo(SysInfo_t *info);

#endif // !DRIVERS_BSP_CONFIG_CONFIG_INFO_H