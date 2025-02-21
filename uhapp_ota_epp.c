#include "uhepp.h"
#include "uh_libc.h"
#include "uh_sys.h"
#include "uhapp_ota.h"
#include "uhapp_common.h"
#include "uhsd_ota.h"
#include "uhapp_ota_common.h"
#include "uhapp_ota_epp.h"

/**
 * @brief OTA epp下载状态枚举定义
 */
typedef enum
{
    UHAPP_OTA_EPP_UPGRADE_TYPE_DEF = 0x00,
    UHAPP_OTA_EPP_UPGRADE_TYPE_SUPPROT,
    UHAPP_OTA_EPP_UPGRADE_TYPE_NO_SUPPORT,
} uhapp_ota_epp_upgrade_type_e;

/**
 * @brief OTA epp下载状态枚举定义
 */
typedef enum
{
    UHAPP_OTA_EPP_STATUS_INIT = 0x00,
    UHAPP_OTA_EPP_STATUS_DL_REQ,
    UHAPP_OTA_EPP_STATUS_DL_EPP,
    UHAPP_OTA_EPP_STATUS_COMPLETE
} uhapp_ota_epp_status_e;

/**
 * @brief OTA epp描述结构体
 */
typedef struct
{
    /**
     * @brief  EPP下载状态
     */
    uhapp_ota_epp_status_e m_status;

    /**
     * @brief 固件请求信息
     */
    uhsd_ota_evt_firm_info_t m_fwInfo;

    /**
     * @brief OTA升级类型
     */
    uhapp_ota_epp_upgrade_type_e m_upgradeType;

    /**
     * @brief OTA是否已经开始标志
     */
    uhsd_u8 m_is_start;

    /**
     * @brief 如果底板无握手成功，则设置此标志
     */
    uhsd_u8 m_need_wait_board;

    /**
     * @brief OTA备份分区
     */
    uhsd_u8 m_imageZone;

    /**
     * @brief OTA备份分区长度
     */
    uhsd_u32 m_imageSize;

    /**
     * @brief 下载固件状态
     */
    uhsd_u8 m_upgradeStatus;

    /**
     * @brief 是否下载完成标志
     */
    uhsd_u8 m_dlComplete;

    /**
     * @brief  OTA相关operation
     */
    uhapp_ota_op_t m_op;

    /**
     * @brief  OTA查询结果回调
     */
    uhapp_ota_query_allow_ack_fn m_ack_fn;
} uhapp_ota_epp_t;

static uhapp_ota_epp_t g_ota_epp; /* 此结构需要单例*/

uhsd_void *uhapp_ota_epp_new(uhsd_ota_evt_firm_info_t *fw_info, uhapp_ota_op_t *op)
{
    g_ota_epp.m_status = UHAPP_OTA_EPP_STATUS_DL_REQ;
    uhos_libc_memcpy(&g_ota_epp.m_fwInfo, fw_info, sizeof(uhsd_ota_evt_firm_info_t));
    uhos_libc_memcpy(&g_ota_epp.m_op, op, sizeof(uhapp_ota_op_t));

    return (uhsd_void *)&g_ota_epp;
}

uhsd_void uhapp_ota_epp_destroy(uhsd_void *eppCtx)
{
    uhapp_ota_epp_t *epp = (uhapp_ota_epp_t *)eppCtx;

    epp->m_status = UHAPP_OTA_EPP_STATUS_INIT;
    uhos_libc_memset(&g_ota_epp.m_fwInfo, 0, sizeof(uhsd_ota_evt_firm_info_t));

    uhepp_ota_stop(0);
}

