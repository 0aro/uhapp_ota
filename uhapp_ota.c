#include "uh_libc.h"
#include "uhapp_common.h"
#include "uhapp_ota.h"
#include "uhsd_ota.h"
#include "uhsd.h"
#include "uhapp_ota_localPkg.h"
#include "uhapp_ota_onlinePkg.h"
#include "uhapp_ota_modulePkg.h"

/**
 * @brief ota打包类型枚举定义
 */
typedef enum
{
    UHAPP_OTA_PKG_TYPE_ONLINE = 0x01,
    UHAPP_OTA_PKG_TYPE_LOCAL,
    UHAPP_OTA_PKG_TYPE_VOICE_MODULE,
    UHAPP_OTA_PKG_TYPE_WIFI_MODULE
} uhapp_pkgType_e;

/**
 * @brief ota状态枚举定义
 */
typedef enum
{
    UHAPP_OTA_STATUS_INIT = 0x00,
    UHAPP_OTA_STATUS_CHECK,
    UHAPP_OTA_STATUS_DL,
} uhapp_ota_status_e;

/**
 * @brief ota信息描述结构体
 */
typedef struct
{
    /**
     * @brief ota总状态
     */
    uhapp_ota_status_e m_status;

    /**
     * @brief 升级超时定时器句柄
     */
    uhsd_void *m_otaTimer;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief 固件打包信息对象
     */
    uhsd_void *m_pkgInfo;

    /**
     * @brief OTA停止标志，只针
     * 对EPP握手后主动结束升级，OTA组件还未推送升级请求
     */
    uhsd_u32 m_is_stop;

    /**
     * @brief OTA停止原因
     */
    uhsd_s32 m_err;

    /**
     * @brief 打包固件类型
     */
    uhapp_pkgType_e m_pkgType;

    /**
     * @brief OTA相关operation
     */
    uhapp_ota_op_t m_op;
} uhapp_ota_ctrl_t;

static uhapp_ota_ctrl_t g_app_ota_hdl;

static uhsd_s32 uhapp_ota_stop_cb(uhsd_s32 err);
static uhsd_s32 uhapp_ota_fw_dl_cb(uhsd_u32 offset, uhsd_u32 len);
static uhsd_s32 uhapp_ota_status_rpt_cb(uhsd_u8 status, uhsd_u8 is_complete);
static uhsd_s32 uhapp_ota_image_write(uhos_u32 offset, uhos_u8 *fragment, uhos_u32 len);
static uhsd_s32 uhapp_ota_image_read(uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len);
static uhsd_s32 uhapp_ota_evt_notify_cb(const uhsd_evt_t *ota_evt, void *userdata);

uhsd_s32 uhapp_ota_init(uhsd_void)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhsd_ota_operation_t opt;

#ifdef CONFIG_WIFI_MODULE
    UHAPP_LOG("HW Ver      : H_%s", UHAPP_HW_VERSION);
    UHAPP_LOG("HW TYPE     : %s", UHAPP_HW_TYPE);
    UHAPP_LOG("SW Ver      : e_%s", UHAPP_SW_VERSION);
    UHAPP_LOG("SW Type     : %s", UHAPP_SW_TYPE);
#else
    UHAPP_LOG("HW Ver      : %s", UHAPP_HW_VERSION);
    UHAPP_LOG("HW TYPE     : %s", UHAPP_HW_TYPE);
    UHAPP_LOG("SW Ver      : %s", UHAPP_SW_VERSION);
    UHAPP_LOG("SW Type     : %s", UHAPP_SW_TYPE);
#endif
    UHAPP_LOG("UHomeOS SDK : V%s", uhsd_version());

    uhos_libc_memset(&g_app_ota_hdl, 0, sizeof(uhapp_ota_ctrl_t));

    g_app_ota_hdl.m_status = UHAPP_OTA_STATUS_INIT;
    g_app_ota_hdl.m_op.dl_fw_fn = uhapp_ota_fw_dl_cb;
    g_app_ota_hdl.m_op.dl_status_rpt_fn = uhapp_ota_status_rpt_cb;
    g_app_ota_hdl.m_op.stop_fn = uhapp_ota_stop_cb;

    uhsd_ota_evt_register(uhapp_ota_evt_notify_cb, &g_app_ota_hdl);

    opt.firmware_write = uhapp_ota_image_write;
    opt.firmware_read = uhapp_ota_image_read;
    ret = uhsd_ota_operation_set(opt, UHOS_NULL);

    uhapp_ota_epp_init(&g_app_ota_hdl.m_op);

    return ret;
}

