
#include "uh_libc.h"
#include "uhapp_ota.h"
#include "uhsd.h"
#include "uhsd_ota.h"
#include "uhapp_common.h"
#include "uhapp_ota_common.h"
#include "uhapp_ota_app.h"
#include "uh_sys.h"
#include "uh_verify.h"
#include "uh_log.h"

#define UHAPP_OTA_VENDOR_MAGIC_SIG   "haier"
#define UHAPP_OTA_VENDOR_PRODUCT_SIG "UDISCOVERY_UWT"

/**
 * @brief APP固件头部信息
 *  如下头部定义信息使用内部打包工具生成，如果开发者有字节的头部信息，
 *  请修改如下数据结构.并且根据APP固件的组成部分，分段下载。 目前
 *  APP固件组成部分只包含头部128字节和真实固件
 */
typedef struct
{
    uhsd_u8 m_magicSig[16];
    uhsd_u8 m_hwVer[16];
    uhsd_u8 m_swVer[16];
    uhsd_u8 m_productSig[16];
    uhsd_u8 m_verification[64];
} uhapp_vendor_fwHead_t;

/**
 * @brief OTA APP下载状态枚举定义
 */
typedef enum
{
    UHAPP_OTA_APP_STATUS_INIT = 0x00,
    UHAPP_OTA_APP_STATUS_DL_HEAD,
    UHAPP_OTA_APP_STATUS_DL_APP,
    UHAPP_OTA_APP_STATUS_COMPLETE,
    UHAPP_OTA_APP_STATUS_NEED_REBOOT
} uhapp_ota_app_status_e;

/**
 * @brief OTA APP描述结构体
 */
typedef struct
{
    /**
     * @brief OTA APP下载状态
     */
    uhapp_ota_app_status_e m_status;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief 固件头部信息
     */
    uhapp_vendor_fwHead_t m_vendorHead;

    /**
     * @brief 设备APP固件偏移地址
     */
    uhsd_u32 m_appOff;

    /**
     * @brief 设备APP固件长度
     */
    uhsd_u32 m_appLen;

    /**
     * @brief OTA备份分区
     */
    uhsd_u8 m_imageZone;

    /**
     * @brief OTA备份分区长度
     */
    uhsd_u32 m_imageSize;

    /**
     * @brief OTA固件只有APP固件
     */
    uhsd_bool m_only_app;

    /**
     * @brief  OTA相关operation
     */
    uhapp_ota_op_t m_op;

} uhapp_ota_app_t;

uhsd_void *uhapp_ota_app_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op)
{
    uhapp_ota_app_t *ret = UHOS_NULL;
    uhsd_u8 image_zone = 0;

    ret = uhos_libc_zalloc(sizeof(uhapp_ota_app_t));
    if (UHOS_NULL == ret)
    {
        goto fini;
    }

    uhos_libc_memcpy(&ret->m_fwInfo, fw_info, sizeof(uhsd_ota_evt_firm_info_t));
    uhos_libc_memcpy(&ret->m_op, op, sizeof(uhapp_ota_op_t));

    uhos_sys_run_image_get(&image_zone);
    if (image_zone == ZONE_1)
    {
        ret->m_imageZone = ZONE_2;
    }
    else
    {
        ret->m_imageZone = ZONE_1;
    }

    /* 此宏位于uh_flash.h 不同芯片平台固件flash大小可能不一样 */
    ret->m_imageSize = uhos_get_sys_image_size(ret->m_imageZone);
    ret->m_status = UHAPP_OTA_APP_STATUS_INIT;

fini:
    return ret;
}

uhsd_void uhapp_ota_app_destroy(uhsd_void *appCtx)
{
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    if (UHOS_NULL != app)
    {
        uhos_libc_free(app);
    }
}

uhsd_s32 uhapp_ota_app_start(void *appCtx, uhsd_u32 appOff, uhsd_u32 appLen, uhsd_bool is_only_app)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

#ifdef CONFIG_LINUX // SDK暂不需要判断大小限制，直接下载APP,
    app->m_status = UHAPP_OTA_APP_STATUS_DL_APP;
    app->m_appOff = appOff;
    app->m_appLen = appLen;
    app->m_only_app = is_only_app;
    ret = app->m_op.dl_fw_fn(appOff, appLen);
#else
    if ((appLen - sizeof(uhapp_vendor_fwHead_t)) > app->m_imageSize)
    {
        UHAPP_LOG("APP curSize %u maxSize %u", (appLen - sizeof(uhapp_vendor_fwHead_t)), app->m_imageSize);
        goto fini;
    }
    app->m_status = UHAPP_OTA_APP_STATUS_DL_HEAD;
    app->m_appOff = appOff;
    app->m_appLen = appLen;
    app->m_only_app = is_only_app;
    ret = app->m_op.dl_fw_fn(appOff, sizeof(uhapp_vendor_fwHead_t));
