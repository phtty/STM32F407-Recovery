#include "cmd.h"
#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"
#include "rtc.h"
#include "crc.h"
#include "iwdg.h"

#define U8_LEN(x)  ((x) * (sizeof(uint32_t)))
#define U32_LEN(y) ((y) / (sizeof(uint32_t)))

static void cmd_Test_00(IAP_Frame_t *IAP_Data);
static void cmd_ReportIp_01(IAP_Frame_t *IAP_Data);
static void cmd_ForceModifyIP_02(IAP_Frame_t *IAP_Data);
static void cmd_ReportFirmwareStatus_03(IAP_Frame_t *IAP_Data);
static void cmd_PrepareUpgrade_04(IAP_Frame_t *IAP_Data);
static void cmd_SendUpgradePackage_05(IAP_Frame_t *IAP_Data);
static void cmd_EnterRecoveryMode_06(IAP_Frame_t *IAP_Data);
static void cmd_Restart_07(IAP_Frame_t *IAP_Data);
static HAL_StatusTypeDef write_fw(uint32_t addr_offset, uint32_t *data, uint16_t word_len);

const pfcmd_Functions cmd_Functions[] = {
    cmd_Test_00,
    cmd_ReportIp_01,
    cmd_ForceModifyIP_02,
    cmd_ReportFirmwareStatus_03,
    cmd_PrepareUpgrade_04,
    cmd_SendUpgradePackage_05,
    cmd_EnterRecoveryMode_06,
    cmd_Restart_07,
};

/**
 * @brief  内部函数，构造返回数据包并发送
 *
 * @param  ReSeq: 帧序号
 * @param  ReCmd: 返回命令
 * @param  ReLen: 数据长度(单位: uint32_t)
 * @param  ReData: 数据
 */
static void cmd_SendReData(uint32_t ReSeq, uint32_t ReCmd, uint32_t ReLen, uint32_t *ReData)
{
    uint32_t ReBuff[FRAME_MAX_LEN] = {0};

    IAP_Frame_t *pIAP_ReTmp = (IAP_Frame_t *)&(ReBuff);
    pIAP_ReTmp->head        = 0x5A5A5A5A;
    pIAP_ReTmp->seq         = ReSeq;
    pIAP_ReTmp->cmd         = ReCmd;
    pIAP_ReTmp->len         = ReLen;

    if (ReLen != 0) // 数据载荷
        memcpy(pIAP_ReTmp->data_crc, ReData, U8_LEN(ReLen));

    // crc校验值
    pIAP_ReTmp->data_crc[ReLen] = HAL_CRC_Calculate(&hcrc, (uint32_t *)pIAP_ReTmp, U32_LEN((sizeof(IAP_Frame_t) + U8_LEN(ReLen))));

    if (ReCmd == rtn_cmd01 || ReCmd == rtn_cmd02)
        udp_send_data(udp_pcb, boardcast, (uint8_t *)pIAP_ReTmp, sizeof(IAP_Frame_t) + U8_LEN(ReLen) + sizeof(uint32_t));
    else
        udp_send_data(udp_pcb, unicast, (uint8_t *)pIAP_ReTmp, sizeof(IAP_Frame_t) + U8_LEN(ReLen) + sizeof(uint32_t));
}

