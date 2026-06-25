#include <utils/plat_log.h>
#include <sys/time.h>
#include "tmessage.h"
#include "media/mpi_isp.h"
#include "ComponentCommon.h"
#include "TDMRawProcessForTools.h"

#define TDM_RAW_PROCESS_NUM_MAX       (4)
#define DUMP_DEV "/sys/class/sunxi_dump/dump"
#define TDM_RX_REG_BASE              (0x05908100)
#define TDMRAWFRAMECNTMIN 0
#define TDMRAWFRAMECNTMAX 1
#define TDMRAWFRAMETYPE 3
#define FIXEDCAPSPATH "raw"
#define FIXEDFRAMEPATH "/raw_frame"
#define FIXEDFLAGPATH "raw/raw_flag"
#define FIXEDWIDTHPATH "/raw_width"
#define FIXEDHEIGHTPATH "/raw_height"



typedef struct tdm_raw_process_context {
    ISP_DEV mIsp;
    tdm_raw_process_config_param config_param;
    int process_enable;
    int process_frame_cnt;
    FILE *tdm_raw_fp;
    unsigned char *tdm_buf;
    unsigned int tdm_buf_size;
    int mbus_code;
    char flagpath[32];
} tdm_raw_process_context;

static tdm_raw_process_context gTdmRawProcessContext[TDM_RAW_PROCESS_NUM_MAX];

static char *ReadFlagPath(char* flagpath)
{
    char *buffer = NULL;

    FILE *file = fopen(flagpath, "r");
    if (file == NULL)
    {
        aloge("fatal error! open file[%s] fail! errno is %d", flagpath, errno);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    rewind(file);
    if (fileSize == 0)
    {
        aloge("fatal error! flag file is NULL!");
        goto exit;
    }
    buffer = (char *)malloc(sizeof(char) * (fileSize + 1));
    if (NULL == buffer)
    {
        aloge("fatal error! malloc fail! size is %d", fileSize + 1);
        goto exit;
    }
    fread(buffer, sizeof(char), fileSize, file);
    buffer[fileSize] = '\0';

exit:
    fclose(file);

    return buffer;
}

static int DeleteFiles(char *filename)
{
    if(remove(filename) == 0)
    {
        alogd("Delete the flag file successfull");
    }
    else
    {
        aloge("Failed to delete flag file");
        return -1;
    }
    return 0;
}

static int tdm_en_sunxi_read()
{
    FILE *tdm_sunxi_fd;
    unsigned int reg_start, len;
    char tdm_reg_cmd[256];
    char tdm_reg_data[24];
    int tdm_lbc_en = -1;
    reg_start = TDM_RX_REG_BASE;
    len = 32;

    if ((tdm_sunxi_fd = fopen(DUMP_DEV, "r+")) == NULL)
    {
        aloge("open mem failed!\n");
        return -1;
    }
    memset(tdm_reg_cmd, 0x0, sizeof(tdm_reg_cmd));

    snprintf(tdm_reg_cmd, sizeof(tdm_reg_cmd), "0x%x,0x%x", reg_start, (reg_start + len));
    fwrite(tdm_reg_cmd, strlen(tdm_reg_cmd), 1, tdm_sunxi_fd);
    fclose(tdm_sunxi_fd);
    if ((tdm_sunxi_fd = fopen(DUMP_DEV, "r+")) == NULL)
    {
        aloge("open mem failed\n");
        return -2;
    }

    memset(tdm_reg_data, 0x0, sizeof(tdm_reg_data));
    fread(tdm_reg_data, 23, 1, tdm_sunxi_fd);
    tdm_lbc_en = (tdm_reg_data[21] - '0') & 1;
    fclose(tdm_sunxi_fd);
    tdm_sunxi_fd = NULL;

    return tdm_lbc_en;
}

static struct tdm_raw_process_context *tdm_raw_process_get_context(int isp)
{
    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return NULL;
    }
    return &gTdmRawProcessContext[isp];
}

