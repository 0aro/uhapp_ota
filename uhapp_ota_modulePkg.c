#include "uh_libc.h"
#include "uhsd_ota.h"
#include "uhapp_ota.h"
#include "uhapp_common.h"
#include "uhapp_ota_common.h"
#include "uhapp_ota_modulePkg.h"
#include "uhapp_ota_app.h"

/**
 * @brief OTA模组类型描述结构体
 */
typedef struct
{
    /**
     * @brief OTA模组类型下载状态
     */
    uhapp_ota_dl_status_e m_status;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief dev固件描述对象
     */
    uhsd_void *m_appCtx;

    /**
     * @brief OTA相关operation
     */
    uhapp_ota_op_t m_op;

} uhapp_ota_modulePkg_t;

uhsd_void *uhapp_ota_modulePkg_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op)
{
    uhapp_ota_modulePkg_t *ret = UHOS_NULL;

    ret = uhos_libc_zalloc(sizeof(uhapp_ota_modulePkg_t));
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

uhsd_void uhapp_ota_modulePkg_destroy(uhsd_void *moduleCtx)
{
    uhapp_ota_modulePkg_t *module = (uhapp_ota_modulePkg_t *)moduleCtx;

    if (UHOS_NULL != module)
    {
        if (UHOS_NULL != module->m_appCtx)
        {
            uhapp_ota_app_destroy(module->m_appCtx);
            module->m_appCtx = UHOS_NULL;
        }
        uhos_libc_free(module);
    }
}

uhsd_s32 uhapp_ota_modulePkg_start(uhsd_void *moduleCtx)
{
    uhapp_ota_modulePkg_t *module = (uhapp_ota_modulePkg_t *)moduleCtx;
    uhsd_s32 ret = UHOS_FAILURE;

    /* 升级设备APP */
    module->m_appCtx = uhapp_ota_app_new(&module->m_fwInfo, &module->m_op);
    if (UHOS_NULL == module->m_appCtx)
    {
        goto fini;
    }

    ret = uhapp_ota_app_start(module->m_appCtx, 0, module->m_fwInfo.total_len, UHOS_TRUE);
    if (UHOS_FAILURE == ret)
    {
        goto fini;
    }

    module->m_status = UHAPP_OTA_DL_STATUS_DL_APP;

fini:
    return ret;
}

uhsd_s32 uhapp_ota_modulePkg_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow)
{
    return uhapp_ota_app_query_allow(ack_fn, is_allow);
}

uhsd_s32 uhapp_ota_modulePkg_seg_write(uhsd_void *moduleCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhapp_ota_modulePkg_t *module = (uhapp_ota_modulePkg_t *)moduleCtx;
    uhsd_s32 ret = UHOS_FAILURE;

    UHAPP_LOG("OTA module %u offset %u len %u", module->m_status, offset, len);

    if (UHAPP_OTA_DL_STATUS_DL_APP == module->m_status)
    {
        ret = uhapp_ota_app_seg_write(module->m_appCtx, offset, buf, len);
    }

    return ret;
}

uhsd_s32 uhapp_ota_modulePkg_seg_finish(uhsd_void *moduleCtx)
{
    uhapp_ota_modulePkg_t *module = (uhapp_ota_modulePkg_t *)moduleCtx;
    uhsd_s32 ret = UHOS_SUCCESS;

    UHAPP_LOG("OTA module finish %u", module->m_status);

    if (UHAPP_OTA_DL_STATUS_DL_APP == module->m_status)
    {
        ret = uhapp_ota_app_seg_finish(module->m_appCtx);
        if (UHOS_SUCCESS != ret)
        {
            goto fini;
        }

        if (UHOS_FALSE == uhapp_ota_app_is_complete(module->m_appCtx))
        {
            goto fini;
        }

        /* 设置状态，等待重启 */
        module->m_status = UHAPP_OTA_DL_STATUS_FINISH;

        /* 如果版本相同, 则不需要重启，直接停止升级 */
        if (UHOS_FALSE == uhapp_ota_app_is_need_reset(module->m_appCtx))
        {
            ret = UHAPP_OTA_ERR_NO_NEED_UPGRADE;
        }
    }

fini:
    return ret;
}