/**
 * @brief  测试
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_Test_00(IAP_Frame_t *IAP_Data)
{
}

/**
 * @brief  报告IP地址
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_ReportIp_01(IAP_Frame_t *IAP_Data)
{
    SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    SysInfo_t config_info = {0};
    memcpy(&config_info, pConfig, sizeof(SysInfo_t));

    uint32_t ReData[4] = {0};
    ReData[0]          = config_info.net_cfg.ip[0] << 24 | config_info.net_cfg.ip[1] << 16 | config_info.net_cfg.ip[2] << 8 | config_info.net_cfg.ip[3];
    ReData[1]          = config_info.net_cfg.mask[0] << 24 | config_info.net_cfg.mask[1] << 16 | config_info.net_cfg.mask[2] << 8 | config_info.net_cfg.mask[3];
    ReData[2]          = config_info.net_cfg.gw[0] << 24 | config_info.net_cfg.gw[1] << 16 | config_info.net_cfg.gw[2] << 8 | config_info.net_cfg.gw[3];
    ReData[3]          = config_info.net_cfg.port;

    cmd_SendReData(IAP_Data->seq, rtn_cmd01, U32_LEN(sizeof(ReData)), ReData);
}

/**
 * @brief  强制修改IP地址
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_ForceModifyIP_02(IAP_Frame_t *IAP_Data)
{
    SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    SysInfo_t config_info = {0};
    memcpy(&config_info, pConfig, sizeof(SysInfo_t));

    uint32_t TmpData[4] = {0};
    memcpy(TmpData, IAP_Data->data_crc, sizeof(TmpData));

    NetConfig_t net_info = {0};
    net_info.ip[0]       = (uint8_t)(TmpData[0] >> 24);
    net_info.ip[1]       = (uint8_t)(TmpData[0] >> 16);
    net_info.ip[2]       = (uint8_t)(TmpData[0] >> 8);
    net_info.ip[3]       = (uint8_t)(TmpData[0]);
    net_info.mask[0]     = (uint8_t)(TmpData[1] >> 24);
    net_info.mask[1]     = (uint8_t)(TmpData[1] >> 16);
    net_info.mask[2]     = (uint8_t)(TmpData[1] >> 8);
    net_info.mask[3]     = (uint8_t)(TmpData[1]);
    net_info.gw[0]       = (uint8_t)(TmpData[2] >> 24);
    net_info.gw[1]       = (uint8_t)(TmpData[2] >> 16);
    net_info.gw[2]       = (uint8_t)(TmpData[2] >> 8);
    net_info.gw[3]       = (uint8_t)(TmpData[2]);
    net_info.port        = TmpData[3];

    config_info.net_cfg = net_info;

    // 写入配置信息
    Edit_Config_Info(&config_info);

    cmd_SendReData(IAP_Data->seq, rtn_cmd02, 0, NULL);
}

/**
 * @brief 报告固件状态
 *
 * @param IAP_Data IAP数据包
 */
static void cmd_ReportFirmwareStatus_03(IAP_Frame_t *IAP_Data)
{
    SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    SysInfo_t config_info = {0};
    memcpy(&config_info, pConfig, sizeof(SysInfo_t));

    uint32_t ReData[11] = {0};
    ReData[0]           = config_info.app_info.size;                                        // 固件大小
    ReData[1]           = config_info.app_info.crc32;                                       // 固件CRC32
    memcpy(ReData + 2, config_info.app_info.version, sizeof(config_info.app_info.version)); // 固件版本信息
    ReData[10] = config_info.update_sta;

    cmd_SendReData(IAP_Data->seq, rtn_cmd03, U32_LEN(sizeof(ReData)), ReData);
}

/**
 * @brief 准备升级
 *
 * @param IAP_Data IAP数据包
 */
static void cmd_PrepareUpgrade_04(IAP_Frame_t *IAP_Data)
{
    uint32_t fw_size = IAP_Data->data_crc[0];
    if (fw_size > FIRMWARE_MAXLEN)
        return;

    // 解锁 Flash 控制寄存器
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 开始擦除
    uint32_t erase_sta                     = 0;
    FLASH_EraseInitTypeDef eraseInitStruct = {
        .TypeErase    = FLASH_TYPEERASE_SECTORS,
        .Sector       = 6,
        .NbSectors    = (fw_size * 4 + FLASH_SECTOR_SIZE - 1) / FLASH_SECTOR_SIZE,
        .VoltageRange = FLASH_VOLTAGE_RANGE_3,
    };

    do {
        uint32_t SectorError = 0;
        if (HAL_FLASHEx_Erase(&eraseInitStruct, &SectorError) != HAL_OK) {
            HAL_FLASH_Lock();
            erase_sta = 1;
            break;
        }

        const SysInfo_t *pConfig = (SysInfo_t *)ADDR_CONFIG_SECTOR;
        SysInfo_t config_info    = {0};
        memcpy(&config_info, pConfig, sizeof(SysInfo_t));
        config_info.app_info.size = fw_size * 4;
        config_info.update_sta    = updating;
        Edit_Config_Info(&config_info);
    } while (false);

    HAL_FLASH_Lock(); // 锁定 Flash 控制寄存器

    cmd_SendReData(IAP_Data->seq, rtn_cmd04, U32_LEN(sizeof(uint32_t)), &erase_sta);
}

