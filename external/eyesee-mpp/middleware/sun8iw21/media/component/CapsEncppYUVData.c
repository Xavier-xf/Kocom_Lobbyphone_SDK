/******************************************************************************
  Copyright (C), 2001-2023, Allwinner Tech. Co., Ltd.
 ******************************************************************************
  File Name     : CapsEncppYUVData.c
  Version       : Initial Draft
  Author        : Allwinner
  Created       : 2023/01/30
  Last Modified :
  Description   : capture encpp yuv data function
  Function List :
  History       :
******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <utils/plat_log.h>
#include "videoInputHw.h"
#include <ConfigOption.h>

#include "CapsEncppYUVData.h"

#define MAX_LEN 301

typedef struct ISP_INFO_S
{
    ISP_DEV isp_id;
    int isp_width;
    int isp_height;
    int isp_fps;
    int isp_wdr;
    int srcwidth;
    int srcheight;
    char Framepath[MAX_LEN];
    char flagpath[MAX_LEN];
    char widthpath[MAX_LEN];
    char heightpath[MAX_LEN];
    char encpppath[MAX_LEN];
    BOOL flag;
}ISP_INFO_S;

static int DeleteFiles(char *filename)
{
    if (remove(filename) == 0)
    {
        alogd("Delete the flag file %s success", filename);
        return 0;
    }
    else
    {
        aloge("Failed to delete flag file %s", filename);
        return -1;
    }
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

static int ReadNum(char *path)
{
    int num = 0;
    FILE *pathfile = fopen(path, "r");
    if (pathfile == NULL)
    {
        aloge("fatal error! open file[%s] fail! errno is %s", path, errno);
        return -1;
    }
    while (!feof(pathfile))
    {
        fscanf(pathfile, "%d", &num);
    }
    fclose(pathfile);
    return num;
}

static int ReadFileNameInfo(char *filepath,  ISP_INFO_S *stispinfo)
{
    DIR *dir = NULL;
    struct dirent *ent = NULL;
    char filename[MAX_LEN] = "";
    char tmpname[6] = "/tmp/";
    int ispnum = 0;
    ISP_DEV isp_id_num = -1;

    if((dir = opendir("/tmp")) != NULL)
    {
        while((ent = readdir(dir)) != NULL)
        {
            if(strstr(ent->d_name, "isp"))
            {
                ispnum++;
                sscanf(ent->d_name, "isp%d_%d_%d_%d_%d", &isp_id_num, &stispinfo->isp_width, &stispinfo->isp_height, &stispinfo->isp_fps, &stispinfo->isp_wdr);
                if(isp_id_num == stispinfo->isp_id)
                {
                    //alogd("isp_id[%d] match", isp_id_num);
                    strcpy(filename, ent->d_name);
                    break;
                }
            }
        }
        closedir(dir);

        if(ispnum == 0)
        {
            //alogd("isp file does not exist");
            return -1;
        }
    }
    else
    {
        //alogd("tmp directory does not exist");
        return -1;
    }
    if(strlen(filename) == 0)
    {
        alogv("No file with the name isp_xxx was found in the /tmp directory");
        return -1;
    }
    strcpy(filepath, tmpname);
    strcat(filepath, filename);
    return 0;
}

static int GetFileContent(VIDEOENCDATATYPE *pVideoEncData, ISP_INFO_S *stispinfo)
{
    char isp_cfg_path[MAX_LEN] = "";
    int ret = ReadFileNameInfo(isp_cfg_path, stispinfo);
    if(ret == -1)
    {
        //alogd("ReadFileNameInfo failed!!!");
        return -1;
    }

    strncpy(stispinfo->encpppath, isp_cfg_path, sizeof(isp_cfg_path));
    char fixedencpppath[7] = "/encpp";
    strcat(stispinfo->encpppath, fixedencpppath);

    strncpy(stispinfo->flagpath, stispinfo->encpppath, strlen(stispinfo->encpppath)+1);
    strncpy(stispinfo->widthpath, stispinfo->encpppath, strlen(stispinfo->encpppath)+1);
    strncpy(stispinfo->heightpath, stispinfo->encpppath, strlen(stispinfo->encpppath)+1);
    char fixedflagpath[12] = "/encpp_flag";
    char fixedwidthpath[13] = "/encpp_width";
    char fixedheightpath[14] = "/encpp_height";
    strcat(stispinfo->flagpath, fixedflagpath);
    strcat(stispinfo->widthpath, fixedwidthpath);
    strcat(stispinfo->heightpath, fixedheightpath);

    if ((access(stispinfo->encpppath, F_OK) != 0) || (access(stispinfo->flagpath, F_OK) != 0))
    {
        return -1;
    }
    stispinfo->srcwidth = ReadNum(stispinfo->widthpath);
    stispinfo->srcheight = ReadNum(stispinfo->heightpath);

    if (0 == stispinfo->srcwidth || 0 == stispinfo->srcheight)
    {
        aloge("fatal error! invalid width %d or height %d", stispinfo->srcwidth, stispinfo->srcheight);
        return -1;
    }

    char *Datapath = ReadFlagPath(stispinfo->flagpath);
    if (NULL == Datapath)
    {
        aloge("fatal error! Datapath is NULL!");
        return -1;
    }
    Datapath[strcspn(Datapath, "\n")] = 0;
    if (access(Datapath, F_OK) != 0)
    {
        mkdirs(Datapath);
    }

    if (strlen(Datapath)+1 <= MAX_LEN)
    {
        strncpy(stispinfo->Framepath, Datapath, strlen(Datapath)+1);
    }
    else
    {
        aloge("fatal error! Datapath size %d is too long!", strlen(Datapath)+1);
    }

    if (Datapath)
    {
        free(Datapath);
        Datapath = NULL;
    }

    char fixeframepppath[13] = "/encpp_frame";
    strcat(stispinfo->Framepath, fixeframepppath);

    if(stispinfo->isp_height < stispinfo->srcheight || stispinfo->isp_width < stispinfo->srcwidth)
    {
        aloge("fatal error! isp_height:%d,srcheight:%d,isp_width:%d,srcwidth:%d", stispinfo->isp_height, stispinfo->srcheight, stispinfo->isp_width, stispinfo->srcwidth);
        return -1;
    }
    if(pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight < stispinfo->srcheight || pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth < stispinfo->srcwidth)
    {
        aloge("fatal error! SrcPicHeight:%d,srcheight:%d,SrcPicWidth:%d,stispinfo->srcwidth:%d ", pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight, stispinfo->srcheight, pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth, stispinfo->srcwidth);
        return -1;
    }

    if (FALSE == pVideoEncData->mbWbYuvEnable)
    {
        struct sWbYuvParam stWbYuvParam;
        memset(&stWbYuvParam, 0, sizeof(sWbYuvParam));
        stWbYuvParam.bEnableWbYuv = 1;
        stWbYuvParam.nWbBufferNum = 1;
        stWbYuvParam.scalerRatio = VENC_ISP_SCALER_0;
       // if(pVideoEncData->mEncChnAttr.VeAttr.SrcPicHeight != stispinfo->srcheight || pVideoEncData->mEncChnAttr.VeAttr.SrcPicWidth != stispinfo->srcwidth)
        //{
            stWbYuvParam.bEnableCrop = 1;
            stWbYuvParam.sWbYuvcrop.nHeight = stispinfo->srcheight;
            stWbYuvParam.sWbYuvcrop.nWidth = stispinfo->srcwidth;
            int ntop = AWALIGN(stispinfo->isp_height, 16)/2 - stWbYuvParam.sWbYuvcrop.nHeight/2;
            int nleft = AWALIGN(stispinfo->isp_width, 16)/2 - stWbYuvParam.sWbYuvcrop.nWidth/2;
            stWbYuvParam.sWbYuvcrop.nTop = (ntop >> 4) << 4;
            stWbYuvParam.sWbYuvcrop.nLeft = (nleft >> 4) << 4;
        //}
        alogd("VenChn[%d],vippid[%d],isp_id[%d],srcwidth = %d,srcheight = %d",pVideoEncData->mMppChnInfo.mChnId,pVideoEncData->mvipp_id,stispinfo->isp_id,stispinfo->srcwidth,stispinfo->srcheight);

        if (0 == VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEnableWbYuv, (void*)&stWbYuvParam))
        {
            pVideoEncData->mbWbYuvEnable = TRUE;
        }
        else
        {
            aloge("fatal error! VencChn[%d] enable WbYuv fail!", pVideoEncData->mMppChnInfo.mChnId);
            ret = -1;
        }
    }

    if (TRUE == pVideoEncData->mbWbYuvEnable)
    {
        FILE* fp_wb_yuv = fopen(stispinfo->Framepath, "wb");
        if (NULL == fp_wb_yuv)
        {
            aloge("fatal error! open file %s fail? errno is %d", stispinfo->Framepath, errno);
            return -1;
        }
        struct VencThumbInfo stThumbInfo;
        memset(&stThumbInfo, 0, sizeof(VencThumbInfo));
        stThumbInfo.bWriteToFile = 1;
        stThumbInfo.fp = fp_wb_yuv;
        if (0 == VencGetParameter(pVideoEncData->pCedarV, VENC_IndexParamGetThumbYUV, (void*)&stThumbInfo))
        {
            alogd("VenChn[%d] get WbYuv success, save to file %s, delete flag file %s.", pVideoEncData->mMppChnInfo.mChnId, stispinfo->Framepath, stispinfo->flagpath);
            ret = 0;
            DeleteFiles(stispinfo->flagpath);
            pVideoEncData->mbWbYuvEnable = FALSE;
            struct sWbYuvParam stWbYuvParam;
            memset(&stWbYuvParam, 0, sizeof(sWbYuvParam));
            if (VencSetParameter(pVideoEncData->pCedarV, VENC_IndexParamEnableWbYuv, (void*)&stWbYuvParam))
            {
                aloge("fatal error! VenChn[%d] disable WbYuv fail!", pVideoEncData->mMppChnInfo.mChnId);
                ret = -1;
            }
        }
        else
        {
            alogv("fatal error! VenChn[%d] get WbYuv fail!", pVideoEncData->mMppChnInfo.mChnId);
            ret = -1;
        }
        fclose(fp_wb_yuv);
    }
    return ret;
}

/**
 * Directory Structure:
 * /tmp/ispx_xxx/encpp/encpp_flag
 * /tmp/ispx_xxx/encpp/encpp_width
 * /tmp/ispx_xxx/encpp/encpp_height
 */
int SendCropYuvToApp(VIDEOENCDATATYPE *pVideoEncData)
{
    int result = -1;
    ISP_DEV isp_id = 0;
    int ret = -1;

    if(pVideoEncData->mvipp_id == -1)
    {
        return ret;
    }

    struct ISP_INFO_S stispinfo;
    memset(&stispinfo, 0, sizeof(struct ISP_INFO_S));
    /* if user don't enable vi component, isp id default 0 */
#if (MPPCFG_VI == OPTION_VI_ENABLE)
    videoInputHw_GetIspDev(pVideoEncData->mvipp_id, &isp_id);
#endif
    if (access("/tmp", F_OK) != 0)
    {
        return result;
    }
    stispinfo.isp_id = isp_id;
    ret = GetFileContent(pVideoEncData, &stispinfo);
    return ret;
}

