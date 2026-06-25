#include <pthread.h>
#include <sys/time.h>

#include <awaiisp.h>
#include <ion_mem_alloc.h>
#include "list.h"
#include "confparser.h"

#include "awaiisp_demo.h"
#include "awaiisp_demo_config.h"

static demo_awaiisp_common_context *gDemoAiIspCommonContext = NULL;

#ifndef __linux__
struct rt_memheap aiisp_demo_mempool;
#define AIISP_MEMRESERVE_DTS 0x45100000
#define AIISP_MEMRESERVE 0x48200000
#define AIISP_MEMRESERVE_SIZE 0x2800000

#define MODEL_HEADER_ADDR 0x42000000
#define DATA_RESERVE_ADDR 0x42300000
#define NR_MODEL "aiisp_model"
#define LUT_MODEL "aiisp_lut_model"
#define ISP_DATA "aiisp_data"
#define MAX_MODEL_COUNT 10
#define MAX_MODEL_NAME_LENGTH 16
typedef struct ModelInfo {
    int model_count;
    char model_name[MAX_MODEL_COUNT][MAX_MODEL_NAME_LENGTH];
    int model_size[MAX_MODEL_COUNT];
    uint32_t model_start_address[MAX_MODEL_COUNT];
} ModelInfo;
#endif

static int ParseCmdLine(int argc, char **argv, DemoCmdLineParam *pCmdLinePara)
{
    awaiisp_common_dbg_print("path:[%s], arg number is [%d]", argv[0], argc);
    int ret = 0;
#ifdef __linux__
    int i=1;
    memset(pCmdLinePara, 0, sizeof(DemoCmdLineParam));
    while(i < argc)
    {
        if(!strcmp(argv[i], "-path"))
        {
            if(++i >= argc)
            {
                awaiisp_common_err_print("fatal error! use -h to learn how to set parameter!!!");
                ret = -1;
                break;
            }
            if(strlen(argv[i]) >= MAX_FILE_PATH_SIZE)
            {
                awaiisp_common_err_print("fatal error! file path[%s] too long: [%d]>=[%d]!", argv[i], strlen(argv[i]), MAX_FILE_PATH_SIZE);
            }
            strncpy(pCmdLinePara->mConfigFilePath, argv[i], MAX_FILE_PATH_SIZE-1);
            pCmdLinePara->mConfigFilePath[MAX_FILE_PATH_SIZE-1] = '\0';
        }
        else if(!strcmp(argv[i], "-h"))
        {
            awaiisp_common_dbg_print("CmdLine param:\n"
                "\t-path /mnt/extsd/awaiisp_demo.conf\n");
            ret = 1;
            break;
        }
        else
        {
            awaiisp_common_err_print("ignore invalid CmdLine param:[%s], type -h to get how to set parameter!", argv[i]);
        }
        i++;
    }
#else
    awaiisp_common_dbg_print("It is melis now, it will parser cmdline");
    strcpy(pCmdLinePara->mConfigFilePath, "melis aiisp");
#endif

    return ret;
}