static uhsd_void __uhapp_ota_stop(uhsd_void)
{
    if (UHAPP_OTA_STATUS_INIT != g_app_ota_hdl.m_status && UHOS_NULL != g_app_ota_hdl.m_pkgInfo)
    {
        if (UHAPP_OTA_PKG_TYPE_LOCAL == g_app_ota_hdl.m_pkgType)
        {
            uhapp_ota_localPkg_destroy(g_app_ota_hdl.m_pkgInfo);
        }
        else if (UHAPP_OTA_PKG_TYPE_ONLINE == g_app_ota_hdl.m_pkgType)
        {
            uhapp_ota_onlinePkg_destroy(g_app_ota_hdl.m_pkgInfo);
        }
        else
        {
            uhapp_ota_modulePkg_destroy(g_app_ota_hdl.m_pkgInfo);
        }
    }

    g_app_ota_hdl.m_status = UHAPP_OTA_STATUS_INIT;
    g_app_ota_hdl.m_pkgType = 0;
    uhos_libc_memset(&g_app_ota_hdl.m_fwInfo, 0, sizeof(uhsd_ota_evt_firm_info_t));
}

static uhsd_void uhapp_ota_stop(uhsd_s32 err)
{
    uhsd_ota_notify_stop(g_app_ota_hdl.m_fwInfo.upgrade_sn, err);
    __uhapp_ota_stop();
}

uhsd_s32 uhapp_ota_is_running(uhsd_void)
{
    return ((g_app_ota_hdl.m_status != UHAPP_OTA_STATUS_INIT) ? UHOS_SUCCESS : UHOS_FAILURE);
}

/* 目前OTA组件内部回调单次长度最大为1024字节， 如果请求段的大小超过1024字节，则分多次回调 */
static uhsd_s32 uhapp_ota_image_write(uhos_u32 offset, uhos_u8 *fragment, uhos_u32 len)
{
    uhsd_s32 ret = UHOS_FAILURE;

    if (UHAPP_OTA_STATUS_INIT != g_app_ota_hdl.m_status && UHOS_NULL != g_app_ota_hdl.m_pkgInfo)
    {
        if (UHAPP_OTA_PKG_TYPE_LOCAL == g_app_ota_hdl.m_pkgType)
        {
            ret = uhapp_ota_localPkg_seg_write(g_app_ota_hdl.m_pkgInfo, offset, fragment, len);
        }
        else if (UHAPP_OTA_PKG_TYPE_ONLINE == g_app_ota_hdl.m_pkgType)
        {
            ret = uhapp_ota_onlinePkg_seg_write(g_app_ota_hdl.m_pkgInfo, offset, fragment, len);
        }
        else
        {
            ret = uhapp_ota_modulePkg_seg_write(g_app_ota_hdl.m_pkgInfo, offset, fragment, len);
        }
    }

    if (UHOS_SUCCESS != ret)
    {
        uhapp_ota_stop(UHSD_OTA_ERROR_OTHER);
    }

    return 0;
}

static uhsd_s32 uhapp_ota_image_read(uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    return 0;
}

static void uhapp_ota_version_query_cb(uhsd_s32 err, uhsd_ota_version_qry_ack_t *qry_ack, uhsd_void *user_data)
{
    if (err == 0 && qry_ack)
    {
        uhsd_ota_notify_start(qry_ack->firm_info.upgrade_sn);
    }
}

uhsd_s32 uhapp_ota_version_query(uhsd_void)
{
    uhsd_s32 ret = UHOS_FAILURE;

    ret = uhsd_ota_version_qry(uhapp_ota_version_query_cb, UHOS_NULL);

    return ret;
}

static uhsd_s32 uhapp_ota_query_allow_ack_cb(uhsd_u8 is_allow)
{
    if (UHAPP_OTA_STATUS_CHECK == g_app_ota_hdl.m_status)
    {
        if (UHAPP_OTA_QUERY_RESULT_ALLOW == is_allow)
        {
            uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_ALLOW);
        }
        else
        {
            uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_NOT_ALLOW);
        }
    }

    return UHOS_SUCCESS;
}