uhsd_s32 uhapp_ota_epp_start(uhsd_void *eppCtx, uhsd_ota_online_sub_firm_info_t *subDesc, uhsd_u32 subNum, uhsd_u32 eppNum)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_epp_t *epp = (uhapp_ota_epp_t *)eppCtx;
    uhepp_ota_fw_desc_info_t descInfo;
    uhepp_u8 pkt_type = 0x00;

    uhos_libc_memset(&descInfo, 0, sizeof(uhepp_ota_fw_desc_info_t));
    descInfo.fw_len = epp->m_fwInfo.total_len;

    if (epp->m_fwInfo.pkg_src == UHSD_OTA_PKG_LOCAL)
    {
        descInfo.fw_desc_num = 1;
        descInfo.fw_num = 1;
        pkt_type = 0x01;

        descInfo.fw_descs = uhos_libc_zalloc(sizeof(uhepp_ota_fw_desc_t));
        if (UHOS_NULL == descInfo.fw_descs)
        {
            goto fini;
        }

        descInfo.fw_descs->offset = 0;
        descInfo.fw_descs->data_len = epp->m_fwInfo.total_len;
        descInfo.fw_descs->digest_alg = !epp->m_fwInfo.verifier_method;
        uhos_libc_memcpy(descInfo.fw_descs->digest, epp->m_fwInfo.verification, sizeof(descInfo.fw_descs->digest));
    }
    else if (epp->m_fwInfo.pkg_src == UHSD_OTA_PKG_ONLINE)
    {
        uhsd_u8 i = 0, cnt = 0;

        descInfo.fw_desc_num = eppNum;
        descInfo.fw_num = subNum;
        descInfo.fw_descs = uhos_libc_zalloc(descInfo.fw_desc_num * sizeof(uhepp_ota_fw_desc_t));
        if (UHOS_NULL == descInfo.fw_descs)
        {
            goto fini;
        }

        for (i = 0; i < subNum; i++)
        {
            if (UHAPP_OTA_SUBFW_TYPE_EPP == subDesc[i].firm_type)
            {
                if (subDesc[i].firm_size > epp->m_imageSize)
                {
                    UHAPP_LOG("EPP curSize %u maxSize %u", subDesc[i].firm_size, epp->m_imageSize);
                    goto fini;
                }

                descInfo.fw_descs[cnt].digest_alg = 0x00;
                descInfo.fw_descs[cnt].offset = subDesc[i].firm_addr;
                descInfo.fw_descs[cnt].data_len = subDesc[i].firm_size;
                uhos_libc_memcpy(descInfo.fw_descs[cnt].digest, subDesc[i].summary, sizeof(descInfo.fw_descs[i].digest));
                cnt++;
            }
        }
    }
    else
    {
        goto fini;
    }

    ret = uhepp_ota_set_fw_desc(epp->m_fwInfo.upgrade_sn, &descInfo, pkt_type);
    if (UHOS_SUCCESS == ret)
    {
        epp->m_status = UHAPP_OTA_EPP_STATUS_DL_EPP;
    }

    if (UHOS_NULL != descInfo.fw_descs)
    {
        uhos_libc_free(descInfo.fw_descs);
    }

fini:
    return ret;
}

uhsd_s32 uhapp_ota_epp_query_allow(uhapp_ota_query_allow_ack_fn ack_fn, uhsd_u8 *is_allow)
{
    uhsd_s32 ret = UHAPP_OTA_QUERY_FAIL;

    g_ota_epp.m_ack_fn = ack_fn;

    if (UHAPP_OTA_EPP_UPGRADE_TYPE_NO_SUPPORT == g_ota_epp.m_upgradeType)
    {
        ret = UHAPP_OTA_QUERY_SUCCESS;
        *is_allow = UHAPP_OTA_QUERY_RESULT_REJECT;
        goto fini;
    }
    else if (UHAPP_OTA_EPP_UPGRADE_TYPE_DEF == g_ota_epp.m_upgradeType)
    {
        g_ota_epp.m_is_start = 1;
        ret = UHAPP_OTA_QUERY_WAIT;
    }
    else
    {
        ret = uhepp_ota_start_req();
        if (0 == ret)
        {
            ret = UHAPP_OTA_QUERY_WAIT;

            g_ota_epp.m_need_wait_board = 1;
        }
        else if (1 == ret)
        {
            ret = UHAPP_OTA_QUERY_SUCCESS;
            *is_allow = UHAPP_OTA_QUERY_RESULT_ALLOW;
            g_ota_epp.m_op.dl_status_rpt_fn(UHSD_OTA_UPGRADE_STATUS_ALLOW, 1);
            goto fini;
        }
    }

fini:
    return ret;
}

uhsd_s32 uhapp_ota_epp_seg_write(uhsd_void *eppCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_epp_t *epp = (uhapp_ota_epp_t *)eppCtx;

    if (UHAPP_OTA_EPP_STATUS_DL_EPP == epp->m_status)
    {
        ret = uhepp_ota_data_rsp(offset, buf, len);
    }

    return ret;
}

