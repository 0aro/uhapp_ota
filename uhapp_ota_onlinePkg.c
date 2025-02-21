
#include "uh_libc.h"
#include "uhapp_ota.h"
#include "uhsd_ota.h"
#include "uhapp_ota_common.h"
#include "uhapp_common.h"
#include "uhapp_ota_onlinePkg.h"
#include "uhapp_ota_desc.h"
#include "uhapp_ota_app.h"
#include "uhapp_ota_epp.h"

/**
 * @brief OTA在线打包类型描述结构体
 */
typedef struct
{
    /**
     * @brief OTA在线打包类型下载状态
     */
    uhapp_ota_dl_status_e m_status;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief 升级描述信息对象
     */
    uhsd_void *m_descInfo;

    /**
     * @brief dev固件描述对象
     */
    uhsd_void *m_appCtx;

    /**
     * @brief epp固件描述对象
     */
    uhsd_void *m_eppCtx;

    /**
     * @brief OTA相关operation
     */
    uhapp_ota_op_t m_op;
} uhapp_ota_onlinePkg_t;

uhsd_void *uhapp_ota_onlinePkg_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op)
{
    uhapp_ota_onlinePkg_t *ret = UHOS_NULL;

    ret = uhos_libc_zalloc(sizeof(uhapp_ota_onlinePkg_t));
    if (UHOS_NULL == ret)
    {
        goto fini;
    }

    uhos_libc_memcpy(&ret->m_fwInfo, fw_info, sizeof(uhsd_ota_evt_firm_info_t));
    uhos_libc_memcpy(&ret->m_op, op, sizeof(uhapp_ota_op_t));
    ret->m_status = UHAPP_OTA_DL_STATUS_INIT;

fini:
    return ret;
}

uhsd_void uhapp_ota_onlinePkg_destroy(uhsd_void *onlineCtx)
{
    uhapp_ota_onlinePkg_t *online = (uhapp_ota_onlinePkg_t *)onlineCtx;

    if (UHOS_NULL != online)
    {
        if (UHOS_NULL != online->m_descInfo)
        {
            uhapp_ota_desc_destroy(online->m_descInfo);
            online->m_descInfo = UHOS_NULL;
        }

        if (UHOS_NULL != online->m_appCtx)
        {
            uhapp_ota_app_destroy(online->m_appCtx);
            online->m_appCtx = UHOS_NULL;
        }

        if (UHOS_NULL != online->m_eppCtx)
        {
            uhapp_ota_epp_destroy(online->m_eppCtx);
            online->m_eppCtx = UHOS_NULL;
        }

        uhos_libc_free(online);
    }
}

uhsd_s32 uhapp_ota_onlinePkg_start(void *onlineCtx)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_onlinePkg_t *online = (uhapp_ota_onlinePkg_t *)onlineCtx;

    online->m_descInfo = uhapp_ota_desc_new(&online->m_op);
    if (UHOS_NULL == online->m_descInfo)
    {
        goto fini;
    }

    online->m_status = UHAPP_OTA_DL_STATUS_DL_DESC;

    ret = uhapp_ota_desc_start(online->m_descInfo);
    if (UHOS_FAILURE == ret)
    {
        goto fini;
    }

    return ret;

fini:
    if (UHOS_NULL != online->m_descInfo)
    {
        uhapp_ota_desc_destroy(online->m_descInfo);
        online->m_descInfo = UHOS_NULL;
    }
    return ret;
}

uhsd_s32 uhapp_ota_onlinePkg_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow)
{
    return uhapp_ota_epp_query_allow(ack_fn, is_allow);
}

uhsd_s32 uhapp_ota_onlinePkg_seg_write(uhsd_void *onlineCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhapp_ota_onlinePkg_t *online = (uhapp_ota_onlinePkg_t *)onlineCtx;
    uhsd_s32 ret = UHOS_FAILURE;

    UHAPP_LOG("OTA online seg status %u offset %u len %u", online->m_status, offset, len);

    if (UHAPP_OTA_DL_STATUS_DL_DESC == online->m_status)
    {
        ret = uhapp_ota_desc_seg_write(online->m_descInfo, offset, buf, len);
    }
    else if (UHAPP_OTA_DL_STATUS_DL_APP == online->m_status)
    {
        ret = uhapp_ota_app_seg_write(online->m_appCtx, offset, buf, len);
    }
    else if (UHAPP_OTA_DL_STATUS_DL_EPP == online->m_status)
    {
        ret = uhapp_ota_epp_seg_write(online->m_eppCtx, offset, buf, len);
    }

    return ret;
}

