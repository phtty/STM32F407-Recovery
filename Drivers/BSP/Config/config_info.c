#include "config_info.h"
#include "crc.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <string.h>

/**
 * @brief 判断配置区是否为全空(0xFF)
 */
bool Is_Config_Empty(SysInfo_t *info)
{
    // 检查魔数和一部分关键内容是否为0xFF
    if (info->magic == 0xFFFFFFFF && info->config_crc == 0xFFFFFFFF) {
        return true;
    }
    return false;
}

void Init_Config_Info(SysInfo_t *info)
{
    info->magic      = CONFIG_MAGIC;
    info->update_sta = failed;

    memset(&(info->app_info), 0, sizeof(info->app_info));
    NetConfig_t net_info = {
        .ip   = {192, 168, 114, 200},
        .mask = {255, 255, 255, 0},
        .gw   = {192, 168, 114, 1},
        .port = 0x2538,
    };
    memcpy(&(info->net_cfg), &net_info, sizeof(NetConfig_t));

    EraseConfigInfo();
    WriteConfigInfo(info);
}

HAL_StatusTypeDef EraseConfigInfo(void)
{
    HAL_StatusTypeDef status               = HAL_ERROR;
    uint32_t SectorError                   = 0;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 电压范围2.7V~3.6V
    EraseInitStruct.Sector       = FLASH_SECTOR_1;        // 获取扇区号
    EraseInitStruct.NbSectors    = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

    if (EraseInitStruct.Sector != FLASH_SECTOR_11) // 防止地址越界
        status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

    HAL_FLASH_Lock();
    return status;
}

HAL_StatusTypeDef WriteConfigInfo(SysInfo_t *info)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    for (uint32_t i = 0; i < sizeof(SysInfo_t) / 4; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ADDR_CONFIG_SECTOR + i, ((uint32_t *)info)[i]);

        if (status != HAL_OK) {
            break; // 写入出错，退出
        }
    }

    HAL_FLASH_Lock();
    return status;
}
