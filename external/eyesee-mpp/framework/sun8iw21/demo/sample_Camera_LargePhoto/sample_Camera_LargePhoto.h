
#ifndef _SAMPLE_CAMERA_LARGEPHOTO_H_
#define _SAMPLE_CAMERA_LARGEPHOTO_H_

#include <string>
#include <mm_comm_sys.h>
#include <mm_comm_video.h>
#include <mm_comm_vo.h>
#include <tsemaphore.h>
#include <EyeseeCamera.h>

typedef struct SampleCameraLargePhotoCmdLineParam
{
    std::string mConfigFilePath;
}SampleCameraLargePhotoCmdLineParam;

typedef struct SampleCameraLargePhotoConfig
{
    int nPreviewWidth;
    int nPreviewHeight;
    int nPreviewFrameRate;
    int nPreviewFrameNum;
    PIXEL_FORMAT_E ePreviewPicFormat;
    int nPreviewRotation; //(previewMirror << 16) | previewRotation;
    int nDisplayFrameRate;  //0 means display every frame.
    int nDisplayWidth;
    int nDisplayHeight;
    bool bKeepRender;
    
    int nCaptureWidth;
    int nCaptureHeight;
    int nCaptureFrameRate;
    int nCaptureFrameNum;
    PIXEL_FORMAT_E eCapturePicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420

    int nDigitalZoom;   //0~10
    
    int nTakePhotoTimes;
    bool bKeepJpegEncoder;
    int nJpegWidth;
    int nJpegHeight;
    int nJpegQuality;
    int nJpegThumbWidth;
    int nJpegThumbHeight;
    int nJpegThumbQuality;
    int nJpegNum;
    int nJpegInterval;  //unit:ms
    std::string strJpegFolderPath;

    int nTestDuration;  //unit:s, 0 mean infinite
    int capture_lost_frame_cnt;
}SampleCameraLargePhotoConfig;

class SampleCameraLargePhotoContext;

class EyeseeCameraCallback
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
    , public EyeseeLinux::EyeseeCamera::ErrorCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onError(int chnId, int error, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraCallback(SampleCameraLargePhotoContext *pContext);
    virtual ~EyeseeCameraCallback(){}
private:
    SampleCameraLargePhotoContext *const mpContext;
};

class SampleCameraLargePhotoPictureRegionCallback : public EyeseeLinux::PictureRegionCallback
{
public:
    SampleCameraLargePhotoPictureRegionCallback(){}
    ~SampleCameraLargePhotoPictureRegionCallback(){};
    virtual void addPictureRegion(std::list<PictureRegionType> &rPictureRegionList);
};

class SampleCameraLargePhotoContext
{
public:
    SampleCameraLargePhotoContext();
    ~SampleCameraLargePhotoContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    SampleCameraLargePhotoCmdLineParam mCmdLinePara;
    SampleCameraLargePhotoConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeCameraCallback mCameraCallbacks;
    SampleCameraLargePhotoPictureRegionCallback mPictureRegionCallback;
    cdx_sem_t mSemRenderStart;
    int mPicNum;
    int mPicNumInOneTakePicture; //record received picture number in current takingPicture process.
    cdx_sem_t mSemTakePictureDone;
    int mScalerOutChnForPreview; // = vippIndex
    int mScalerOutChnForLargePhoto; // = vippIndex
    ISP_DEV mIspDevForPreview;
    ISP_DEV mIspDevForLargePhoto;
};

#endif  /* _SAMPLE_CAMERA_LARGEPHOTO_H_ */