static int tdm_get_raw_format(int isp_dev){

    int tdm_lbc_enable = tdm_en_sunxi_read();
    int raw_format_flag = -1;
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp_dev);
    if(tdm_lbc_enable)
    {
        switch (pContext->mbus_code)
        {
            case V4L2_MBUS_FMT_SBGGR8_1X8:
            case V4L2_MBUS_FMT_SGBRG8_1X8:
            case V4L2_MBUS_FMT_SGRBG8_1X8:
            case V4L2_MBUS_FMT_SRGGB8_1X8:
            {
                raw_format_flag = TDM_RAW_DUMP_8BIT;
                break;
            }
            case V4L2_MBUS_FMT_SBGGR10_1X10:
            case V4L2_MBUS_FMT_SGBRG10_1X10:
            case V4L2_MBUS_FMT_SGRBG10_1X10:
            case V4L2_MBUS_FMT_SRGGB10_1X10:
            {
                raw_format_flag = TDM_RAW_DUMP_10BIT;
                break;
            }
            default:
                aloge("fatal error! isp%d tdm raw process type %d is invalid!", isp_dev, pContext->mbus_code);
                break;
        }
    }
    else
    {
        switch (pContext->mbus_code)
        {
            case V4L2_MBUS_FMT_SBGGR8_1X8:
            case V4L2_MBUS_FMT_SGBRG8_1X8:
            case V4L2_MBUS_FMT_SGRBG8_1X8:
            case V4L2_MBUS_FMT_SRGGB8_1X8:
            {
                raw_format_flag = TDM_RAW_DUMP_8BIT_FOR_TOOLS;
                break;
            }
            case V4L2_MBUS_FMT_SBGGR10_1X10:
            case V4L2_MBUS_FMT_SGBRG10_1X10:
            case V4L2_MBUS_FMT_SGRBG10_1X10:
            case V4L2_MBUS_FMT_SRGGB10_1X10:
            {
                raw_format_flag = TDM_RAW_DUMP_10BIT_FOR_TOOLS;
                break;
            }
            default:
                aloge("fatal error! isp%d tdm raw process type %d is invalid!", isp_dev, pContext->mbus_code);
                break;
        }
    }
    return raw_format_flag;
}



