#ifndef RECOVERY_BOARD_CONFIG_H
#define RECOVERY_BOARD_CONFIG_H

/**
 * @file    board_config.h
 * @brief   Recovery 的板级身份（5006048 / 3833024）
 *
 * Recovery 没有显示，也不需要 RS232：USART3/USART6 已经从本工程删掉，
 * 所以两块板的**引脚初始化现在是同一份**（只剩 USART1/RS485、ETH、LED、时基）。
 *
 * 仍保留这个头，是因为 0x00 要上报不同的 hw_id / caps：
 *   5006048：hw_id=0x05006048，caps.bit0=1（有从卡）
 *   3833024：hw_id=0x03833024，caps=0（单卡）
 *
 * 构建时用 -DRECOVERY_BOARD=5006048 或 3833024 选择；不定义时默认 5006048。
 * 如果以后要求"一个 Recovery 镜像同时认两块板"，需要给 hw_id 一个运行期来源
 * （配置区字段/板级 ID 脚），不能继续用编译期宏。
 */

#ifndef RECOVERY_BOARD
#define RECOVERY_BOARD 5006048
#endif

#if RECOVERY_BOARD == 5006048
#define RECOVERY_BOARD_NAME "5006048"
#define RECOVERY_IAP_HW_ID  0x05006048U
#define RECOVERY_IAP_CAPS   0x00000001U /* bit0：支持从卡 */
#elif RECOVERY_BOARD == 3833024
#define RECOVERY_BOARD_NAME "3833024"
#define RECOVERY_IAP_HW_ID  0x03833024U
#define RECOVERY_IAP_CAPS   0x00000000U /* 单卡，无从卡；Recovery 也没有工厂测试 */
#else
#error "RECOVERY_BOARD 只支持 5006048 / 3833024"
#endif

#endif // !RECOVERY_BOARD_CONFIG_H
