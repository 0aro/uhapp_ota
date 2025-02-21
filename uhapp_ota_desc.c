#include "uh_libc.h"
#include "uhapp_common.h"
#include "uhsd_ota.h"
#include "uhapp_ota_common.h"
#include "uhapp_ota_desc.h"

/**
 * @brief 描述信息下载状态定义
 */
typedef enum
{
    UHAPP_OTA_DESC_STATUS_INIT = 0x00,
    UHAPP_OTA_DESC_STATUS_DL_BASE_INFO,
    UHAPP_OTA_DESC_STATUS_DL_DESC,
    UHAPP_OTA_DESC_STATUS_DL_SUB_DSEC,
    UHAPP_OTA_DESC_STATUS_COMPLETE
} uhapp_ota_desc_status_e;

/**
 * @brief ota信息描述结构体
 */
typedef struct
{
    /**
     * @brief ota描述信息状态
     */
    uhapp_ota_desc_status_e m_status;

    /**
     * @brief 固件基础信息
     */
    uhsd_ota_online_whole_base_info_t m_basicInfo;

    /**
     * @brief 固件描述信息
     */
    uhsd_ota_online_whole_desc_info_t m_descInfo;

    /**
     * @brief 子固件描述信息裸数据
     */
    uhsd_u8 *m_sub_rawData;

    /**
     * @brief 子固件描述信息裸数据长度
     */
    uhsd_u32 m_sub_rawLen;

    /**
     * @brief 子固件描述信息裸数据偏移地址
     */
    uhsd_u32 m_sub_rawOff;

    /**
     * @brief 子固件描述信息
     */
    uhsd_ota_online_sub_firm_info_t *m_subDesc;

    /**
     * @brief 是否含有epp固件
     */
    uhsd_u8 m_has_epp_fw;

    /**
     * @brief 是否含有app固件
     */
    uhsd_u8 m_has_app_fw;

    /**
     * @brief OTA相关operation
     */
    uhapp_ota_op_t m_op;
} uhapp_ota_fwDesc_t;

uhsd_void *uhapp_ota_desc_new(uhapp_ota_op_t *op)
{
    uhapp_ota_fwDesc_t *ret = UHOS_NULL;

    ret = uhos_libc_zalloc(sizeof(uhapp_ota_fwDesc_t));
    if (UHOS_NULL == ret)
    {
        goto fini;
    }

    uhos_libc_memcpy(&ret->m_op, op, sizeof(uhapp_ota_op_t));
    ret->m_status = UHAPP_OTA_DESC_STATUS_INIT;

fini:
    return (uhsd_void *)ret;
}

uhsd_void uhapp_ota_desc_destroy(uhsd_void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    if (UHOS_NULL != desc)
    {
        if (UHOS_NULL != desc->m_sub_rawData)
        {
            uhos_libc_free(desc->m_sub_rawData);
            desc->m_sub_rawData = UHOS_NULL;
        }

        if (UHOS_NULL != desc->m_subDesc)
        {
            uhos_libc_free(desc->m_subDesc);
            desc->m_subDesc = UHOS_NULL;
        }
        uhos_libc_free(desc);
    }
}

uhsd_s32 uhapp_ota_desc_start(void *descCtx)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    desc->m_status = UHAPP_OTA_DESC_STATUS_DL_BASE_INFO;

    ret = desc->m_op.dl_fw_fn(0, sizeof(uhsd_ota_online_whole_base_info_t));  /* 下载固件包基本信息 */

    return ret;
}

uhsd_s32 uhapp_ota_desc_seg_write(uhsd_void *descCtx, uhsd_u32 offset, uhsd_u8 *buf, uhsd_u32 len)
{
    uhsd_s32 ret = UHOS_SUCCESS;
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    UHAPP_LOG("OTA desc status %u offset %u len %u", desc->m_status, offset, len);

    if (UHAPP_OTA_DESC_STATUS_DL_BASE_INFO == desc->m_status)
    {
        uhsd_ota_get_online_whole_base_info(buf, len, &desc->m_basicInfo);
    }
    else if (UHAPP_OTA_DESC_STATUS_DL_DESC == desc->m_status)
    {
        uhsd_ota_get_online_whole_desc_info(buf, len, &desc->m_descInfo);
    }
    else if (UHAPP_OTA_DESC_STATUS_DL_SUB_DSEC == desc->m_status)
    {
        uhos_libc_memcpy(desc->m_sub_rawData + desc->m_sub_rawOff, buf, len);
        desc->m_sub_rawOff += len;
    }

    return ret;
}