static int loadDemoConfig(DemoConfig *pConfig, const char *conf_path)
{
    if (NULL == pConfig)
    {
        awaiisp_common_err_print("fatal error, pConfig is NULL!");
        return -1;
    }

    if (NULL != conf_path)
    {
#ifdef __linux__
        char *ptr = NULL;
        CONFPARSER_S stConfParser;
        int ret = createConfParser(conf_path, &stConfParser);
        if (ret < 0)
        {
            awaiisp_common_err_print("fatal error, load conf failed!");
            return -1;
        }

        ptr = (char*)GetConfParaString(&stConfParser, CFG_DemoAiIspLutNbgFilePath, NULL);
        strncpy(pConfig->mDemoAiIspLutNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_DemoAiIspNbgFilePath, NULL);
        strncpy(pConfig->mDemoAiIspNbgFilePath, ptr, MAX_FILE_PATH_SIZE);
        pConfig->mDemoAiIspModelVersion = GetConfParaInt(&stConfParser, CFG_DemoAiIspModelVersion, 0);
        pConfig->mDemoAiIspWidth = GetConfParaInt(&stConfParser, CFG_DemoAiIspWidth, 0);
        pConfig->mDemoAiIspHeight = GetConfParaInt(&stConfParser, CFG_DemoAiIspHeight, 0);
        pConfig->mDemoAiIspTdmRxBufNum = GetConfParaInt(&stConfParser, CFG_DemoAiIspTdmRxBufNum, 0);
        pConfig->mDemoAiIspMode = GetConfParaInt(&stConfParser, CFG_DemoAiIspMode, 0);
        pConfig->mDemoAiIspReserve0 = GetConfParaInt(&stConfParser, CFG_DemoAiIspReserve0, 0);
        pConfig->mDemoAiIspReserve1 = GetConfParaInt(&stConfParser, CFG_DemoAiIspReserve1, 0);
        pConfig->mDemoAiIspReserve2 = GetConfParaInt(&stConfParser, CFG_DemoAiIspReserve2, 0);
        pConfig->mDemoAiIspFrameRate = GetConfParaInt(&stConfParser, CFG_DemoAiIspFrameRate, 0);
        pConfig->mDemoAiIspSaveAfterFrame = GetConfParaInt(&stConfParser, CFG_DemoAiIspSaveAfterFrame, 0);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_DemoAiIspInputFile, NULL);
        strncpy(pConfig->mDemoAiIspInputFilePath, ptr, MAX_FILE_PATH_SIZE);
        ptr = (char*)GetConfParaString(&stConfParser, CFG_DemoAiIspOutputFile, NULL);
        strncpy(pConfig->mDemoAiIspOutputFilePath, ptr, MAX_FILE_PATH_SIZE);

        awaiisp_common_dbg_print("ModelVersion:%d, Width:%d, Height:%d, RxBufNum:%d, FrameRate:%d, Reserve0:0x%x, Reserve1:0x%x, Reserve2:0x%x\n",
            pConfig->mDemoAiIspModelVersion, pConfig->mDemoAiIspWidth, pConfig->mDemoAiIspHeight, pConfig->mDemoAiIspTdmRxBufNum,
            pConfig->mDemoAiIspFrameRate, pConfig->mDemoAiIspReserve0, pConfig->mDemoAiIspReserve1, pConfig->mDemoAiIspReserve2);

        awaiisp_common_dbg_print("FilePath LutNbg:%s, Nbg:%s, Input:%s, Output:%s\n",
            pConfig->mDemoAiIspLutNbgFilePath, pConfig->mDemoAiIspNbgFilePath,
            pConfig->mDemoAiIspInputFilePath, pConfig->mDemoAiIspOutputFilePath);

        destroyConfParser(&stConfParser);
#else

#define AIISP_WIDTH 1920
#define AIISP_HEIGH 1080
#define AIIsp_TDMRXBUFNUM 5
#define AIISP_MDOE 0
#define AIISP_RESERVE0 0
#define AIISP_FRAMERATE 1
        pConfig->mDemoAiIspWidth = AIISP_WIDTH;
        pConfig->mDemoAiIspHeight = AIISP_HEIGH;
        pConfig->mDemoAiIspTdmRxBufNum = AIIsp_TDMRXBUFNUM;
        pConfig->mDemoAiIspMode = AIISP_MDOE;
        pConfig->mDemoAiIspReserve0 = AIISP_RESERVE0;
        pConfig->mDemoAiIspFrameRate = AIISP_FRAMERATE;
#endif
        awaiisp_common_dbg_print("Width:%d,Height:%d,RxBufNum:%d\n",pConfig->mDemoAiIspWidth,pConfig->mDemoAiIspHeight,pConfig->mDemoAiIspTdmRxBufNum);
    }
    else
    {
        awaiisp_common_err_print("user not set config file\n");
        return -1;
    }

    return 0;
}

