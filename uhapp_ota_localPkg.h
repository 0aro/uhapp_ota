/**
 *
 * @author zhoudong (zhoudong@haier.com)
 * @brief  OTA本地打包固件相关接口
 * @details
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 * @file uhapp_ota_localPkg.h
 *
 */

#ifndef _UHAPP_OTA_LOCAL_PKG_H_
#define _UHAPP_OTA_LOCAL_PKG_H_

#include "uhsd_types.h"
#include "uhapp_ota_common.h"

/**
 * @brief OTA本地打包对象创建
 * @details
 * @param fw_info       固件信息
 *        op            OTA操作函数
 * @return 返回非空 ---> 创建成功
 *         返回空值 ---> 创建失败
 */
uhsd_void *uhapp_ota_localPkg_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op);

/**
 * @brief OTA本地打包对象销毁
 * @details
 * @param localCtx       本地固件对象
 */
uhsd_void uhapp_ota_localPkg_destroy(uhsd_void *localCtx);

/**
 * @brief OTA开始下载本地固件
 * @details
 * @param localcCtx  本地打包固件升级对象
 * @return 返回0 --->  成功
 *        返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_localPkg_start(void *localCtx);

/**
 * @brief OTA本地打包固件查询是否允许升级接口
 * @details
 * @param localCtx   本地固件对象
 * @param is_allow   是否允许升级   0 不允许升级/1 允许升级
 * @return 返回0 ---> 接口调用成功，实际结果见is_allow
 *         返回1 ---> 接口调用成功，实际结果见ack_fn异步回调
 *         返回其他值 --->  接口调用失败
 */
uhsd_s32 uhapp_ota_localPkg_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow);

/**
 * @brief OTA本地打包固件分段固件写接口
 * @details
 * @param localCtx    本地固件对象
 * @param offset      分段固件偏移地址
 * @param buf         分段固件税局
 * @param len         分段固件长度
 * @return 返回0 --->  成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_localPkg_seg_write(uhsd_void *localCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len);

/**
 * @brief OTA分段固件完成
 * @details
 * @param localCtx  本地固件对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_localPkg_seg_finish(uhsd_void *localCtx);

#endif