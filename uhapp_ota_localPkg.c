#include "uh_libc.h"
#include "uhsd_ota.h"
#include "uhapp_common.h"
#include "uhapp_ota_common.h"
#include "uhapp_ota_localPkg.h"
#include "uhapp_ota_app.h"
#include "uhapp_ota_epp.h"
#include "uhapp_data.h"

/**
 * @brief OTA 本地打包类型描述结构体
 */
typedef struct
{
    /**
     * @brief OTA本地打包类型下载状态
     */
    uhapp_ota_dl_status_e m_status;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief APP固件描述对象
     */
    uhsd_void *m_appCtx;

    /**
     * @brief 固件打包信息对象
     */
    uhsd_void *m_eppCtx;

    /**
     * @brief 固件打包信息对象
     */
    uhsd_u32 m_eppFlag; /* 0 -> 升级APP， 1 -> 升级EPP */

    /**
     * @brief OTA相关operation
     */
    uhapp_ota_op_t m_op;

} uhapp_ota_localPkg_t;

uhsd_void *uhapp_ota_localPkg_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op)
{
    uhapp_ota_localPkg_t *ret = UHOS_NULL;

    ret = uhos_libc_zalloc(sizeof(uhapp_ota_localPkg_t));
    if (UHOS_NULL == ret)
    {
        goto fini;
    }

    uhos_libc_memcpy(&ret->m_fwInfo, fw_info, sizeof(uhsd_ota_evt_firm_info_t));
    uhos_libc_memcpy(&ret->m_op, op, sizeof(uhapp_ota_op_t));
    ret->m_status = UHAPP_OTA_DL_STATUS_INIT;

#ifdef CONFIG_LINUX
    /* 对于wifibase SDK，本地打包默认只升级整机，后续具体的操作由设备APP来决定 */
    ret->m_eppFlag = 0;
#else
    /* 对于软硬一体模块，本地打包只能升级底板 */
    ret->m_eppFlag = 1;
#endif

fini:
    return ret;
}

uhsd_void uhapp_ota_localPkg_destroy(uhsd_void *localCtx)
{
    uhapp_ota_localPkg_t *local = (uhapp_ota_localPkg_t *)localCtx;

    if (UHOS_NULL != local)
    {
        if (UHOS_NULL != local->m_appCtx)
        {
            uhapp_ota_app_destroy(local->m_appCtx);
            local->m_appCtx = UHOS_NULL;
        }

        if (UHOS_NULL != local->m_eppCtx)
        {
            uhapp_ota_epp_destroy(local->m_eppCtx);
            local->m_eppCtx = UHOS_NULL;
        }
        uhos_libc_free(local);
    }
}

/*
 * 由于本地打包只包含一个文件，其文件格式有开发者定义，目前提供代码，根据flash的存储的
 * 的配置信息决定，要么升级设备APP，要么升级底板
 */
uhsd_s32 uhapp_ota_localPkg_start(void *localCtx)
{
    uhapp_ota_localPkg_t *local = (uhapp_ota_localPkg_t *)localCtx;
    uhsd_s32 ret = UHOS_FAILURE;

    if (local->m_eppFlag)
    {
        /* 升级设备EPP */
        local->m_eppCtx = uhapp_ota_epp_new(&local->m_fwInfo, &local->m_op);
        if (UHOS_NULL == local->m_eppCtx)
        {
            goto fini;
        }

        ret = uhapp_ota_epp_start(local->m_eppCtx, UHOS_NULL, 1, 1);
        if (UHOS_FAILURE == ret)
        {
            goto fini;
        }

        local->m_status = UHAPP_OTA_DL_STATUS_DL_EPP;
    }
    else
    {
        /* 升级设备APP */
        local->m_appCtx = uhapp_ota_app_new(&local->m_fwInfo, &local->m_op);
        if (UHOS_NULL == local->m_appCtx)
        {
            goto fini;
        }

        ret = uhapp_ota_app_start(local->m_appCtx, 0, local->m_fwInfo.total_len, UHOS_TRUE);
        if (UHOS_FAILURE == ret)
        {
            goto fini;
        }

        local->m_status = UHAPP_OTA_DL_STATUS_DL_APP;
    }

fini:
    return ret;
}

uhsd_s32 uhapp_ota_localPkg_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow)
{
#ifdef CONFIG_LINUX
    /* 对于wifibase sdk，demo 目前都允许升级 */
    UHOS_LOGD("query allow");
    *is_allow = 1;
    return 0;
#else
    /* 对于软硬一体模块，本地打包只能升级底板 */
    return uhapp_ota_epp_query_allow(ack_fn, is_allow);
#endif
}

uhsd_s32 uhapp_ota_localPkg_seg_write(uhsd_void *localCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhapp_ota_localPkg_t *local = (uhapp_ota_localPkg_t *)localCtx;
    uhsd_s32 ret = UHOS_FAILURE;

    if (UHAPP_OTA_DL_STATUS_DL_APP == local->m_status)
    {
        ret = uhapp_ota_app_seg_write(local->m_appCtx, offset, buf, len);
    }
    else if (UHAPP_OTA_DL_STATUS_DL_EPP == local->m_status)
    {
        ret = uhapp_ota_epp_seg_write(local->m_eppCtx, offset, buf, len);
    }

    return ret;
}

uhsd_s32 uhapp_ota_localPkg_seg_finish(uhsd_void *localCtx)
{
    uhapp_ota_localPkg_t *local = (uhapp_ota_localPkg_t *)localCtx;
    uhsd_s32 ret = UHOS_SUCCESS;

    UHAPP_LOG("OTA local finish %u", local->m_status);

    if (UHAPP_OTA_DL_STATUS_DL_APP == local->m_status)
    {
        ret = uhapp_ota_app_seg_finish(local->m_appCtx);
        if (UHOS_SUCCESS != ret)
        {
            goto fini;
        }

        if (UHOS_FALSE == uhapp_ota_app_is_complete(local->m_appCtx))
        {
            goto fini;
        }

        local->m_status = UHAPP_OTA_DL_STATUS_FINISH;

        /* 如果版本相同, 则不需要重启，直接停止升级 */
        if (UHOS_FALSE == uhapp_ota_app_is_need_reset(local->m_appCtx))
        {
            ret = UHAPP_OTA_ERR_NO_NEED_UPGRADE;
        }
    }
    else if (UHAPP_OTA_DL_STATUS_DL_EPP == local->m_status)
    {
        ret = uhapp_ota_epp_seg_finish(local->m_eppCtx);
    }

fini:
    return ret;
}