static void uhapp_ota_req(const uhsd_evt_t *ota_evt)
{
    uhsd_ota_evt_firm_info_t *fwInfo = ota_evt->evt_data.firm_info;
    uhsd_s32 ret = UHAPP_OTA_QUERY_FAIL;
    uhsd_u8 is_allow = 0;

    if (fwInfo == UHOS_NULL)
    {
        goto fini;
    }

    UHAPP_LOG("OTA req sn %u timeout %u src %u total %u dst %u", fwInfo->upgrade_sn, fwInfo->timeout, fwInfo->pkg_src, fwInfo->total_len, fwInfo->upgrade_dst);

    if (g_app_ota_hdl.m_is_stop)
    {
        UHAPP_LOG("OTA was already stop");
        uhapp_ota_stop(g_app_ota_hdl.m_err);
        goto fini;
    }

    if (UHAPP_OTA_STATUS_CHECK == g_app_ota_hdl.m_status)
    {
        UHAPP_LOG("OTA is checking allow");
        goto fini;
    }

    uhos_libc_memcpy(&g_app_ota_hdl.m_fwInfo, fwInfo, sizeof(uhsd_ota_evt_firm_info_t));

    if (fwInfo->upgrade_dst == UHSD_OTA_UPGRADE_DST_WHOLE)
    {
        if (fwInfo->pkg_src == UHSD_OTA_PKG_LOCAL)
        {
            g_app_ota_hdl.m_pkgType = UHAPP_OTA_PKG_TYPE_LOCAL;

            ret = uhapp_ota_localPkg_query_allow(uhapp_ota_query_allow_ack_cb, &is_allow);
        }
        else
        {
            g_app_ota_hdl.m_pkgType = UHAPP_OTA_PKG_TYPE_ONLINE;

            ret = uhapp_ota_onlinePkg_query_allow(uhapp_ota_query_allow_ack_cb, &is_allow);
        }
    }
    else
    {
        /* 目前只支持语音模组升级 */
        if (fwInfo->upgrade_dst == UHSD_OTA_UPGRADE_DST_VOICE)
        {
            g_app_ota_hdl.m_pkgType = UHAPP_OTA_PKG_TYPE_VOICE_MODULE;

            ret = uhapp_ota_modulePkg_query_allow(uhapp_ota_query_allow_ack_cb, &is_allow);
        }
        else if (fwInfo->upgrade_dst == UHSD_OTA_UPGRADE_DST_WIFI_MODULE)
        {
            g_app_ota_hdl.m_pkgType = UHAPP_OTA_PKG_TYPE_WIFI_MODULE;
            ret = uhapp_ota_modulePkg_query_allow(uhapp_ota_query_allow_ack_cb, &is_allow);
        }
        else
        {
            goto fini;
        }
    }

    if (UHAPP_OTA_QUERY_FAIL == ret)
    {
        uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_NOT_ALLOW);
    }
    else if (UHAPP_OTA_QUERY_SUCCESS == ret)
    {
        if (UHAPP_OTA_QUERY_RESULT_ALLOW == is_allow)
        {
            uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_ALLOW);
        }
        else
        {
            uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_NOT_ALLOW);
        }
    }
    else
    {
        /* 等待异步回调结果返回 */
        g_app_ota_hdl.m_status = UHAPP_OTA_STATUS_CHECK;
        uhsd_ota_notify_req_result(UHSD_OTA_REQ_RESULT_LATER);
    }

    UHAPP_LOG("OTA req status %u pkgType %u", g_app_ota_hdl.m_status, g_app_ota_hdl.m_pkgType);
fini:
    return;
}

static void uhapp_ota_fw_ready(const uhsd_evt_t *ota_evt)
{
    uhsd_ota_evt_firm_info_t *fwInfo = ota_evt->evt_data.firm_info;
    uhsd_s32 ret = UHOS_FAILURE;

    if (fwInfo == UHOS_NULL)
    {
        goto fini;
    }

    g_app_ota_hdl.m_status = UHAPP_OTA_STATUS_DL;

    if (UHAPP_OTA_PKG_TYPE_LOCAL == g_app_ota_hdl.m_pkgType)
    {
        g_app_ota_hdl.m_pkgInfo = uhapp_ota_localPkg_new(fwInfo, &g_app_ota_hdl.m_op);
        if (UHOS_NULL == g_app_ota_hdl.m_pkgInfo)
        {
            goto fini;
        }

        ret = uhapp_ota_localPkg_start(g_app_ota_hdl.m_pkgInfo);
    }
    else if (UHAPP_OTA_PKG_TYPE_ONLINE == g_app_ota_hdl.m_pkgType)
    {

        g_app_ota_hdl.m_pkgInfo = uhapp_ota_onlinePkg_new(fwInfo, &g_app_ota_hdl.m_op);
        if (UHOS_NULL == g_app_ota_hdl.m_pkgInfo)
        {
            goto fini;
        }

        ret = uhapp_ota_onlinePkg_start(g_app_ota_hdl.m_pkgInfo);
    }
    else
    {
        g_app_ota_hdl.m_pkgInfo = uhapp_ota_modulePkg_new(fwInfo, &g_app_ota_hdl.m_op);
        if (UHOS_NULL == g_app_ota_hdl.m_pkgInfo)
        {
            goto fini;
        }

        ret = uhapp_ota_modulePkg_start(g_app_ota_hdl.m_pkgInfo);
    }

    if (UHOS_FAILURE == ret)
    {
        goto fini;
    }

    return;
fini:
    uhapp_ota_stop(UHSD_OTA_ERROR_OTHER);
    return;
}