static void tdm_raw_data_process_ft_callback(void *param)
{
    int ret = 0;
    struct vin_isp_tdm_event_status *status = (struct vin_isp_tdm_event_status *)param;
    if (NULL == status)
    {
        aloge("fatal error! status is NULL!");
        return;
    }
    int isp = status->dev_id;
    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return;
    }
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp);
    if (pContext->process_enable)
    {
        if (!(0 == pContext->config_param.frame_cnt_min && 0 == pContext->config_param.frame_cnt_max))
        {
            if (pContext->config_param.frame_cnt_min > pContext->process_frame_cnt || pContext->process_frame_cnt >= pContext->config_param.frame_cnt_max)
            {
                goto exit;
            }
        }

        unsigned int tdm_buf_id = status->buf_id;
        unsigned int tdm_buf_size = status->buf_size;
        unsigned int tdm_frm_fill_len = status->fill_len;
        unsigned int tdm_frm_head_len = status->head_len;
        unsigned int width = pContext->config_param.width;
        unsigned int height = pContext->config_param.height;
        unsigned int tdm_type = tdm_get_raw_format(isp);
        alogd("Get tdm raw type [%d]",tdm_type);

        char hightfdstr[128],widthfdstr[128];
        FILE *height_fp = NULL;
        FILE *width_fp = NULL;
        snprintf(hightfdstr, 128, "%s/raw_height", pContext->config_param.tdm_raw_file_path);
        snprintf(widthfdstr, 128, "%s/raw_width", pContext->config_param.tdm_raw_file_path);
        height_fp = fopen(hightfdstr, "w");
        if (NULL == height_fp)
        {
            aloge("fatal error! open %s failed! errno is %d", hightfdstr, errno);
        }
        fprintf(height_fp, "%u", height);
        if (height_fp)
        {
            fclose(height_fp);
            height_fp = NULL;
        }
        width_fp = fopen(widthfdstr, "w");
        if (NULL == width_fp)
        {
            aloge("fatal error! open %s failed! errno is %d", widthfdstr, errno);
        }
        fprintf(width_fp, "%u", width);
        if (width_fp)
        {
            fclose(width_fp);
            width_fp = NULL;
        }

        switch (tdm_type)
        {
            case TDM_RAW_DUMP_8BIT:
            case TDM_RAW_DUMP_10BIT:
            {
                char fdstr[128];
                FILE *fp = NULL;
                snprintf(fdstr, 128, "%s/raw_frame", pContext->config_param.tdm_raw_file_path);
                fp = fopen(fdstr, "w");
                if (NULL == fp)
                {
                    aloge("fatal error! open %s failed! errno is %d", fdstr, errno);
                    break;
                }

                struct vin_isp_tdm_data data;
                memset(&data, 0, sizeof(struct vin_isp_tdm_data));
                data.buf = malloc(tdm_buf_size);
                if (data.buf == NULL)
                {
                    aloge("fatal error! isp%d malloc size is %d error!", isp, tdm_buf_size);
                    fclose(fp);
                    break;
                }
                memset(data.buf, 0, tdm_buf_size);
                data.buf_size = tdm_buf_size;
                data.req_buf_id = tdm_buf_id;
                AW_MPI_ISP_GetTdmData(isp, &data);

                char *tdm_data = data.buf + tdm_frm_fill_len;
                int tdm_data_len = data.buf_size - tdm_frm_fill_len;

                fwrite(tdm_data, tdm_data_len, 1, fp);

                alogd("isp%d save tdm raw file success, tdm_buf id:%d size:%d, tdm_frm fill_len:%d head_len:%d",
                    isp, tdm_buf_id, tdm_buf_size, tdm_frm_fill_len, tdm_frm_head_len);

                if (data.buf)
                {
                    free(data.buf);
                    data.buf = NULL;
                    data.buf_size = 0;
                }
                if (fp)
                {
                    fclose(fp);
                    fp = NULL;
                }
                DeleteFiles(pContext->flagpath);
                tdm_raw_process_ft_stop(isp);
                break;
            }
            case TDM_RAW_DUMP_8BIT_FOR_TOOLS:
            {
                char fdstr[128];
                FILE *fp = NULL;
                snprintf(fdstr, 128, "%s/raw_frame", pContext->config_param.tdm_raw_file_path);
                fp = fopen(fdstr, "w");
                if (NULL == fp)
                {
                    aloge("fatal error! open %s failed! errno is %d", fdstr, errno);
                    break;
                }

                struct vin_isp_tdm_data data;
                memset(&data, 0, sizeof(struct vin_isp_tdm_data));
                data.buf = malloc(tdm_buf_size);
                if (data.buf == NULL)
                {
                    aloge("fatal error! isp%d malloc size is %d error!", isp, tdm_buf_size);
                    fclose(fp);
                    break;
                }
                memset(data.buf, 0, tdm_buf_size);
                data.buf_size = tdm_buf_size;
                data.req_buf_id = tdm_buf_id;
                AW_MPI_ISP_GetTdmData(isp, &data);

                char *tdm_data = data.buf + tdm_frm_fill_len + tdm_frm_head_len;
                int tdm_data_len = width*height;

                fwrite(tdm_data, tdm_data_len, 1, fp);

                alogd("isp%d save tdm raw file success, tdm_buf id:%d size:%d, tdm_frm fill_len:%d head_len:%d",
                    isp, tdm_buf_id, tdm_buf_size, tdm_frm_fill_len, tdm_frm_head_len);

                if (data.buf)
                {
                    free(data.buf);
                    data.buf = NULL;
                    data.buf_size = 0;
                }
                if (fp)
                {
                    fclose(fp);
                    fp = NULL;
                }
                DeleteFiles(pContext->flagpath);
                tdm_raw_process_ft_stop(isp);
                break;
            }
            case TDM_RAW_DUMP_10BIT_FOR_TOOLS:
            {
                char fdstr[128];
                FILE *fp = NULL;
                snprintf(fdstr, 128, "%s/raw_frame", pContext->config_param.tdm_raw_file_path);
                aloge("save raw path is %s\n",fdstr);
                fp = fopen(fdstr, "w");
                if (NULL == fp)
                {
                    aloge("fatal error! open %s failed! errno is %d", fdstr, errno);
                    break;
                }

                struct vin_isp_tdm_data data;
                memset(&data, 0, sizeof(struct vin_isp_tdm_data));
                data.buf = malloc(tdm_buf_size);
                if (data.buf == NULL)
                {
                    aloge("fatal error! isp%d malloc size is %d error!", isp, tdm_buf_size);
                    fclose(fp);
                    break;
                }
                memset(data.buf, 0, tdm_buf_size);
                data.buf_size = tdm_buf_size;
                data.req_buf_id = tdm_buf_id;
                AW_MPI_ISP_GetTdmData(isp, &data);

                char *tdm_data = data.buf + tdm_frm_fill_len + tdm_frm_head_len;
                int tdm_data_len = data.buf_size - tdm_frm_fill_len - tdm_frm_head_len;

                int buf_size = width*height*2;
                unsigned short *buf = (unsigned short *)malloc(buf_size);
                if (buf == NULL)
                {
                    aloge("fatal error! isp%d malloc size is %d error!", isp, buf_size);
                    free(data.buf);
                    fclose(fp);
                    break;
                }
                memset(buf, 0, buf_size);

                int stride = (((width * 10) + 511) & ~511)/8;;
                for (int h = 0; h < height; h++)
                {
                    for (int w = 0; w < stride/5; w++)
                    {
                        unsigned short p0 = tdm_data[stride*h + w*5 + 0];
                        unsigned short p1 = tdm_data[stride*h + w*5 + 1];
                        unsigned short p2 = tdm_data[stride*h + w*5 + 2];
                        unsigned short p3 = tdm_data[stride*h + w*5 + 3];
                        unsigned short p4 = tdm_data[stride*h + w*5 + 4];
                        buf[width*h + w*4 + 0] = (p0 << 2) + (p4 >> 6);
                        buf[width*h + w*4 + 1] = (p1 << 2) + ((p4 & 0x3f) >> 4);
                        buf[width*h + w*4 + 2] = (p2 << 2) + ((p4 & 0xf) >> 2);
                        buf[width*h + w*4 + 3] = (p3 << 2) + (p4 & 0x3);
                    }
                }

                fwrite(buf, buf_size, 1, fp);

                alogv("isp%d save tdm raw file success, tdm_buf id:%d size:%d, tdm_frm fill_len:%d head_len:%d",
                    isp, tdm_buf_id, tdm_buf_size, tdm_frm_fill_len, tdm_frm_head_len);

                if (buf)
                {
                    free(buf);
                    buf = NULL;
                    buf_size = 0;
                }
                if (data.buf)
                {
                    free(data.buf);
                    data.buf = NULL;
                    data.buf_size = 0;
                }
                if (fp)
                {
                    fclose(fp);
                    fp = NULL;
                }
                DeleteFiles(pContext->flagpath);
                tdm_raw_process_ft_stop(isp);
                break;
            }
            default:
                aloge("fatal error! isp%d tdm raw process type %d is invalid!", isp, pContext->config_param.mbus_code);
                break;
        }
    }

