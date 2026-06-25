
#include <sys/stat.h>

#include <utils/plat_log.h>

#include <mpi_videoformat_conversion.h>

#include <isp.h>

#include "ISPCapsYUVData.h"

static int SaveFrameData(VIDEO_FRAME_INFO_S *pstFrameInfo, char *framepath)
{
    VIDEO_FRAME_INFO_S *pSrc = pstFrameInfo;
    unsigned int nFrmsize[3] = {0,0,0};
    unsigned int dataWidth  = pSrc->VFrame.mOffsetRight - pSrc->VFrame.mOffsetLeft;
    unsigned int dataHeight = pSrc->VFrame.mOffsetBottom - pSrc->VFrame.mOffsetTop;
    alogv("dataWidth:%d, dataHeight:%d", dataWidth, dataHeight);

    switch(pSrc->VFrame.mPixelFormat)
    {
        case MM_PIXEL_FORMAT_YUV_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420:
        case MM_PIXEL_FORMAT_AW_NV21M:
            nFrmsize[0] = dataWidth*dataHeight;
            nFrmsize[1] = dataWidth*dataHeight/2;
            nFrmsize[2] = 0;
            break;
        case MM_PIXEL_FORMAT_YVU_PLANAR_420:
        case MM_PIXEL_FORMAT_YUV_PLANAR_420:
            nFrmsize[0] = dataWidth*dataHeight;
            nFrmsize[1] = dataWidth*dataHeight/4;
            nFrmsize[2] = dataWidth*dataHeight/4;
            break;
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_0X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_5X:
        case MM_PIXEL_FORMAT_YUV_AW_LBC_1_0X:
            nFrmsize[0] = pSrc->VFrame.mStride[0];
            nFrmsize[1] = pSrc->VFrame.mStride[1];
            nFrmsize[2] = pSrc->VFrame.mStride[2];
            break;
        default:
            aloge("fatal error! not support pixel format[0x%x]", pSrc->VFrame.mPixelFormat);
            return -1;
    }

    int nlen = 0;
    int mlen = 0;
    FILE* fp = fopen (framepath, "w+");
    if (fp != NULL)
    {
        for (int i = 0; i < 3; i++)
        {
            if (pSrc->VFrame.mpVirAddr[i])
            {
                nlen += fwrite(pSrc->VFrame.mpVirAddr[i], 1, nFrmsize[i], fp);
                mlen += nFrmsize[i];
            }
        }
        if(nlen != mlen)
        {
            aloge("fatal error! fwrite fail, write len[%d], frm len[%d]", nlen, mlen);
        }
        alogd("length[%d]", mlen);
        fclose(fp);
        alogd("store raw frame in file[%s]", framepath);
    }
    else
    {
        aloge("fatal error! open file[%s] fail!", framepath);
    }
    return 0;
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

static void CreateWHFFiles(VIDEO_FRAME_INFO_S *pstFrameInfo, char *widthpath, char *heightpath, char *formatpath)
{
    FILE *fpw, *fph, *fpf;
    fpw = fopen (widthpath, "w+");
    if (fpw == NULL)
    {
        aloge("fatal error! open file[%s] fail!", widthpath);
    }
    fprintf(fpw, "%d", pstFrameInfo->VFrame.mWidth);

    fph = fopen (heightpath, "w+");
    if (fph == NULL)
    {
        aloge("fatal error! open file[%s] fail!", heightpath);
    }
    fprintf(fph, "%d", pstFrameInfo->VFrame.mHeight);

    fpf = fopen (formatpath, "w+");
    if (fph == NULL)
    {
        aloge("fatal error! open file[%s] fail!", formatpath);
    }
    int formatnum = map_PIXEL_FORMAT_E_to_V4L2_PIX_FMT(pstFrameInfo->VFrame.mPixelFormat);
    fprintf(fpf, "%d", formatnum);

    fclose(fpw);
    fclose(fph);
    fclose(fpf);
}

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

static void mkdirs(char *Datapath)
{
    char str[MAX_LEN];
    strncpy(str, Datapath, MAX_LEN);
    int len = strlen(str);
    for(int i = 0; i < len; i++)
    {
        if(str[i] == '/')
        {
            str[i] = '\0';
            if(access(str, 0) != 0)
            {
                mkdir(str, 0777);
            }
            str[i] =  '/';
        }
    }
    if(len > 0 && access(str, 0) !=  0)
    {
        mkdir(str, 0777);
    }
}

static void CreateWHFPath(VIDEO_FRAME_INFO_S *pstFrameInfo, char* Datapath)
{
    char widthpath[MAX_LEN] = "";
    char heightpath[MAX_LEN] = "";
    char formatpath[MAX_LEN] = "";
    strncpy(widthpath, Datapath, strlen(Datapath)+1);
    strncpy(heightpath, Datapath, strlen(Datapath)+1);
    strncpy(formatpath, Datapath, strlen(Datapath)+1);

    char fixedwidthpath[7] = "/width";
    char fixedheightpath[8] = "/height";
    char fixedformatpath[9] = "/format";
    strcat(widthpath, fixedwidthpath);
    strcat(heightpath, fixedheightpath);
    strcat(formatpath, fixedformatpath);

    if ((access(widthpath, F_OK) != 0) && (access(heightpath, F_OK) != 0)  && (access(formatpath, F_OK) != 0))//判断高宽文件是否存在
    {
        CreateWHFFiles(pstFrameInfo, widthpath, heightpath, formatpath);
    }
    else
    {
        aloge("file already exists!");
    }
}

int SendYuvToApp(VIDEO_FRAME_INFO_S *pstFrameInfo, viChnManager *pManager, int vipp_id)
{
    int result = -1;
    ISP_DEV isp_id = 0;

    char flagpath[MAX_LEN] = "";
    char isp_cfg_path[MAX_LEN] = "";
    char capturepath[MAX_LEN] = "";
    char framepath[MAX_LEN] = "";

    struct sensor_config stConfig;
    memset(&stConfig, 0, sizeof(struct sensor_config));

    if(pstFrameInfo == NULL)
    {
        aloge("fatal error! pointer is illegal!");
        return result;
    }

    if(pManager == NULL)
    {
        aloge("fatal error! pointer is illegal!");
        return result;
    }

    videoInputHw_GetIspDev(vipp_id, &isp_id);
    if(isp_get_sensor_info(isp_id, &stConfig) == -1)
    {
        return result;
    }
    sprintf(isp_cfg_path, "/tmp/isp%d_%d_%d_%d_%d/", isp_id, stConfig.width, stConfig.height, stConfig.fps_fixed, stConfig.wdr_mode);
    strncpy(capturepath, isp_cfg_path, strlen(isp_cfg_path)+1);
    char fixedcapspath[8] = "capture";
    strcat(capturepath, fixedcapspath);

    if (access(capturepath, F_OK) == 0)
    {
        strncpy(flagpath, isp_cfg_path, sizeof(isp_cfg_path));
        char fixedflagpath[13] = "capture/flag";
        strcat(flagpath, fixedflagpath);

        if (access(flagpath, F_OK) == 0)
        {
            char *Datapath = ReadFlagPath(flagpath);
            if (NULL == Datapath)
            {
                aloge("fatal error! Datapath is NULL!");
                return -1;
            }
            Datapath[strcspn(Datapath, "\n")] = 0;
            if(access(Datapath, F_OK) != 0)
            {
               mkdirs(Datapath);
            }
            if (strlen(Datapath)+1 <= MAX_LEN)
            {
                strncpy(framepath, Datapath, strlen(Datapath)+1);
            }
            else
            {
                aloge("fatal error! Datapath size %d is too long!", strlen(Datapath)+1);
            }

            char fixedframepath[7] = "/frame";
            strcat(framepath, fixedframepath);

            CreateWHFPath(pstFrameInfo, Datapath);
            SaveFrameData(pstFrameInfo, framepath);
            alogd("YUV data written to the file successfully");

            if (Datapath)
            {
                free(Datapath);
                Datapath = NULL;
            }

            DeleteFiles(flagpath);
            result = 0;
        }
        /*else
        {
            aloge("flag file does not exist");
        }*/
    }
    /*else
    {
        aloge("ISP file does not exist");
        return result;
    }*/
    return result;
}
