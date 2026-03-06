#include "cmd.h"
#include "protocol.h"
#include "udp_conn.h"
#include "lwip.h"
#include "config_info.h"
#include "flash.h"
#include "BSP_rtc.h"

#define U8_LEN(x)  ((x) * (sizeof(uint32_t)))
#define U32_LEN(y) ((y) / (sizeof(uint32_t)))

extern CRC_HandleTypeDef hcrc;

/*
 * @brief  内部函数，构造返回数据包并发送
 * @param  ReSeq: 帧序号
 * @param  ReCmd: 返回命令
 * @param  ReLen: 数据长度(单位: uint32_t)
 * @param  ReData: 数据
 */
static void cmd_SendReData(uint32_t ReSeq, uint32_t ReCmd, uint32_t ReLen, uint32_t *ReData)
{
    uint8_t ReBuff[16 + 256 * 4 + 4] = {0};

    IAP_Frame_t *pIAP_ReTmp = (IAP_Frame_t *)&(ReBuff);
    pIAP_ReTmp->head        = 0X5A5A5A5A;
    pIAP_ReTmp->seq         = ReSeq;
    pIAP_ReTmp->cmd         = ReCmd;
    pIAP_ReTmp->len         = ReLen;

    if (ReLen != 0)
        memcpy(pIAP_ReTmp->data_crc, ReData, U8_LEN(ReLen));
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)pIAP_ReTmp, U32_LEN((sizeof(IAP_Frame_t) + U8_LEN(ReLen))));
    memcpy(&(pIAP_ReTmp->data_crc[ReLen]), &crc, sizeof(uint32_t));
    // 广播
    udp_send_data(udp_pcb, boardcast, (uint8_t *)pIAP_ReTmp, sizeof(IAP_Frame_t) + U8_LEN(ReLen) + sizeof(uint32_t));
}

/*
 * @brief  测试
 * @param  IAP_Data: IAP数据包
 */
void cmd_Test_00(IAP_Frame_t *IAP_Data)
{
}

/*
 * @brief  报告IP地址
 * @param  IAP_Data: IAP数据包
 */
void cmd_ReportIp_01(IAP_Frame_t *IAP_Data)
{
    SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    SysInfo_t config_info = {0};
    memcpy(&config_info, pConfig, sizeof(SysInfo_t));

    uint32_t ReCmd = 0x0000b401;

    uint32_t ReData[4] = {0};
    ReData[0]          = config_info.net_cfg.ip[0] << 24 | config_info.net_cfg.ip[1] << 16 | config_info.net_cfg.ip[2] << 8 | config_info.net_cfg.ip[3];
    ReData[1]          = config_info.net_cfg.mask[0] << 24 | config_info.net_cfg.mask[1] << 16 | config_info.net_cfg.mask[2] << 8 | config_info.net_cfg.mask[3];
    ReData[2]          = config_info.net_cfg.gw[0] << 24 | config_info.net_cfg.gw[1] << 16 | config_info.net_cfg.gw[2] << 8 | config_info.net_cfg.gw[3];
    ReData[3]          = config_info.net_cfg.port;

    cmd_SendReData(IAP_Data->seq, ReCmd, U32_LEN(sizeof(ReData)), ReData);
}

/*
 * @brief  强制修改IP地址
 * @param  IAP_Data: IAP数据包
 */
void cmd_ForceModifyIP_02(IAP_Frame_t *IAP_Data)
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

    uint32_t ReCmd = 0x0000b402;
    cmd_SendReData(IAP_Data->seq, ReCmd, 0, NULL);
}

/*
 * @brief  报告固件状态
 * @param  IAP_Data: IAP数据包
 */
void cmd_ReportFirmwareStatus_03(IAP_Frame_t *IAP_Data)
{
    SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
    SysInfo_t config_info = {0};
    memcpy(&config_info, pConfig, sizeof(SysInfo_t));

    uint32_t ReCmd = 0x0000b403;

    uint32_t ReData[11] = {0};
    ReData[0]           = config_info.app_info.size;                                        // 固件大小
    ReData[1]           = config_info.app_info.crc32;                                       // 固件CRC32
    memcpy(ReData + 2, config_info.app_info.version, sizeof(config_info.app_info.version)); // 固件版本信息
    ReData[10] = config_info.update_sta;

    cmd_SendReData(IAP_Data->seq, ReCmd, U32_LEN(sizeof(ReData)), ReData);
}

/*
 * @brief  准备升级（擦除固件）
 * @param  IAP_Data: IAP数据包
 */
void cmd_PrepareUpgrade_04(IAP_Frame_t *IAP_Data)
{
    uint32_t FirmwareSize = 0;
    memcpy(&FirmwareSize, IAP_Data->data_crc, sizeof(FirmwareSize));

    uint32_t ReCmd = 0x0000b404;

    uint32_t EraseSta = 0;
    EraseSta          = Flash_Erase(ADDR_MAIN_APP, FirmwareSize);
    if (EraseSta) {
        SysInfo_t *pConfig    = (SysInfo_t *)ADDR_CONFIG_SECTOR;
        SysInfo_t config_info = {0};
        memcpy(&config_info, pConfig, sizeof(SysInfo_t));
        config_info.app_info.size = FirmwareSize;
        Edit_Config_Info(&config_info);
    }
    cmd_SendReData(IAP_Data->seq, ReCmd, U32_LEN(sizeof(uint32_t)), &EraseSta);
}

/*
 * @brief  发送升级包
 * @param  IAP_Data: IAP数据包
 */
void cmd_SendUpgradePackage_05(IAP_Frame_t *IAP_Data)
{
    static uint32_t Packet_Number = 0;
}

/*
 * @brief  进入恢复模式
 * @param  IAP_Data: IAP数据包
 */
void cmd_EnterRecoveryMode_06(IAP_Frame_t *IAP_Data)
{
    uint32_t Rtc_Data = 0x0000DEAD;
    RTC_Backup_Write(Rtc_Data);

    uint32_t ReCmd = 0x0000b406;
    cmd_SendReData(IAP_Data->seq, ReCmd, 0, NULL);

    HAL_Delay(1000);
    NVIC_SystemReset();
}

/*
 * @brief  重新启动
 * @param  IAP_Data: IAP数据包
 */
void cmd_Restart_07(IAP_Frame_t *IAP_Data)
{
    uint32_t ReCmd = 0x0000b407;
    cmd_SendReData(IAP_Data->seq, ReCmd, 0, NULL);

    HAL_Delay(1000);
    NVIC_SystemReset();
}

pfcmd_Functions cmd_Functions[] = {
    cmd_Test_00,
    cmd_ReportIp_01,
    cmd_ForceModifyIP_02,
    cmd_ReportFirmwareStatus_03,
    cmd_PrepareUpgrade_04,
    cmd_SendUpgradePackage_05,
    cmd_EnterRecoveryMode_06,
    cmd_Restart_07,
};