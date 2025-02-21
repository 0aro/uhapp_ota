/*
 * @Author: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install git &
 * please set dead value or install git
 * @Date: 2023-04-21 11:23:10
 * @LastEditors: error: error: git config user.name & please set dead value or install git && error: git config user.email & please set dead value or install
 * git & please set dead value or install git
 * @LastEditTime: 2023-04-26 14:22:34
 * @FilePath: /project_release/application/sdr_demo/source/ota/uhapp_ota_app.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/**
 *
 * @author zhoudong (zhoudong@haier.com)
 * @brief  OTA设备APP升级接口定义
 * @details
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 * @file uhapp_ota_app.h
 *
 */

#ifndef _UHAPP_OTA_APP_H_
#define _UHAPP_OTA_APP_H_

#include "uhsd_types.h"
#include "uhapp_ota_common.h"

/**
 * @brief OTA设备APP升级对象创建
 * @details
 * @param   op            OTA操作函数
 * @return 返回非空 ---> 创建成功
 *         返回空值 ---> 创建失败
 */
uhsd_void *uhapp_ota_app_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op);

/**
 * @brief OTA设备APP升级对象销毁
 * @details
 * @param appCtx       设备APP对象
 */
uhsd_void uhapp_ota_app_destroy(uhsd_void *appCtx);

/**
 * @brief OTA设备APP升级对象销毁
 * @details
 * @param appCtx       设备APP升级对象
 * @param appOff       设备APP相对于整机固件的偏移位置
 * @param appLen       设备APP固件的长度
 */
uhsd_s32 uhapp_ota_app_start(void *appCtx, uhsd_u32 appOff, uhsd_u32 appLen, uhsd_bool is_only_app);

/**
 * @brief 查询OTA是否允许升级
 * @details
 * @param ack_fn      查询结果回调函数
 * @param is_allow    是否允许升级   0 不允许升级/1 允许升级
 * @return 返回0 ---> 接口调用成功，实际结果见is_allow
 *         返回1 ---> 接口调用成功，实际结果见ack_fn异步回调
 *         返回其他值 --->  接口调用失败
 */
uhsd_s32 uhapp_ota_app_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow);

/**
 * @brief OTA设备APP升级分段固件写接口
 * @details
 * @param appCtx   设备APP升级对象
 * @param offset   分段固件偏移地址
 * @param buf      分段固件税局
 * @param len      分段固件长度
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_app_seg_write(uhsd_void *appCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len);

/**
 * @brief OTA设备APP升级分段固件完成
 * @details
 * @param appCtx 设备APP升级对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_app_seg_finish(uhsd_void *appCtx);

/**
 * @brief 检查APP是否已经下载完成
 * @details
 * @param appCtx 设备APP升级对象
 * @return 返回true ---> 成功
 *         返回false--->  失败
 */
uhsd_bool uhapp_ota_app_is_complete(uhsd_void *appCtx);

/**
 * @brief 检查APP是否需要重启
 * @details
 * @param appCtx 设备APP升级对象
 * @return 返回true ---> 成功
 *         返回false--->  失败
 */
uhsd_bool uhapp_ota_app_is_need_reset(uhsd_void *appCtx);

/**
 * @brief  设置固件哈希值
 * @details
 * @param appCtx 设备APP升级对象
 * @param digest 固件摘要信息
 * @return 返回true ---> 成功
 *         返回false--->  失败
 */
uhsd_s32 uhapp_ota_app_set_digest(uhsd_void *appCtx, uhsd_u8 digest[32]);

#endif