uhsd_s32 uhapp_ota_epp_seg_finish(uhsd_void *eppCtx)
{
    uhsd_s32 ret = UHOS_SUCCESS;

    return ret;
}

uhsd_bool uhapp_ota_epp_is_complete(uhsd_void *eppCtx)
{
    uhapp_ota_epp_t *epp = (uhapp_ota_epp_t *)eppCtx;

    return ((UHAPP_OTA_EPP_STATUS_COMPLETE == epp->m_status) ? UHOS_TRUE : UHOS_FALSE);
}

static uhsd_s32 uhapp_ota_epp_upgrade_status_rpt(uhsd_u8 progress, uhsd_u8 result)
{
    if (result == UHEPP_OTA_STSRT_RESULT_ALLOWED)
    {
        g_ota_epp.m_upgradeStatus = UHSD_OTA_UPGRADE_STATUS_ALLOW;
    }
    else
    {
        if (0 == progress)
        {
            g_ota_epp.m_upgradeStatus = UHSD_OTA_UPGRADE_STATUS_IDLE_NO_DOWNLOAD;
        }
        else
        {
            g_ota_epp.m_upgradeStatus = UHSD_OTA_UPGRADE_STATUS_IDLE_DOWNLOADING;
        }
    }

    g_ota_epp.m_op.dl_status_rpt_fn(g_ota_epp.m_upgradeStatus, g_ota_epp.m_dlComplete);

    return 0;
}

uhsd_s32 uhapp_ota_epp_start_rsp_cb(uhsd_u8 progress, uhsd_u8 result)
{
    if (g_ota_epp.m_need_wait_board)
    {
        g_ota_epp.m_need_wait_board = 0;

        if (result == UHEPP_OTA_STSRT_RESULT_ALLOWED)
        {
            g_ota_epp.m_ack_fn(UHAPP_OTA_QUERY_RESULT_ALLOW);

            /* 说明模块重启固件已经下载完成 */
            if (0x64 == progress)
            {
                g_ota_epp.m_upgradeStatus = UHSD_OTA_UPGRADE_STATUS_ALLOW;
                g_ota_epp.m_dlComplete = 1;
                g_ota_epp.m_op.dl_status_rpt_fn(g_ota_epp.m_upgradeStatus, g_ota_epp.m_dlComplete);
            }
        }
        else if (result == UHEPP_OTA_STSRT_RESULT_IDLE)
        {
            /*注意：此处需要先上报103状态，再allow升级 */
            uhapp_ota_epp_upgrade_status_rpt(progress, result);
            g_ota_epp.m_ack_fn(UHAPP_OTA_QUERY_RESULT_ALLOW);
        }
        else
        {
            g_ota_epp.m_ack_fn(UHAPP_OTA_QUERY_RESULT_REJECT);
        }
    }
    else
    {
        if (result == UHEPP_OTA_STSRT_RESULT_ALLOWED || result == UHEPP_OTA_STSRT_RESULT_IDLE)
        {
            uhapp_ota_epp_upgrade_status_rpt(progress, result);
        }
        else if (result == UHEPP_OTA_STSRT_RESULT_REJECTED)
        {
            g_ota_epp.m_op.stop_fn(UHSD_OTA_ERROR_BASEBOARD_REJECT);
        }
    }

    return UHOS_SUCCESS;
}

static uhsd_s32 uhapp_ota_epp_data_req_cb(uhsd_u32 offset, uhsd_u32 len)
{
    uhsd_s32 ret = UHOS_SUCCESS;

    ret = g_ota_epp.m_op.dl_fw_fn(offset, len);

    return ret;
}

static uhsd_s32 uhapp_ota_epp_stop_cb(uhsd_s32 err)
{
    uhsd_s32 ret = UHOS_SUCCESS;

    if (UHAPP_OTA_EPP_STATUS_INIT != g_ota_epp.m_status)
    {
        ret = g_ota_epp.m_op.stop_fn(err);
    }

    return ret;
}

static uhsd_u32 uhapp_ota_epp_get_size_cb(void)
{
    return g_ota_epp.m_imageSize;
}