uhsd_s32 uhapp_ota_onlinePkg_seg_finish(uhsd_void *onlineCtx)
{
    uhapp_ota_onlinePkg_t *online = (uhapp_ota_onlinePkg_t *)onlineCtx;
    uhsd_s32 ret = UHOS_FAILURE;
    uhsd_u32 appOff = 0, appLen = 0;
    uhsd_bool only_app = UHOS_FALSE;
    uhsd_u8 digest[32] = {0};

    UHAPP_LOG("OTA online finish status %u", online->m_status);

    if (UHAPP_OTA_DL_STATUS_DL_DESC == online->m_status)
    {
        ret = uhapp_ota_desc_seg_finish(online->m_descInfo);
        if (UHOS_SUCCESS != ret)
        {
            goto fini;
        }

        if (UHOS_FALSE == uhapp_ota_desc_is_complete(online->m_descInfo))
        {
            ret = UHOS_SUCCESS;
            goto fini;
        }

        if (UHOS_TRUE == uhapp_ota_desc_app_check(online->m_descInfo))
        {
            /* 升级设备APP */
            online->m_appCtx = uhapp_ota_app_new(&online->m_fwInfo, &online->m_op);
            if (UHOS_NULL == online->m_appCtx)
            {
                goto fini;
            }

            if (1 == uhapp_ota_desc_num_get(online->m_descInfo))
            {
                only_app = UHOS_TRUE;
            }

            uhapp_ota_desc_appInfo_get(online->m_descInfo, &appOff, &appLen, digest);

            uhapp_ota_app_set_digest(online->m_appCtx, digest);

            ret = uhapp_ota_app_start(online->m_appCtx, appOff, appLen, only_app);
            if (UHOS_FAILURE == ret)
            {
                goto fini;
            }

            online->m_status = UHAPP_OTA_DL_STATUS_DL_APP;
        }
        else if (UHOS_TRUE == uhapp_ota_desc_epp_check(online->m_descInfo))
        {
            /* 升级底板 */
            online->m_eppCtx = uhapp_ota_epp_new(&online->m_fwInfo, &online->m_op);
            if (UHOS_NULL == online->m_eppCtx)
            {
                goto fini;
            }

            ret = uhapp_ota_epp_start(online->m_eppCtx,
                                      uhapp_ota_desc_subDesc_get(online->m_descInfo),
                                      uhapp_ota_desc_num_get(online->m_descInfo),
                                      uhapp_ota_desc_eppNum_get(online->m_descInfo));

            if (UHOS_FAILURE == ret)
            {
                goto fini;
            }

            online->m_status = UHAPP_OTA_DL_STATUS_DL_EPP;
        }
        else
        {
            goto fini;
        }
    }
    else if (UHAPP_OTA_DL_STATUS_DL_APP == online->m_status)
    {
        ret = uhapp_ota_app_seg_finish(online->m_appCtx);
        if (UHOS_SUCCESS != ret)
        {
            goto fini;
        }

        if (UHOS_FALSE == uhapp_ota_app_is_complete(online->m_appCtx))
        {
            ret = UHOS_SUCCESS;
            goto fini;
        }

        /* 如果没有EPP则直接结束升级 */
        if (UHOS_FALSE == uhapp_ota_desc_epp_check(online->m_descInfo))
        {
            ret = UHAPP_OTA_ERR_NO_NEED_UPGRADE;
            goto fini;
        }

        online->m_eppCtx = uhapp_ota_epp_new(&online->m_fwInfo, &online->m_op);
        if (UHOS_NULL == online->m_eppCtx)
        {
            goto fini;
        }

        ret = uhapp_ota_epp_start(online->m_eppCtx,
                                  uhapp_ota_desc_subDesc_get(online->m_descInfo),
                                  uhapp_ota_desc_num_get(online->m_descInfo),
                                  uhapp_ota_desc_eppNum_get(online->m_descInfo));

        if (UHOS_FAILURE == ret)
        {
            goto fini;
        }

        online->m_status = UHAPP_OTA_DL_STATUS_DL_EPP;
    }
    else if (UHAPP_OTA_DL_STATUS_DL_EPP == online->m_status)
    {
        ret = uhapp_ota_epp_seg_finish(online->m_eppCtx);
    }

fini:
    return ret;
}