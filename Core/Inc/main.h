/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

// SRAM区地址位带映射
#define BITBAND_SRAM(address, bit) (*(volatile uint32_t *)(0x22000000 + ((uint32_t)(address) - 0x20000000) * 0x20 + (bit) * 0x04))
// 外设区地址位带映射
#define BITBAND_PERIPH(address, bit) (*(volatile uint32_t *)(0x42000000 + ((uint32_t)(address) - 0x40000000) * 0x20 + (bit) * 0x04))

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RS232_TX1_Pin       GPIO_PIN_10
#define RS232_TX1_GPIO_Port GPIOB
#define RS232_RX1_Pin       GPIO_PIN_11
#define RS232_RX1_GPIO_Port GPIOB
#define LED_Pin             GPIO_PIN_9
#define LED_GPIO_Port       GPIOD
#define RS232_TX2_Pin       GPIO_PIN_6
#define RS232_TX2_GPIO_Port GPIOC
#define RS232_RX2_Pin       GPIO_PIN_7
#define RS232_RX2_GPIO_Port GPIOC
#define RS485_RE_Pin        GPIO_PIN_8
#define RS485_RE_GPIO_Port  GPIOA
#define RS485_TX_Pin        GPIO_PIN_9
#define RS485_TX_GPIO_Port  GPIOA
#define RS485_RX_Pin        GPIO_PIN_10
#define RS485_RX_GPIO_Port  GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