fini:
#endif
    return ret;
}

uhsd_s32 uhapp_ota_app_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow)
{
    uhsd_s32 ret = UHAPP_OTA_QUERY_SUCCESS;

    *is_allow = UHAPP_OTA_QUERY_RESULT_ALLOW;

    return ret;
}

uhsd_s32 uhapp_ota_app_seg_write(uhsd_void *appCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhsd_s32 ret = UHOS_SUCCESS;
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    if (UHAPP_OTA_APP_STATUS_DL_HEAD == app->m_status)
    {
        uhos_libc_memcpy(&app->m_vendorHead, buf, len);
    }
    else if (UHAPP_OTA_APP_STATUS_DL_APP == app->m_status)
    {
#ifdef CONFIG_LINUX
        uhos_sys_image_write(app->m_imageZone, offset - app->m_appOff, buf, len);
#else
        uhos_sys_image_write(app->m_imageZone, offset - app->m_appOff - sizeof(uhapp_vendor_fwHead_t), buf, len);
#endif
    }
    else
    {
        ret = UHOS_FAILURE;
    }

    return ret;
}

static uhsd_bool uhapp_ota_check_header(uhapp_ota_app_t *app)
{
    uhsd_bool ret = UHOS_FALSE;

    if ((0 == uhos_libc_memcmp(app->m_vendorHead.m_magicSig, UHAPP_OTA_VENDOR_MAGIC_SIG, sizeof(UHAPP_OTA_VENDOR_MAGIC_SIG))) &&
        (0 == uhos_libc_memcmp(app->m_vendorHead.m_hwVer, UHAPP_HW_VERSION, sizeof(UHAPP_HW_VERSION))) &&
        (0 == uhos_libc_memcmp(app->m_vendorHead.m_productSig, UHAPP_OTA_VENDOR_PRODUCT_SIG, sizeof(UHAPP_OTA_VENDOR_PRODUCT_SIG))))
    {
        ret = UHOS_TRUE;
    }

    return ret;
}

static uhsd_bool uhapp_ota_check_upgrade(uhapp_ota_app_t *app)
{
    uhsd_bool ret = UHOS_FALSE;

    UHAPP_LOG("OTA APP cur %s fw %s", UHAPP_SW_VERSION, app->m_vendorHead.m_swVer);

    if ((uhos_libc_memcmp(app->m_vendorHead.m_swVer, UHAPP_SW_VERSION, sizeof(UHAPP_SW_VERSION)) != 0))
    {
        ret = UHOS_TRUE;
    }

    return ret;
}

#ifndef CONFIG_LINUX
static uhsd_void uhapp_util_hex2str_expand(uhsd_u8 *hex, uhsd_s32 len, uhsd_u8 lowercase)
{
    uhsd_s32 i;
    const char *hexstr = lowercase ? "0123456789abcdef" : "0123456789ABCDEF";

    for (i = len - 1; i >= 0; i--)
    {
        hex[i * 2 + 1] = hexstr[hex[i] & 0xF];
        hex[i * 2] = hexstr[hex[i] >> 4];
    }
}
#endif

#ifndef CONFIG_LINUX
static uhsd_bool uhapp_ota_verfiy_app(uhapp_ota_app_t *app)
{
    uhsd_bool ret = UHOS_FALSE;
    uhos_void *context = UHOS_NULL;
    uhsd_s32 md_type = UHOS_MD_SHA256;
    uhsd_u8 digest[32] = {0};
    uhsd_u32 need_read = 0;
    uhsd_u8 *buf = UHOS_NULL;
    uhsd_u32 offset = 0;
    uhsd_u32 len = app->m_appLen - sizeof(uhapp_vendor_fwHead_t);

#define UHAPP_OTA_DATA_BUF_SIZE 4096

    if (0 == app->m_fwInfo.verifier_method)
    {
        md_type = UHOS_MD_MD5;
    }

    buf = uhos_libc_zalloc(UHAPP_OTA_DATA_BUF_SIZE);
    if (buf == UHOS_NULL)
    {
        goto fini;
    }

    context = uhos_md_init(md_type);
    if (UHOS_NULL == context)
    {
        goto fini;
    }

    uhos_md_update(context, (uhsd_u8 *)&app->m_vendorHead, sizeof(uhapp_vendor_fwHead_t));

    while (offset < len)
    {
        need_read = len - offset;

        if (need_read > UHAPP_OTA_DATA_BUF_SIZE)
        {
            need_read = UHAPP_OTA_DATA_BUF_SIZE;
        }

        if (uhos_sys_image_read(app->m_imageZone, offset, buf, need_read))
        {
            goto fini;
        }

        uhos_md_update(context, buf, need_read);
        offset += need_read;
    }

    uhos_md_finish(context, digest);
    context = UHOS_NULL;
    if (md_type == UHOS_MD_MD5)
    {
        uhapp_util_hex2str_expand(digest, 16, 1);
    }

    if (uhos_libc_memcmp(app->m_fwInfo.verification, digest, 32))
    {
        goto fini;
    }

    ret = UHOS_TRUE;
fini:
    if (buf)
    {
        uhos_libc_free(buf);
    }

    if (context)
    {
        uhos_md_finish(context, digest);
    }

    return ret;
}
#endif