uhsd_s32 uhapp_ota_desc_seg_finish(uhsd_void *descCtx)
{
    uhsd_s32 ret = UHOS_FAILURE;
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;
    uhsd_u8 i = 0;

    UHAPP_LOG("OTA desc finish status %u", desc->m_status);

    if (UHAPP_OTA_DESC_STATUS_DL_BASE_INFO == desc->m_status)
    {
        desc->m_status = UHAPP_OTA_DESC_STATUS_DL_DESC;

        ret = desc->m_op.dl_fw_fn(sizeof(uhsd_ota_online_whole_base_info_t), sizeof(uhsd_ota_online_whole_desc_info_t));  /* 下载固件包描述信息 */
    }
    else if (UHAPP_OTA_DESC_STATUS_DL_DESC == desc->m_status)
    {
        desc->m_status = UHAPP_OTA_DESC_STATUS_DL_SUB_DSEC;

        desc->m_sub_rawLen = desc->m_descInfo.sub_firm_count * sizeof(uhsd_ota_online_sub_firm_info_t);
        desc->m_sub_rawData = uhos_libc_zalloc(desc->m_sub_rawLen);
        if (UHOS_NULL == desc->m_sub_rawData)
        {
            goto fini;
        }

        ret = desc->m_op.dl_fw_fn(sizeof(uhsd_ota_online_whole_base_info_t) + sizeof(uhsd_ota_online_whole_desc_info_t), desc->m_sub_rawLen);  /* 下载子固件包描述信息 */
    }
    else if (UHAPP_OTA_DESC_STATUS_DL_SUB_DSEC == desc->m_status)
    {
        desc->m_subDesc = uhos_libc_zalloc(desc->m_sub_rawLen);
        if (UHOS_NULL == desc->m_subDesc)
        {
            goto fini;
        }

        uhsd_ota_get_online_sub_firm_info(desc->m_sub_rawData, desc->m_sub_rawLen, &desc->m_basicInfo, &desc->m_descInfo, desc->m_subDesc);

        for (i = 0; i < desc->m_descInfo.sub_firm_count; i++)
        {
            if (UHAPP_OTA_SUBFW_TYPE_EPP == desc->m_subDesc[i].firm_type)
            {
                desc->m_has_epp_fw = 1;
            }
            else
            {
                desc->m_has_app_fw = 1;
            }
        }

        desc->m_status = UHAPP_OTA_DESC_STATUS_COMPLETE;

        ret = UHOS_SUCCESS;
    }

fini:
    return ret;
}

uhsd_bool uhapp_ota_desc_is_complete(uhsd_void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    return ((UHAPP_OTA_DESC_STATUS_COMPLETE == desc->m_status) ? UHOS_TRUE : UHOS_FALSE);
}

uhsd_bool uhapp_ota_desc_epp_check(void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    return ((0 != desc->m_has_epp_fw) ? UHOS_TRUE : UHOS_FALSE);
}

uhsd_bool uhapp_ota_desc_app_check(void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    return ((0 != desc->m_has_app_fw) ? UHOS_TRUE : UHOS_FALSE);
}

uhsd_s32 uhapp_ota_desc_appInfo_get(void *descCtx, uhsd_u32 *appOff, uhsd_u32 *appLen, uhsd_u8 digest[32])
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;
    uhsd_u8 i = 0;
    uhsd_s32 ret = UHOS_FAILURE;

    for (i = 0; i < desc->m_descInfo.sub_firm_count; i++)
    {
        if (UHAPP_OTA_SUBFW_TYPE_EPP != desc->m_subDesc[i].firm_type)
        {
            *appOff = desc->m_subDesc[i].firm_addr;
            *appLen = desc->m_subDesc[i].firm_size;
            uhos_libc_memcpy(digest, desc->m_subDesc[i].summary, sizeof(desc->m_subDesc[i].summary));
            ret = UHOS_SUCCESS;
            break;
        }
    }

    return ret;
}

uhsd_u32 uhapp_ota_desc_num_get(void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    return desc->m_descInfo.sub_firm_count;
}

uhsd_u32 uhapp_ota_desc_eppNum_get(void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;
    uhsd_u8 i = 0;
    uhsd_u32 num = 0;

    for (i = 0; i < desc->m_descInfo.sub_firm_count; i++)
    {
        if (UHAPP_OTA_SUBFW_TYPE_EPP == desc->m_subDesc[i].firm_type)
        {
            num++;
        }
    }

    return num;
}

uhsd_void *uhapp_ota_desc_subDesc_get(void *descCtx)
{
    uhapp_ota_fwDesc_t *desc = (uhapp_ota_fwDesc_t *)descCtx;

    return desc->m_subDesc;
}