static void uhapp_ota_seg_finish(const uhsd_evt_t *ota_evt)
{
    uhsd_s32 ret = UHSD_OTA_ERROR_OTHER;

    if (UHAPP_OTA_STATUS_INIT != g_app_ota_hdl.m_status && UHOS_NULL != g_app_ota_hdl.m_pkgInfo)
    {
        if (UHAPP_OTA_PKG_TYPE_LOCAL == g_app_ota_hdl.m_pkgType)
        {
            ret = uhapp_ota_localPkg_seg_finish(g_app_ota_hdl.m_pkgInfo);
        }
        else if (UHAPP_OTA_PKG_TYPE_ONLINE == g_app_ota_hdl.m_pkgType)
        {
            ret = uhapp_ota_onlinePkg_seg_finish(g_app_ota_hdl.m_pkgInfo);
        }
        else
        {
            ret = uhapp_ota_modulePkg_seg_finish(g_app_ota_hdl.m_pkgInfo);
        }
    }

    /* 不需要升级直接，停止升级，且错误码为 0 */
    if (UHAPP_OTA_ERR_NO_NEED_UPGRADE == ret)
    {
        uhapp_ota_stop(UHSD_OTA_ERROR_OK);
    }
    else if (UHOS_SUCCESS == ret)
    {
        goto fini;
    }
    else
    {
        uhapp_ota_stop(ret);
    }

fini:
    return;
}

static void uhapp_ota_exception(const uhsd_evt_t *ota_evt)
{
    __uhapp_ota_stop();
}

static uhsd_s32 uhapp_ota_stop_cb(uhsd_s32 err)
{
    UHAPP_LOG("OTA stop %d", err);

    /**
     * 偶见EPP握手后底板主动结束升级，OTA组件还未推送
     * 升级请求，此时需要记录停止标志，等待推送OTA后结束升级
     */
    if (UHAPP_OTA_STATUS_INIT == g_app_ota_hdl.m_status && 0 == g_app_ota_hdl.m_pkgType)
    {
        g_app_ota_hdl.m_is_stop = 1;
        g_app_ota_hdl.m_err = err;
    }
    else
    {
        uhapp_ota_stop(err);
    }

    return UHOS_SUCCESS;
}

static uhsd_s32 uhapp_ota_fw_dl_cb(uhsd_u32 offset, uhsd_u32 len)
{
    uhsd_s32 ret = UHOS_FAILURE;

    UHAPP_LOG("OTA dl %u offset %u len %u", g_app_ota_hdl.m_status, offset, len);

    if (UHAPP_OTA_STATUS_INIT != g_app_ota_hdl.m_status)
    {
        ret = uhsd_ota_download_section_firm(g_app_ota_hdl.m_fwInfo.upgrade_sn, offset, len);
    }

    return ret;
}

static uhsd_s32 uhapp_ota_status_rpt_cb(uhsd_u8 status, uhsd_u8 is_complete)
{
    uhsd_s32 ret = UHOS_FAILURE;

    UHAPP_LOG("OTA rpt %u status %u complete %u", g_app_ota_hdl.m_status, status, is_complete);

    if (UHAPP_OTA_STATUS_INIT != g_app_ota_hdl.m_status)
    {
        // TODO 4.3 和 4.4 接口
        ret = uhsd_ota_upgrade_status_rpt(g_app_ota_hdl.m_fwInfo.upgrade_sn, status, is_complete);
    }

    return ret;
}

static uhsd_s32 uhapp_ota_evt_notify_cb(const uhsd_evt_t *ota_evt, void *userdata)
{
    if (ota_evt == UHOS_NULL)
    {
        return UHOS_FAILURE;
    }

    UHAPP_LOG("OTA evt type %u", ota_evt->evt);

    switch (ota_evt->evt)
    {
    case UHSD_OTA_EVT_REQ:
        UHAPP_LOG("UHSD_OTA_EVT_REQ");
        uhapp_ota_req(ota_evt);
        break;

    case UHSD_OTA_EVT_FIRM_READY:
        UHAPP_LOG("UHSD_OTA_EVT_FIRM_READY");
        uhapp_ota_fw_ready(ota_evt);
        break;

    case UHSD_OTA_EVT_SECTION_FIRM_DOWNLOAD_FINISH:
        UHAPP_LOG("UHSD_OTA_EVT_SECTION_FIRM_DOWNLOAD_FINISH");
        uhapp_ota_seg_finish(ota_evt);
        break;

    case UHSD_OTA_EVT_EXCEPTION_OCCURED:
        uhapp_ota_exception(ota_evt);
        break;

    default:
        break;
    }

    return UHOS_SUCCESS;
}