static int InitList(demo_awaiisp_common_context *pContext)
{
    int ret = 0;
    int i = 0;
    pContext->isp_id = 0;
#ifdef __linux__
    pContext->pMemops = GetMemAdapterOpsS();
    SunxiMemOpen(pContext->pMemops);
#endif

    pContext->buf_size = pContext->mConfigPara.mDemoAiIspWidth * pContext->mConfigPara.mDemoAiIspHeight + DEMO_AIISP_FILL_LEN + DEMO_AIISP_HEAD_LEN;

    INIT_LIST_HEAD(&pContext->mDemoFrameIdleList);
    INIT_LIST_HEAD(&pContext->mDemoFrameUsedList);

    for (i = 0; i < pContext->mConfigPara.mDemoAiIspTdmRxBufNum; i++)
    {
        DemoFrame *pNode = (DemoFrame*)malloc(sizeof(DemoFrame));
        if (pNode == NULL)
        {
            awaiisp_common_err_print("fatal error! malloc fail\n");
            break;
        }
        memset(pNode, 0, sizeof(DemoFrame));
#ifdef __linux__
        pNode->pvirbuffer = (unsigned char*)SunxiMemPalloc(pContext->pMemops, pContext->buf_size);
#else
        pNode->pvirbuffer = (unsigned char*)rt_memheap_alloc(&aiisp_demo_mempool, pContext->buf_size);
#endif
        if (pNode->pvirbuffer == NULL)
        {
#ifdef __linux__
            SunxiMemPfree(pContext->pMemops, (void *)pNode->pvirbuffer);
#else
            rt_memheap_free(pNode->pvirbuffer);
#endif
            awaiisp_common_err_print("fatal error! malloc failed! size=%d\n", pContext->buf_size);
            return -1;
        }

        pNode->mBufferId = i;
        pNode->datasize = pContext->mConfigPara.mDemoAiIspWidth * pContext->mConfigPara.mDemoAiIspHeight;
        pNode->status.buf_id = (char)i;
        pNode->status.buf_size = pContext->buf_size;
        pNode->status.dev_id = pContext->isp_id;
        pNode->status.fill_len = DEMO_AIISP_FILL_LEN;
        pNode->status.head_len = DEMO_AIISP_HEAD_LEN;
#ifdef __linux__
        pNode->status.iommu_buf = SunxiMemGetPhysicAddressCpu(pContext->pMemops, pNode->pvirbuffer);
#else
        pNode->status.iommu_buf = (pNode->pvirbuffer - AIISP_MEMRESERVE + AIISP_MEMRESERVE_DTS);
        awaiisp_common_dbg_print("pNode[%d]: pvirbuffer:%p, PhysicAddress: %p, buf_size = %d, fill_len = %d, head_len = %d\n",
                i, pNode->pvirbuffer, pNode->status.iommu_buf,
                pNode->status.buf_size, pNode->status.fill_len, pNode->status.head_len);
#endif
        list_add_tail(&pNode->mList, &pContext->mDemoFrameIdleList);
    }

    ret = pthread_mutex_init(&pContext->mDemoFrameListMutex, NULL);
    if(ret != 0)
    {
        awaiisp_common_err_print("fatal error! pthread mutex init fail!\n");
        ret = -1;
    }

    return ret;
}

static int DeInitList(demo_awaiisp_common_context* pContext)
{
    int ret = 0;
    DemoFrame *pNode = NULL;

    pthread_mutex_lock(&pContext->mDemoFrameListMutex);
    if (!list_empty(&pContext->mDemoFrameUsedList))
    {
        awaiisp_common_err_print("fatal error! inputUsedFrame must be 0!");
        list_for_each_entry(pNode, &pContext->mDemoFrameUsedList, mList)
        {
            list_move_tail(&pNode->mList, &pContext->mDemoFrameIdleList);
        }
    }
    if (!list_empty(&pContext->mDemoFrameIdleList))
    {
        DemoFrame *pEntry, *pTmp;
        list_for_each_entry_safe(pEntry, pTmp, &pContext->mDemoFrameIdleList, mList)
        {
            if (pEntry->pvirbuffer != NULL)
            {
#ifdef __linux__
               SunxiMemPfree(pContext->pMemops, pEntry->pvirbuffer);
#else
                rt_memheap_free(pEntry->pvirbuffer);
#endif
            }
            list_del(&pEntry->mList);
            free(pEntry);
        }
    }
    pthread_mutex_unlock(&pContext->mDemoFrameListMutex);

#ifdef __linux__
    SunxiMemClose(pContext->pMemops);
#endif
    ret = pthread_mutex_destroy(&pContext->mDemoFrameListMutex);
    if(ret != 0)
    {
        awaiisp_common_err_print("fatal error! pthread mutex destroy fail!\n");
        ret = -1;
    }

    return ret;
}

