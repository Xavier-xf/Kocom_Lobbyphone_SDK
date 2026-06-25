#ifndef _SAMPLE_FACEKIT_H_
#define _SAMPLE_FACEKIT_H_

#include <plat_type.h>
#include <tsemaphore.h>
#include <mm_comm_vo.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>
#include <mpi_isp.h>

#define VIPP2VO_NUM                 (2)
#define NN_CHN_NUM_MAX              (2)

#define MAX_FILE_PATH_SIZE          (256)

#define MAX_PERSON_NUM              (10)


typedef struct SampleFacekitCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}SampleFacekitCmdLineParam;

typedef struct VIPP2VOConfig
{
    int mIspDev;
    int mVippDev;
	PIXEL_FORMAT_E mPicFormat;
	int mFrameRate;
    int mCaptureWidth;
    int mCaptureHeight;
    int mDisplayX;
    int mDisplayY;
    int mDisplayWidth;
    int mDisplayHeight;
    int mLayerNum;

}VIPP2VOConfig;

typedef struct SampleFacekitConfig
{
    VIPP2VOConfig mVIPP2VOConfigArray[VIPP2VO_NUM];  
    int mTestDuration;  //unit:s, 0 mean infinite
    VO_INTF_TYPE_E mDispType;
    VO_INTF_SYNC_E mDispSync;
	ISP_CFG_MODE mIspWdrSetting;
}SampleFacekitConfig;


typedef struct VIPP2VOLinkInfo
{
	int mIspDev;
	PIXEL_FORMAT_E mPicFormat;
	int mFrameRate;
    int mCaptureWidth;
    int mCaptureHeight;
    VI_DEV mVIDev;
    VI_CHN mVIChn;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;
    VO_CHN mVOChn;
}VIPP2VOLinkInfo;



typedef struct SampleFacekitContext
{
    SampleFacekitCmdLineParam mCmdLinePara;
    SampleFacekitConfig mConfigPara;
    cdx_sem_t mSemExit;
    VO_DEV mVoDev;
    VIPP2VOLinkInfo mLinkInfoArray[VIPP2VO_NUM];
}SampleFacekitContext;


/********************************AI********************************/

#define BOX_NUM 10

typedef struct
{
    int region_hdl_base;
    int old_num_of_boxes;
}region_info_t;

typedef struct 
{
    int xmin;
    int ymin;
    int xmax;
    int ymax;
	BOOL match;
	
}BBoxRect_t;

typedef struct
{
    int valid_cnt;
    BBoxRect_t boxes[BOX_NUM];
}BBoxResults_t;


typedef struct AiServiceInfoItemConfig_t
{
    int mIspDev;
    int mVippDev;
	int mCaptureWidth;
    int mCaptureHeight;
	PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
	char mModelFile[MAX_FILE_PATH_SIZE];
	int mSrcFrameRate;
}AiServiceInfoItemConfig;

#ifndef PIX_FR_FEATURE_BYTES
#define PIX_FR_FEATURE_BYTES (260)
#endif

typedef struct PersonInfoItem_t
{
	char mVaild;
	char mPersonFile[MAX_FILE_PATH_SIZE];
    char mPersonRGBFile[MAX_FILE_PATH_SIZE];
    char mPersonIRFile[MAX_FILE_PATH_SIZE];
	int  mPersonId;
	char mPersonName[MAX_FILE_PATH_SIZE];
	int  mWidht;
	int  mHeight;
	unsigned char mPersonFea[PIX_FR_FEATURE_BYTES];
}PersonInfoItem;

typedef struct PersonInfo_t
{
	PersonInfoItem  mPesonInfos[MAX_PERSON_NUM];
	int 			mToatalNum;
	int             mValidNum;
}PersonInfo;


typedef struct AiServiceInfoConfig
{
    AiServiceInfoItemConfig mAiServiceInfoConifgArray[NN_CHN_NUM_MAX];
	PersonInfo mPersonInfo;
}AiServiceInfoConfig;


typedef struct AiServiceInfo_t
{
    int mChIdx;
    int mNbgType;
    int mIspDev;
    int mVippDev;
    int mViChn;
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPixelFormat;
    int mViBufNum;
    int mSrcFrameRate;
    char mModelFile[MAX_FILE_PATH_SIZE];
    int mDrawOrlEnable;
    int mDrawOrlVipp;
    int mDrawOrlSrcWidth;
    int mDrawOrlSrcHeight;
    int mRegionHdlBase;
} AiServiceInfo;

typedef struct AiContext_t
{
	AiServiceInfoConfig  mConfigPara;
	pthread_t            mDetectThread[NN_CHN_NUM_MAX];
    int                  mAiserviceExit[NN_CHN_NUM_MAX];
    int                  mVichannelStarted;
    AiServiceInfo        mAiServiceInfo[NN_CHN_NUM_MAX];
    unsigned char       *mFaceYyuv;
    region_info_t        mRegionInfo[NN_CHN_NUM_MAX];
	region_info_t       region_info[NN_CHN_NUM_MAX];

    pthread_t            mDetectThreadId;
	int                 mDetectThreadState; 
} AiContext;

#endif