/**
 * @brief  发送升级包
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_SendUpgradePackage_05(IAP_Frame_t *IAP_Data)
{
    static uint8_t bitmap[BITMAP_SIZE] = {0}, frame_cnt = 0;
    uint16_t frame_seq       = IAP_Data->seq - 1;
    const SysInfo_t *pConfig = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    if (pConfig->app_info.size == 0) // config_info中的app信息大小不能为0
        return;

    uint32_t max_len = (pConfig->app_info.size / 4 + 255) / 256;
    if (frame_seq >= max_len) // 包序号不能超过这个app的最大包序号
        return;

    if (write_fw(frame_seq * 1024, IAP_Data->data_crc, IAP_Data->len))
        return; // 这一帧写入出错

    // 写入完成，在bitmap中标记
    BITBAND_SRAM(&bitmap[frame_seq / 8], frame_seq % 8) = 1;
    frame_cnt++;

    if (frame_cnt <= max_len - 1)
        return; // 不是最后一帧，则不需要回复重传内容
    frame_cnt = 0;

    // 遍历bitmap以统计所有缺失的帧
    uint32_t miss_frame[FIRMWARE_MAX_FRAME_NUM] = {0};
    uint16_t miss_cnt                           = 0;
    for (uint16_t byte_cnt = 0; byte_cnt < max_len / 8; byte_cnt++) {
        for (uint8_t bit_cnt = 0; bit_cnt < 8; bit_cnt++) {
            if (!BITBAND_SRAM(&bitmap[byte_cnt], bit_cnt)) {
                miss_frame[miss_cnt] = byte_cnt * 8 + bit_cnt;
                miss_cnt++;
            }
        }
    }

    if (miss_cnt == 0) {
        SysInfo_t config_info = {0};
        memcpy(&config_info, pConfig, sizeof(SysInfo_t));

        // 修改升级状态机并保存整个固件的crc校验值
        config_info.app_info.crc32 = HAL_CRC_Calculate(&hcrc, (uint32_t *)ADDR_MAIN_APP, (pConfig->app_info.size + 3) / 4);
        config_info.update_sta     = updated;
        Edit_Config_Info(&config_info);
    }

    // 发送所有缺失的帧要求重传
    cmd_SendReData(0, rtn_cmd05, miss_cnt, miss_frame);
}

/**
 * @brief  进入恢复模式
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_EnterRecoveryMode_06(IAP_Frame_t *IAP_Data)
{
    // 已经在恢复模式，不需要再进入
    // HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, FLAG_FORCE_UPDATE);

    cmd_SendReData(IAP_Data->seq, rtn_cmd06, 0, NULL);
}

/**
 * @brief  重新启动
 *
 * @param  IAP_Data: IAP数据包
 */
static void cmd_Restart_07(IAP_Frame_t *IAP_Data)
{
    cmd_SendReData(IAP_Data->seq, rtn_cmd07, 0, NULL);
    HAL_IWDG_Refresh(&hiwdg);

    NVIC_SystemReset();
}

/**
 * @brief 写入固件到flash(单帧)
 *
 * @param addr_offset 通过包序号计算的flash地址偏移
 * @param word_len 写入数据的长度
 * @return HAL_StatusTypeDef
 */
static HAL_StatusTypeDef write_fw(uint32_t addr_offset, uint32_t *data, uint16_t word_len)
{
    HAL_StatusTypeDef status = HAL_ERROR;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    // 按word写入
    for (uint32_t i = 0; i < word_len; i++) {
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ADDR_MAIN_APP + addr_offset + i * 4, data[i]);

        if (status != HAL_OK) {
            break; // 写入出错，退出
        }
    }

    HAL_FLASH_Lock();
    return status;
}