static int demo_awaiisp_register_tdm_buffer_callback(int isp, void *callback)
{
    demo_awaiisp_common_context *pContext = gDemoAiIspCommonContext;

    pContext->tdm_buffer_process_callback = callback;

    return 0;
}

static void demo_awaiisp_return_tdm_buffer_callback(struct demo_vin_isp_tdm_event_status *status)
{
    demo_awaiisp_common_context* pContext = gDemoAiIspCommonContext;
    DemoFrame *pNode = NULL;

    pthread_mutex_lock(&pContext->mDemoFrameListMutex);
    if(list_empty(&pContext->mDemoFrameUsedList))
    {
        awaiisp_common_warn_print("The free linked list has been exhausted, please wait\n");
    }
    else
    {
        pNode = list_first_entry(&pContext->mDemoFrameUsedList, DemoFrame, mList);
        if(pContext->recvframecount >= pContext->mConfigPara.mDemoAiIspSaveAfterFrame)
        {
            int offset = pContext->buf_size - pNode->datasize;
#ifdef __linux__
            unsigned char* pVirtaddr = (unsigned char*) SunxiMemGetVirtualAddressCpu(pContext->pMemops, status->iommu_buf);
#else
            unsigned char* pVirtaddr = (unsigned char*)pNode->pvirbuffer;
#endif
            if(pNode->pvirbuffer != pVirtaddr)
            {
                awaiisp_common_warn_print("awaiisp_common_warn_print\n");
            }
#ifdef __linux__
            SunxiMemFlushCache(pContext->pMemops, pVirtaddr, pContext->buf_size);
            size_t count = fwrite(pVirtaddr + offset, 1, pNode->datasize, pContext->outfile);
            if (count != pNode->datasize)
            {
                awaiisp_common_err_print("fWrite error\n");
                pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
                return;
            }
#endif
        }
        list_move_tail(&pNode->mList, &pContext->mDemoFrameIdleList);
        pContext->recvframecount++;
        awaiisp_common_ver_print("recvframecount: %d", pContext->recvframecount);
    }
    pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
}

