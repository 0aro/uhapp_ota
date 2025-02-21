/**
 *
 * @author zhoudong (zhoudong@haier.com)
 * @brief  OTA EPP升级接口定义
 * @details
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 * @file uhepp_ota_epp.h
 *
 */

#ifndef _UHAPP_OTA_EPP_H_
#define _UHAPP_OTA_EPP_H_

#include "uhsd_types.h"
#include "uhapp_ota_common.h"

/**
 * @brief OTA EPP升级对象创建
 * @details
 * @param   op            OTA操作函数
 * @return 返回非空 ---> 创建成功
 *         返回空值 ---> 创建失败
 */
uhsd_void *uhapp_ota_epp_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op);

/**
 * @brief OTA EPP升级对象销毁
 * @details
 * @param eppCtx       设备APP对象
 */
uhsd_void uhapp_ota_epp_destroy(uhsd_void *eppCtx);

/**
 * @brief OTA EPP升级对象销毁
 * @details
 * @param eppCtx       设备APP升级对象
 * @param subDesc      在线打包子固件描述信息（结构体数组）
 * @param subNum       在线打包子固件数量
 * @param eppSum       在线打包EPP子固件数量
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_epp_start(uhsd_void *eppCtx, uhsd_ota_online_sub_firm_info_t *subDesc, uhsd_u32 subNum, uhsd_u32 eppNum);

/**
 * @brief 查询OTA是否允许升级
 * @details
 * @param ack_fn      查询结果回调函数
 * @param is_allow    是否允许升级   0 不允许升级/1 允许升级
 * @return 返回0 ---> 接口调用成功，实际结果见is_allow
 *         返回1 ---> 接口调用成功，实际结果见ack_fn异步回调
 *         返回其他值 --->  接口调用失败
 */
uhsd_s32 uhapp_ota_epp_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow);

/**
 * @brief OTA EPP升级分段固件写接口
 * @details
 * @param eppCtx   设备APP升级对象
 * @param offset   分段固件偏移地址
 * @param buf      分段固件税局
 * @param len      分段固件长度
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_epp_seg_write(uhsd_void *eppCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len);

/**
 * @brief OTA EPP升级分段固件完成
 * @details
 * @param eppCtx 设备APP升级对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_epp_seg_finish(uhsd_void *eppCtx);

/**
 * @brief 检查EPP升级是否已经完成
 * @details
 * @param eppCtx 设备APP升级对象
 * @return 返回true ---> 成功
 *         返回false --->  失败
 */
uhsd_bool uhapp_ota_epp_is_complete(uhsd_void *eppCtx);

#endif