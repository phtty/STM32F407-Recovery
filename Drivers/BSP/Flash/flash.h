#ifndef DRIVERS_BSP_FLASH_H
#define DRIVERS_BSP_FLASH_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

uint32_t Flash_Erase(uint32_t eraseAddr, uint32_t eraseSize);

#endif