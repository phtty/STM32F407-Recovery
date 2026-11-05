#ifndef RECOVERY_BOARD_CONFIG_H
#define RECOVERY_BOARD_CONFIG_H

/**
 * @file    board_config.h
 * @brief   Recovery 的板级开关（5006048 / 3833024）
 *
 * Recovery 的 CubeMX 初始化是从 3833024 那一版延续下来的，两块板**不是**同一份
 * 引脚分布。已核对（对照 Project_STD/boards/<board>/Core/Inc/main.h）：
 *
 *   项目            5006048                        3833024
 *   --------------  -----------------------------  -----------------------------
 *   RS485           USART1 PA9/PA10，RE PA8        同左
 *   LED             PD9                            同左
 *   ETH RMII        PC1/4/5、PA1/2/7、PG11/13/14   同左
 *   RS232           **没有**                        USART3 PB10/PB11、USART6 PC6/PC7
 *   PB10/PB11        HUB75 数据 CH53/CH52           RS232 TX/RX
 *
 * 所以 5006048 上**绝不能**调用 MX_USART3_UART_Init()：它会把 PB10/PB11 配成
 * USART3 的 AF，直接抢走 HUB75 的两根数据线。USART6 的 PC6/PC7 在 5006048 上
 * 未使用，但同样没有保留的必要，一起关掉。
 *
 * 构建时用 -DRECOVERY_BOARD=5006048 或 3833024 选择；不定义时默认当前产品 5006048。
 */

#ifndef RECOVERY_BOARD
#define RECOVERY_BOARD 5006048
#endif

#if RECOVERY_BOARD == 5006048
#define RECOVERY_BOARD_NAME "5006048"
#define RECOVERY_HAS_RS232  0
#define RECOVERY_IAP_HW_ID  0x05006048U
#define RECOVERY_IAP_CAPS   0x00000001U /* bit0：支持从卡 */
#elif RECOVERY_BOARD == 3833024
#define RECOVERY_BOARD_NAME "3833024"
#define RECOVERY_HAS_RS232  1
#define RECOVERY_IAP_HW_ID  0x03833024U
#define RECOVERY_IAP_CAPS   0x00000000U /* 单卡，无从卡；Recovery 也没有工厂测试 */
#else
#error "RECOVERY_BOARD 只支持 5006048 / 3833024"
#endif

#endif // !RECOVERY_BOARD_CONFIG_H
