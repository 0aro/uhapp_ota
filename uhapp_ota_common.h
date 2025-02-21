/**
 *
 * @author zhoudong (zhoudong@haier.com)
 * @brief  ota内存通用数据结构及接口定义
 * @details
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 * @file uhapp_ota_common.h
 *
 */

#ifndef _UHAPP_OTA_COMMON_H_
#define _UHAPP_OTA_COMMON_H_

#include "uhsd_types.h"

/**
 * @brief OTA下载状态枚举定义
 */
typedef enum
{
    UHAPP_OTA_DL_STATUS_INIT = 0x00,
    UHAPP_OTA_DL_STATUS_DL_DESC,
    UHAPP_OTA_DL_STATUS_DL_APP,
    UHAPP_OTA_DL_STATUS_DL_EPP,
    UHAPP_OTA_DL_STATUS_FINISH
} uhapp_ota_dl_status_e;

#define UHAPP_OTA_SUBFW_TYPE_EPP 1

#define UHAPP_OTA_QUERY_SUCCESS (0)
#define UHAPP_OTA_QUERY_FAIL    (-1)
#define UHAPP_OTA_QUERY_WAIT    (1)

#define UHAPP_OTA_QUERY_RESULT_REJECT (0)
#define UHAPP_OTA_QUERY_RESULT_ALLOW  (1)

#define UHAPP_OTA_ERR_NO_NEED_UPGRADE (0xFFFF)

/**
 * @brief 查询是否允许OTA的结果回调函数
 * @details
 * @param is_allow 0 ---> 不允许OTA
 *                 1 ---> 允许OTA
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
typedef uhsd_s32 (*uhapp_ota_query_allow_ack_fn)(uhsd_u8 is_allow);

/**
 * @brief OTA停止接口定义
 * @details
 * @param err OTA停止的错误码
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
typedef uhsd_s32 (*uhapp_ota_stop_fn)(uhsd_s32 err);

/**
 * @brief OTA下载固件接口定义
 * @details
 * @param offset OTA固件偏移地址
 * @param len OTA固件固件下载长度
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
typedef uhsd_s32 (*uhapp_ota_dlFw_fn)(uhsd_u32 offset, uhsd_u32 len);

/**
 * @brief OTA下载状态上报接口定义
 * @details
 * @param status OTA下载状态，具体见@UHSD_OTA_UPGRADE_STATUS_E
 * @param is_complete 0 ->未下载完成， 1 -->下砸完成
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
typedef uhsd_s32 (*uhapp_ota_dl_statusRpt_fn)(uhsd_u8 status, uhsd_u8 is_complete);

/**
 * @brief OTA上行操作函数集结构定义
 */
typedef struct
{
    uhapp_ota_stop_fn stop_fn;
    uhapp_ota_dlFw_fn dl_fw_fn;
    uhapp_ota_dl_statusRpt_fn dl_status_rpt_fn;
} uhapp_ota_op_t;

/**
 * @brief OTA EPP接口初始化
 * @details 此接口实际调用时，需要根据支持EPP的宏，决定是否编译
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_epp_init(uhapp_ota_op_t *op);

#endif