exit:
    pContext->process_frame_cnt++;

    AW_MPI_ISP_ReturnTdmBuf(isp, status);

    return;
}

int tdm_raw_process_ft_open(int isp, tdm_raw_process_config_param *param)
{
    int ret = 0;
    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return -1;
    }

    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp);
    memset(pContext, 0, sizeof(tdm_raw_process_context));
    pContext->mIsp = isp;
    memcpy(&pContext->config_param, param, sizeof(tdm_raw_process_config_param));
    pContext->mbus_code = pContext->config_param.mbus_code;
    strncpy(pContext->flagpath, pContext->config_param.tdm_raw_flag_path, sizeof(pContext->config_param.tdm_raw_flag_path));

    alogd("isp%d open process tdm raw data, mbus_code:%d, path:%s, w:%d, h:%d, frame_cnt min:%d max:%d", isp,
        pContext->config_param.mbus_code, pContext->config_param.tdm_raw_file_path, pContext->config_param.width,
        pContext->config_param.height, pContext->config_param.frame_cnt_min, pContext->config_param.frame_cnt_max);

    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, &tdm_raw_data_process_ft_callback);

    return ret;
}

int tdm_raw_process_ft_start(int isp, tdm_raw_process_config_param *param)
{
    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return -1;
    }
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp);
    strncpy(pContext->config_param.tdm_raw_file_path, param->tdm_raw_file_path, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
    pContext->process_frame_cnt = 0;
    pContext->process_enable = 1;
    alogd("isp%d start process tdm raw data", isp);

    return 0;
}

int tdm_raw_process_ft_stop(int isp)
{
    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return -1;
    }
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp);

    pContext->process_enable = 0;

    alogd("isp%d stop process tdm raw data", isp);

    return 0;
}

int tdm_raw_process_ft_close(int isp)
{
    int ret = 0;

    if (isp >= TDM_RAW_PROCESS_NUM_MAX)
    {
        aloge("fatal error! invalid isp id %d >= %d\n", isp, TDM_RAW_PROCESS_NUM_MAX);
        return -1;
    }
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(isp);

    alogd("isp%d close process tdm raw data", isp);

    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, NULL);

    if (pContext->tdm_buf)
    {
        free(pContext->tdm_buf);
        pContext->tdm_buf = NULL;
        pContext->tdm_buf_size = 0;
    }

    if (pContext->tdm_raw_fp)
    {
        fclose(pContext->tdm_raw_fp);
        pContext->tdm_raw_fp = NULL;
    }

    return ret;
}


