/**
 *
 * @author zhoudong (zhoudong@haier.com)
 * @brief  OTA固件描述信息接口定义
 * @details
 * @version 0.1
 * @date 2023-04-14
 *
 * @copyright Copyright (c) 2023
 * @file uhapp_ota_desc.h
 *
 */

#ifndef _UHAPP_OTA_DESC_H_
#define _UHAPP_OTA_DESC_H_

#include "uhsd_types.h"
#include "uhapp_ota_common.h"

/**
 * @brief OTA描述信息对象创建
 * @details
 * @param   op  OTA操作函数
 * @return 返回非空 ---> 创建成功
 *         返回空值 ---> 创建失败
 */
uhsd_void *uhapp_ota_desc_new(uhapp_ota_op_t *op);

/**
 * @brief OTA描述信息对象销毁
 * @details
 * @param descCtx 固件描述符对象
 */
uhsd_void uhapp_ota_desc_destroy(uhsd_void *descCtx);

/**
 * @brief OTA开始下载描述信息
 * @details
 * @param descCtx  固件描述符对象
 */
uhsd_s32 uhapp_ota_desc_start(void *descCtx);

/**
 * @brief OTA描述信息分段固件写接口
 * @details
 * @param descCtx  固件描述符对象
 * @param offset   分段固件偏移地址
 * @param buf      分段固件税局
 * @param len      分段固件长度
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_desc_seg_write(uhsd_void *descCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len);

/**
 * @brief OTA分段固件下载完成
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_desc_seg_finish(uhsd_void *descCtx);

/**
 * @brief 检查描述信息是否已经下载完成
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_bool uhapp_ota_desc_is_complete(uhsd_void *descCtx);

/**
 * @brief 检查描述信息是否含有EPP固件
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回true ---> 成功
 *         返回false --->  失败
 */
uhsd_bool uhapp_ota_desc_epp_check(void *descCtx);

/**
 * @brief 检查描述信息是否含有APP固件
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回true ---> 成功
 *         返回false --->  失败
 */
uhsd_bool uhapp_ota_desc_app_check(void *descCtx);

/**
 * @brief 获取设备APP固件信息
 * @details
 * @param descCtx 固件描述符对象
 * @param appOff  输出的APP固件偏移地址
 * @param appLen  输出的APP固件长度
 * @param digest  输出的固件摘要信息
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_s32 uhapp_ota_desc_appInfo_get(void *descCtx, uhsd_u32 *appOff, uhsd_u32 *appLen, uhsd_u8 digest[32]);

/**
 * @brief  获取描述信息数量
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_u32 uhapp_ota_desc_num_get(void *descCtx);

/**
 * @brief  获取底板描述信息数量
 * @details
 * @param descCtx 固件描述符对象
 * @return 返回0 ---> 成功
 *         返回其他值 --->  失败
 */
uhsd_u32 uhapp_ota_desc_eppNum_get(void *descCtx);

/**
 * @brief 检查描述信息是否已经下载完成
 * @details
 * @param descCtx 描述信息对象
 * @return 返回非空指针（实际类型是uhsd_ota_online_sub_firm_info_t *） ---> 成功
 *         返回空指针 --->  失败
 */
uhsd_void *uhapp_ota_desc_subDesc_get(void *descCtx);

#endif