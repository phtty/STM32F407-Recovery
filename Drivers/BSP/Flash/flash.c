#include "flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define FLASH_SECTOR_SIZE 0x20000    // 每个扇区大小 128KB
#define FLASH_BASE_ADDR   0x08000000 // Flash 基地址
#define FLASH_SIZE        0x100000   // Flash 总大小，通常为 1MB

/**
 * @brief  Flash 擦除函数
 * @param  eraseAddr: 擦除起始地址
 * @param  eraseSize: 擦除的大小（单位为 uint32_t）
 * @retval 返回值 1：成功，0：失败
 */
uint32_t Flash_Erase(uint32_t eraseAddr, uint32_t eraseSize)
{
    // 1. 检查擦除地址和大小是否合法
    if (eraseAddr < FLASH_BASE_ADDR || eraseAddr >= (FLASH_BASE_ADDR + FLASH_SIZE)) {
        return 0; // 地址越界
    }

    if (eraseSize == 0 || (eraseAddr + eraseSize) > (FLASH_BASE_ADDR + FLASH_SIZE)) {
        return 0; // 擦除大小不合法或越界
    }

    // 2. 解锁 Flash 控制寄存器
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 3. 计算擦除的扇区范围
    uint32_t startSector = (eraseAddr - FLASH_BASE_ADDR) / FLASH_SECTOR_SIZE;
    uint32_t endSector   = (eraseAddr + eraseSize - 1 - FLASH_BASE_ADDR) / FLASH_SECTOR_SIZE;

    // 4. 擦除扇区
    FLASH_Erase_Sector(startSector, VOLTAGE_RANGE_3); // 设置擦除电压范围为3V

    FLASH_EraseInitTypeDef eraseInitStruct;
    eraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
    eraseInitStruct.Sector       = startSector;
    eraseInitStruct.NbSectors    = endSector - startSector + 1;
    eraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sectorError = 0;

    if (HAL_FLASHEx_Erase(&eraseInitStruct, &sectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return 0; // 擦除失败
    }

    // 5. 锁定 Flash 控制寄存器
    HAL_FLASH_Lock();

    return 1; // 擦除成功
}

/**
 * @brief  Flash 写入函数(写入前需要先擦除)
 * @param  writeAddr: 写入起始地址
 * @param  pData: 待写入数据的指针
 * @param  size: 待写入数据的大小（单位为 uint32_t）
 * @retval 返回值 1：成功，0：失败
 */
uint32_t Flash_Write(uint32_t writeAddr, uint32_t *pData, uint32_t size)
{
    // 1. 检查写入地址是否合法
    if (writeAddr < FLASH_BASE_ADDR || writeAddr >= (FLASH_BASE_ADDR + FLASH_SIZE)) {
        return 0; // 地址越界
    }

    // 2. 解锁 Flash 控制寄存器
    HAL_FLASH_Unlock();

    // 3. 写入数据
    for (uint32_t i = 0; i < size; i++) {
        // 等待直到 Flash 准备好写入
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, writeAddr + i * 4, pData[i]) != HAL_OK) {
            HAL_FLASH_Lock(); // 写入失败，锁定 Flash
            return 0;         // 写入失败
        }
    }

    // 4. 锁定 Flash 控制寄存器
    HAL_FLASH_Lock();

    return 1; // 写入成功
}

/**
 * @brief  Flash 读取函数
 * @param  readAddr: 读取起始地址
 * @param  pData: 存储读取数据的指针
 * @param  size: 读取数据的大小（单位为 uint32_t）
 * @retval 返回值 1：成功，0：失败
 */
uint32_t Flash_Read(uint32_t readAddr, uint32_t *pData, uint32_t size)
{
    // 1. 检查读取地址是否合法
    if (readAddr < FLASH_BASE_ADDR || readAddr >= (FLASH_BASE_ADDR + FLASH_SIZE)) {
        return 0; // 地址越界
    }

    // 2. 读取 Flash 数据
    for (uint32_t i = 0; i < size; i++) {
        pData[i] = *(volatile uint32_t *)(readAddr + i * 4);
    }

    return 1; // 读取成功
}