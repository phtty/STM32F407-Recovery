#include "BSP_rtc.h"
#include "stm32f4xx_hal.h"

/**
  * @brief  将数据写入 RTC 备份域寄存器0
  * @param  data: 要写入的数据
  * @retval 返回值 1：成功，0：失败
  */
uint32_t RTC_Backup_Write(uint32_t data)
{
    // 1. 解锁备份域
    HAL_PWR_EnableBkUpAccess();

    // 2. 写入数据到备份寄存器
    RTC->BKP0R = data;  // 写入数据到 RTC_BKP0R

    // 3. 锁定备份域
    HAL_PWR_DisableBkUpAccess();

    return 1;  // 写入成功
}