static void *demo_test_thread(void *pThreadData)
{
    demo_awaiisp_common_context *pContext = (demo_awaiisp_common_context*)pThreadData;
    DemoFrame *pNode = NULL;
    int time = 0;

#ifndef __linux__
    unsigned char *nr_nb = (unsigned char *)DATA_RESERVE_ADDR;
    unsigned int nr_len = 0;
    unsigned char *lut_nb = (unsigned char *)DATA_RESERVE_ADDR;
    unsigned int lut_len = 0;
    unsigned int dataAddress = DATA_RESERVE_ADDR;
    unsigned int dataLen = 0;
    unsigned int i;
    ModelInfo info;
    memcpy(&info, (void *)MODEL_HEADER_ADDR, sizeof(ModelInfo));
    for (i = 0; i < info.model_count; i++) {
        printf("model name: %s 0x%08lx %d\n", info.model_name[i], info.model_start_address[i], info.model_size[i]);
        if (strcmp(info.model_name[i], NR_MODEL) == 0) {
            nr_nb = (unsigned char *)info.model_start_address[i];
            nr_len = info.model_size[i];
        } else if (strcmp(info.model_name[i], LUT_MODEL) == 0) {
            lut_nb = (unsigned char *)info.model_start_address[i];
            lut_len = info.model_size[i];
        } else if (strcmp(info.model_name[i], ISP_DATA) == 0) {
            dataAddress = info.model_start_address[i];
            dataLen = info.model_size[i];
        }
    }
#endif

    while(1)
    {
        pthread_mutex_lock(&pContext->mDemoFrameListMutex);
        if (list_empty(&pContext->mDemoFrameIdleList))
        {
            awaiisp_common_warn_print("The free linked list has been exhausted, please wait\n");
            pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
            usleep(10*1000);
            break;
        }
        else
        {
            pNode = list_first_entry(&pContext->mDemoFrameIdleList, DemoFrame, mList);
            int offset = pContext->buf_size - pNode->datasize;
#ifdef __linux__
            size_t count = fread(pNode->pvirbuffer + offset, 1, pNode->datasize, pContext->infile);
            if (count != pNode->datasize)
            {
                if (feof(pContext->infile))
                {
                    awaiisp_common_dbg_print("reached end of file\n");
                    pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
                    return NULL;
                }
                else
                {
                    awaiisp_common_err_print("fatal error, fread error! count:%d buf_size:%d\n", count, pNode->datasize);
                    pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
                    return NULL;
                }
            }
            SunxiMemFlushCache(pContext->pMemops, pNode->pvirbuffer, pContext->buf_size);
#else
            memcpy(pNode->pvirbuffer + offset, (unsigned char*)(size_t )dataAddress, pNode->datasize);
#endif
            if (pContext->tdm_buffer_process_callback)
            {
                pContext->sendframecount++;
                awaiisp_common_dbg_print("sendframecount :%d, pNode: pvirbuffer = %p, iommu_buf = %p\n",
                    pContext->sendframecount, pNode->pvirbuffer, pNode->status.iommu_buf);
                pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
                pContext->tdm_buffer_process_callback(&pNode->status);
                pthread_mutex_lock(&pContext->mDemoFrameListMutex);
            }

            list_move_tail(&pNode->mList, &pContext->mDemoFrameUsedList);
            pthread_mutex_unlock(&pContext->mDemoFrameListMutex);
        }

        if (pContext->mConfigPara.mDemoAiIspFrameRate)
            time = 1000 / pContext->mConfigPara.mDemoAiIspFrameRate;
        else
            time = 100;
        usleep(time * 1000);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    int ret = 0;

#ifndef __linux__
    awaiisp_common_dbg_print("rt_memheap_init aiisp_demo_mempool\n");
    simple_iommu_map_region(0x48200000, AIISP_MEMRESERVE_DTS, AIISP_MEMRESERVE_SIZE);
	rt_memheap_init(&aiisp_demo_mempool, "aiisp_demo-mempool", (void *)AIISP_MEMRESERVE, AIISP_MEMRESERVE_SIZE);
#endif

#ifdef __linux__
    demo_awaiisp_common_context *pContext = (demo_awaiisp_common_context*)malloc(sizeof(demo_awaiisp_common_context));
#else
    demo_awaiisp_common_context *pContext = (demo_awaiisp_common_context*)rt_memheap_alloc(&aiisp_demo_mempool, sizeof(demo_awaiisp_common_context));
#endif
    if (NULL == pContext)
    {
        awaiisp_common_err_print("fatal error! malloc pContext failed! size=%d\n", sizeof(demo_awaiisp_common_context));
        return -1;
    }
    memset(pContext, 0, sizeof(demo_awaiisp_common_context));
    gDemoAiIspCommonContext = pContext;

    if(ParseCmdLine(argc, argv, &pContext->mCmdLinePara) != 0)
    {
        awaiisp_common_err_print("fatal error! command line param is wrong, exit!\n");
        return -1;
    }

    char *pConfigFilePath;
    if(strlen(pContext->mCmdLinePara.mConfigFilePath) > 0)
    {
        pConfigFilePath = pContext->mCmdLinePara.mConfigFilePath;
    }
    else
    {
        pConfigFilePath = NULL;
    }

    if(loadDemoConfig(&pContext->mConfigPara, pConfigFilePath) != 0)
    {
        awaiisp_common_err_print("fatal error! no config file or parse conf file fail");
        return -1;
    }

    if(InitList(pContext) != 0)
    {
        awaiisp_common_err_print("Failed to initialize linked list\n");
        goto exit;
    }

#ifdef __linux__
    pContext->infile = fopen(pContext->mConfigPara.mDemoAiIspInputFilePath, "rb");
    if (pContext->infile == NULL)
    {
        awaiisp_common_err_print("Failed to open infile:%s\n",pContext->mConfigPara.mDemoAiIspInputFilePath);
        goto exit;
    }

    pContext->outfile = fopen(pContext->mConfigPara.mDemoAiIspOutputFilePath, "wb");
    if(pContext->outfile == NULL)
    {
        awaiisp_common_err_print("Failed to open outfile:%s\n",pContext->mConfigPara.mDemoAiIspOutputFilePath);
        goto exit;
    }

    awaiisp_config_param config;
    memset(&config, 0, sizeof(awaiisp_config_param));
    strncpy(config.lut_model_file, pContext->mConfigPara.mDemoAiIspLutNbgFilePath, AWAIISP_FILE_PATH_MAX);
    strncpy(config.model_file, pContext->mConfigPara.mDemoAiIspNbgFilePath, AWAIISP_FILE_PATH_MAX);
    config.model_version = pContext->mConfigPara.mDemoAiIspModelVersion;
    config.width = pContext->mConfigPara.mDemoAiIspWidth;
    config.height = pContext->mConfigPara.mDemoAiIspHeight;
    config.tdm_rxbuf_cnt = pContext->mConfigPara.mDemoAiIspTdmRxBufNum;
    config.mode = (int)pContext->mConfigPara.mDemoAiIspMode;
    config.reserve0 = pContext->mConfigPara.mDemoAiIspReserve0;
    config.reserve1 = pContext->mConfigPara.mDemoAiIspReserve1;
    config.reserve2 = pContext->mConfigPara.mDemoAiIspReserve2;
    config.ion_mem_open = 0;
#else
    awaiisp_config_param config;
    memset(&config, 0, sizeof(awaiisp_config_param));
    config.width = pContext->mConfigPara.mDemoAiIspWidth;
    config.height = pContext->mConfigPara.mDemoAiIspHeight;
    config.tdm_rxbuf_cnt = pContext->mConfigPara.mDemoAiIspTdmRxBufNum;
    config.mode = (int)pContext->mConfigPara.mDemoAiIspMode;
    config.reserve0 = pContext->mConfigPara.mDemoAiIspReserve0;
    config.ion_mem_open = 1;
    awaiisp_common_dbg_print("Width:%d, Height:%d, RxBufNum:%d, ion_mem_open = %d\n",
        config.width, config.height, config.tdm_rxbuf_cnt, config.ion_mem_open);
#endif
    ret |= awaiisp_open(pContext->isp_id, &config);
    ret |= awaiisp_start(pContext->isp_id);

    ret |= demo_awaiisp_register_tdm_buffer_callback(pContext->isp_id, (void *)&awaiisp_tdm_buffer_process_callback);
    ret |= awaiisp_register_return_tdm_buffer_callback(pContext->isp_id, (void *)&demo_awaiisp_return_tdm_buffer_callback);

    int result = pthread_create(&pContext->TestThreadId, NULL, demo_test_thread, pContext);
    if (result != 0)
    {
        awaiisp_common_err_print("fatal error! pthread create fail[%d]", result);
        ret |= -1;
    }
    pthread_join(pContext->TestThreadId, NULL);

    awaiisp_common_dbg_print("check framecount send:%d, recv:%d before the end.", pContext->sendframecount, pContext->recvframecount);
    int timeout = 200;
    while (timeout--)
    {
        if (pContext->sendframecount == pContext->recvframecount)
        {
            awaiisp_common_dbg_print("All frames (%d) have been received.", pContext->recvframecount);
            break;
        }
        usleep(10*1000);
    }
    if (0 == timeout)
    {
        awaiisp_common_warn_print("fatal error! timeout %d s, but there are still %d frames that have not been received!", timeout/100, pContext->recvframecount - pContext->sendframecount);
    }

    ret |= demo_awaiisp_register_tdm_buffer_callback(pContext->isp_id, NULL);
    ret |= awaiisp_stop(pContext->isp_id);
    ret |= awaiisp_close(pContext->isp_id);

exit:
    DeInitList(pContext);
#ifdef __linux__
    if (pContext->infile)
    {
        fclose(pContext->infile);
        pContext->infile = NULL;
    }

    if (pContext->outfile)
    {
        fclose(pContext->outfile);
        pContext->outfile = NULL;
    }
#endif
    if (pContext)
    {
        free(pContext);
        pContext = NULL;
    }

#ifndef __linux__
    rt_memheap_detach(&aiisp_demo_mempool);
    simple_iommu_unmap_region(0x48200000, AIISP_MEMRESERVE_SIZE);
#endif
    awaiisp_common_dbg_print("%s test result: %s", argv[0], ((0 == ret) ? "success" : "fail"));

    return ret;
}
#ifndef __linux__
FINSH_FUNCTION_EXPORT_ALIAS(aiisp_test, aiisp_test, user defined cmd);
#endif