#ifndef CONFIG_LINUX
static uhsd_void uhapp_ota_reboot_cb(uhsd_u32 seconds)
{
    UHAPP_LOG("Reboot notify %d !", seconds);

    uhapp_sys_delay_reset(seconds);
}
#endif

uhsd_s32 uhapp_ota_app_seg_finish(uhsd_void *appCtx)
{
    uhsd_s32 ret = UHOS_SUCCESS;
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    UHAPP_LOG("OTA APP finish %u", app->m_status);

    if (UHAPP_OTA_APP_STATUS_DL_HEAD == app->m_status)
    {
        if (UHOS_FALSE == uhapp_ota_check_header(app))
        {
            UHAPP_LOG("OTA APP invalid header");
            ret = UHSD_OTA_ERROR_ILLEGAL_CONTENT;
            goto fini;
        }

        /* 版本相同，不需要升级 */
        if (UHOS_FALSE == uhapp_ota_check_upgrade(app))
        {
            app->m_status = UHAPP_OTA_APP_STATUS_COMPLETE;
            goto fini;
        }

        /* 下载设备APP */
        app->m_status = UHAPP_OTA_APP_STATUS_DL_APP;
        ret = app->m_op.dl_fw_fn(app->m_appOff + sizeof(uhapp_vendor_fwHead_t), app->m_appLen - sizeof(uhapp_vendor_fwHead_t));
    }
    else if (UHAPP_OTA_APP_STATUS_DL_APP == app->m_status)
    {
#ifdef CONFIG_LINUX
        app->m_op.dl_status_rpt_fn(UHSD_OTA_UPGRADE_STATUS_ALLOW, UHOS_TRUE); // 上报进度105
        return UHOS_SUCCESS; // wifibase SDK sdr 直接返回正常. 如果走之前的逻辑，判断失败会报107
#else
        if (UHOS_FALSE == uhapp_ota_verfiy_app(app))
        {
            UHAPP_LOG("OTA APP invalid fw");
            ret = UHSD_OTA_ERROR_ILLEGAL_CONTENT;
        }
        else
        {
            /* 如果只有设备APP固件，需要上报状态*/
            if (app->m_only_app)
            {
                app->m_op.dl_status_rpt_fn(UHSD_OTA_UPGRADE_STATUS_ALLOW, UHOS_TRUE);
            }

            uhos_sys_image_switch(app->m_imageZone);

            app->m_status = UHAPP_OTA_APP_STATUS_NEED_REBOOT;
            uhsd_reboot_notify(UHSD_OFFLINE_REASON_OTA_RESET, uhapp_ota_reboot_cb);
        }
#endif
    }
    else
    {
        ret = UHOS_FAILURE;
    }

fini:
    return ret;
}

uhsd_bool uhapp_ota_app_is_complete(uhsd_void *appCtx)
{
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    return ((UHAPP_OTA_APP_STATUS_COMPLETE == app->m_status) ? UHOS_TRUE : UHOS_FALSE);
}

uhsd_bool uhapp_ota_app_is_need_reset(uhsd_void *appCtx)
{
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    return ((UHAPP_OTA_APP_STATUS_NEED_REBOOT == app->m_status) ? UHOS_TRUE : UHOS_FALSE);
}

uhsd_s32 uhapp_ota_app_set_digest(uhsd_void *appCtx, uhsd_u8 digest[32])
{
    uhapp_ota_app_t *app = (uhapp_ota_app_t *)appCtx;

    uhos_libc_memcpy(app->m_fwInfo.verification, digest, sizeof(app->m_fwInfo.verification));

    return UHOS_SUCCESS;
}