static void *SaveTdmRawDataThread(void *pThreadData)
{
    RawInfo *pRawInfo = (RawInfo*)pThreadData;
    message_t stCmdMsg;
    struct tdm_raw_process_context *pContext = tdm_raw_process_get_context(pRawInfo->isp_id);
    int ret = -1;
    int tdm_time = 0;
    alogd("pRawInfo->flagpath is %s\n",pRawInfo->flagpath);
    while(1)
    {
    PROCESS_MESSAGE:
        if (get_message(pRawInfo->pMsgQueue, &stCmdMsg) == 0)
        {
            if (Stop == stCmdMsg.command)
            {
                ret = tdm_raw_process_ft_close(pRawInfo->isp_id);
                break;
            }
            else
            {
                aloge("fatal error! unknown command:0x%x", stCmdMsg.command);
            }
            goto PROCESS_MESSAGE;
        }
        if((access(pRawInfo->flagpath, F_OK) == 0) &&(!pContext->process_enable))
        {
            if(tdm_time == 0)
            {
                tdm_raw_process_ft_open(pRawInfo->isp_id, &pRawInfo->tdm_raw_config_param);
                tdm_time++;
            }
            alogd("start to cap raw!\n");
            char *Datapath = ReadFlagPath(pRawInfo->flagpath);
            if (NULL == Datapath)
            {
                aloge("fatal error! Datapath is NULL!");
                usleep(100*1000);
                continue;
            }
            Datapath[strcspn(Datapath, "\n")] = 0;
            if (strlen(Datapath)+1 <= MAX_LEN)
            {
                strncpy(pRawInfo->framepath, Datapath, strlen(Datapath)+1);
            }
            else
            {
                aloge("fatal error! Datapath size %d is too long!", strlen(Datapath)+1);
            }
            strncpy(pRawInfo->tdm_raw_config_param.tdm_raw_file_path, pRawInfo->framepath, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);

            ret = tdm_raw_process_ft_start(pRawInfo->isp_id, &pRawInfo->tdm_raw_config_param);
            if(ret == 0)
            {
                alogd("RAW data written to the file successfully");
            }
            else
            {
                alogd("RAW data written to the file failed");
            }
            if (Datapath)
            {
                free(Datapath);
                Datapath = NULL;
            }
            usleep(100*1000);
        }
        else
        {
            TMessage_WaitQueueNotEmpty(pRawInfo->pMsgQueue, 500);
        }
    }
    if (pRawInfo)
    {
        free(pRawInfo);
    }
    return (void*)ret;
}

pthread_t SendRawToApp(struct sensor_config *stConfig, int isp_id, message_queue_t* msg_que)
{
    int result = -1;
    int tdm_lbc_enable = -1;
    pthread_t mkdirpathTrd;

    RawInfo *pRawInfo = (RawInfo*)malloc(sizeof(RawInfo));
    memset(pRawInfo, 0, sizeof(RawInfo));
    pRawInfo->isp_id = isp_id;
    pRawInfo->width = stConfig->width;
    pRawInfo->height = stConfig->height;
    pRawInfo->fps = stConfig->fps_fixed;
    pRawInfo->wdr = stConfig->wdr_mode;

    strncpy(pRawInfo->fixedflagpath, FIXEDFLAGPATH, TDM_RAW_PROCESS_FILE_PATH_MAX_LEN);
    sprintf(pRawInfo->isp_cfg_path, "/tmp/isp%d_%d_%d_%d_%d/", isp_id, stConfig->width, stConfig->height,stConfig->fps_fixed,stConfig->wdr_mode);
    strncpy(pRawInfo->flagpath, pRawInfo->isp_cfg_path, sizeof(pRawInfo->isp_cfg_path));
    strcat(pRawInfo->flagpath, pRawInfo->fixedflagpath);
    strncpy(pRawInfo->tdm_raw_config_param.tdm_raw_flag_path, pRawInfo->flagpath, strlen(pRawInfo->flagpath)+1);

    pRawInfo->tdm_raw_config_param.width = pRawInfo->width;
    pRawInfo->tdm_raw_config_param.height = pRawInfo->height;
    pRawInfo->tdm_raw_config_param.frame_cnt_min = 0;
    pRawInfo->tdm_raw_config_param.frame_cnt_max = 1;
    pRawInfo->tdm_raw_config_param.mbus_code = stConfig->mbus_code;
    pRawInfo->pMsgQueue = msg_que;

    result = pthread_create(&mkdirpathTrd, NULL, SaveTdmRawDataThread, (void *)pRawInfo);
    if (result != 0)
    {
        aloge("fatal error! pthread create fail[%d]", result);
    }

    return mkdirpathTrd;
}