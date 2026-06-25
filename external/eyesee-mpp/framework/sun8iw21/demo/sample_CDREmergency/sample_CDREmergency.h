
#ifndef _SAMPLE_CDREMERGENCY_H_
#define _SAMPLE_CDREMERGENCY_H_

#include <string>
#include <deque>

#include <plat_type.h>
#include <tsemaphore.h>
#include <mpi_sys.h>
#include <mpi_clock.h>
#include <mpi_vo.h>

#include <Errors.h>

typedef struct SampleCDREmergencyCmdLineParam
{
    std::string mConfigFilePath;
}SampleCDREmergencyCmdLineParam;

enum SampleRecorderMethod
{
    ByTime = 0,  //by time duration
    BySize,      //by file size.
};
enum class ImpactStyle
{
    AddFile = 0,    //add an extra impact file.
    SwitchFile = 1, //switch impact file in current muxChn, then switch back to normal file.
};
typedef struct SampleCDREmergencyConfig
{
    int mCaptureWidth;
    int mCaptureHeight;
    PIXEL_FORMAT_E mPicFormat; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int mFrameRate;
    int mDisplayWidth;
    int mDisplayHeight;
    PAYLOAD_TYPE_E mEncodeType;
    PAYLOAD_TYPE_E mAudioEncodeType;
    int mEncodeWidth;
    int mEncodeHeight;
    int mEncodeBitrate;
    int mEncodeFrameRate;

    std::string mFolderPath;
    unsigned int mCount;  //keeping  files when app running.
    SampleRecorderMethod mMethod;
    int mDuration;  //unit:s, 0 mean infinite
    int mSize;   //unit: Byte. 0 mean infinite.
    int mMuxCacheDuration; //unit:ms
    int mFirstEmergencyTime; //unit:s
    int mSecondEmergencyTime;
    int mImpactDuration; //unit:s
    ImpactStyle mImpactStyle;
}SampleCDREmergencyConfig;

class SampleCDREmergencyContext;
class EyeseeCameraListener
    : public EyeseeLinux::EyeseeCamera::PictureCallback
    , public EyeseeLinux::EyeseeCamera::InfoCallback
{
public:
    bool onInfo(int chnId, CameraMsgInfoType info, int extra, EyeseeLinux::EyeseeCamera *pCamera);
    void onPictureTaken(int chnId, const void *data, int size, EyeseeLinux::EyeseeCamera* pCamera);

    EyeseeCameraListener(SampleCDREmergencyContext *pContext);
    virtual ~EyeseeCameraListener(){}
private:
    SampleCDREmergencyContext *const mpContext;
};

class EyeseeRecorderListener : public EyeseeLinux::EyeseeRecorder::OnErrorListener
                            , public EyeseeLinux::EyeseeRecorder::OnInfoListener
                            , public EyeseeLinux::EyeseeRecorder::OnDataListener
{
public:
    EyeseeRecorderListener(SampleCDREmergencyContext *pOwner);
    void onError(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onInfo(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
    void onData(EyeseeLinux::EyeseeRecorder *pMr, int what, int extra);
private:
    SampleCDREmergencyContext *const mpOwner;
};

class SampleCDREmergencyContext
{
public:
    SampleCDREmergencyContext();
    ~SampleCDREmergencyContext();
    EyeseeLinux::status_t ParseCmdLine(int argc, char *argv[]);
    EyeseeLinux::status_t loadConfig();
    static EyeseeLinux::status_t CreateFolder(const std::string& strFolderPath);
    std::string MakeFileName();
    EyeseeLinux::status_t setVideoFrameRateAndIFramesNumberInterval(SampleCDREmergencyContext *stContext, int fps);
    SampleCDREmergencyCmdLineParam mCmdLinePara;
    SampleCDREmergencyConfig mConfigPara;

    cdx_sem_t mSemExit;
    int mUILayer;

    MPP_SYS_CONF_S mSysConf;
    VO_DEV mVoDev;
    VO_LAYER mVoLayer;
    VO_VIDEO_LAYER_ATTR_S mLayerAttr;

    CameraInfo mCameraInfo;
    EyeseeLinux::EyeseeCamera *mpCamera;
    EyeseeLinux::EyeseeRecorder *mpRecorder;
    EyeseeCameraListener mCameraListener;
    EyeseeRecorderListener mRecorderListener;
    cdx_sem_t mSemRenderStart;
    int mVencId;
    int mMuxerId;
    std::mutex mSwitchImpactLock;
    bool mbMuxImpactFlag; //indicate impact file is recording.
    int mMuxerIdEmergency; //for impact style add_file.
    int64_t mImpactTm; //unit:ms

    std::deque<std::string> mFiles;
    int mFileNum;
    int mPicNum;
};

#endif  /* _SAMPLE_CDREMERGENCY_H_ */