static uhsd_s32 uhapp_ota_epp_write_cb(uhepp_ota_zone_type_e type, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhsd_s32 ret = 0;

    if (UHEPP_OTA_ZONE_TYPE_IMAGE == type)
        ret = uhos_sys_image_write(g_ota_epp.m_imageZone, offset, buf, len);  // TODO uhos_sys_image_write函数待实现
    else
        ret = uhos_sys_image_write(ZONE_7, offset, buf, len);  // TODO uhos_sys_image_write函数待实现

    if (ret == 0)
    {
        ret = len;
    }

    return ret;
}

static uhsd_s32 uhapp_ota_epp_read_cb(uhepp_ota_zone_type_e type, uhsd_u32 offset, uhsd_u8 buf[], uhsd_u32 len)
{
    uhsd_s32 ret = 0;

    if (UHEPP_OTA_ZONE_TYPE_IMAGE == type)
        ret = uhos_sys_image_read(g_ota_epp.m_imageZone, offset, buf, len);  // TODO uhos_sys_image_read函数待实现
    else
        ret = uhos_sys_image_read(ZONE_7, offset, buf, len);  // TODO uhos_sys_image_read函数待实现

    if (ret == 0)
    {
        ret = len;
    }
    return ret;
}

uhsd_s32 uhapp_ota_epp_ota_support_cb(uhsd_u8 flag)
{
    if (flag)
    {
        g_ota_epp.m_upgradeType = UHAPP_OTA_EPP_UPGRADE_TYPE_SUPPROT;
    }
    else
    {
        g_ota_epp.m_upgradeType = UHAPP_OTA_EPP_UPGRADE_TYPE_NO_SUPPORT;
    }

    if (g_ota_epp.m_is_start)
    {
        if (UHAPP_OTA_EPP_UPGRADE_TYPE_NO_SUPPORT == g_ota_epp.m_upgradeType)
        {
            g_ota_epp.m_ack_fn(UHAPP_OTA_QUERY_RESULT_REJECT);
        }
        else
        {
            uhepp_ota_start_req();
            g_ota_epp.m_need_wait_board = 1;
        }
        g_ota_epp.m_is_start = 0;
    }

    return UHOS_SUCCESS;
}

static uhsd_s32 uhapp_ota_epp_download_cb(void)
{
    g_ota_epp.m_dlComplete = 1;

    return UHOS_SUCCESS;
}

uhsd_s32 uhapp_ota_epp_init(uhapp_ota_op_t *op)
{
    uhsd_u8 image_zone = 0;

    uhos_libc_memset(&g_ota_epp, 0, sizeof(uhapp_ota_epp_t));
    g_ota_epp.m_status = UHAPP_OTA_EPP_STATUS_INIT;

    uhos_libc_memcpy(&g_ota_epp.m_op, op, sizeof(uhapp_ota_op_t));

    uhepp_evt_subscribe(UHEPP_OTA_START_RSP, uhapp_ota_epp_start_rsp_cb);
    uhepp_evt_subscribe(UHEPP_OTA_DATA_REQ, uhapp_ota_epp_data_req_cb);
    uhepp_evt_subscribe(UHEPP_OTA_STOP_NOTIFY, uhapp_ota_epp_stop_cb);
    uhepp_evt_subscribe(UHEPP_OTA_BANK_SIZE_GET, uhapp_ota_epp_get_size_cb);
    uhepp_evt_subscribe(UHEPP_OTA_WRITE_FW, uhapp_ota_epp_write_cb);
    uhepp_evt_subscribe(UHEPP_OTA_READ_FW, uhapp_ota_epp_read_cb);
    uhepp_evt_subscribe(UHEPP_OTA_DOWNLOAD_COMPLETE, uhapp_ota_epp_download_cb);
    uhepp_evt_subscribe(UHEPP_OTA_SUPPORT_NOTIFY, uhapp_ota_epp_ota_support_cb);

    uhos_sys_run_image_get(&image_zone);  // TODO 函数待实现

    if (image_zone == ZONE_1)
    {
        g_ota_epp.m_imageZone = ZONE_2;
    }
    else
    {
        g_ota_epp.m_imageZone = ZONE_1;
    }
    g_ota_epp.m_imageSize = uhos_get_sys_image_size(g_ota_epp.m_imageZone);  // TODO 函数待实现

    return 0;
}
