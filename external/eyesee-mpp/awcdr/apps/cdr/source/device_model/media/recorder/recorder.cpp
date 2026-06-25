/*************************************************
Copyright (C), 2015, AllwinnerTech. Co., Ltd.
File name: recorder.cpp
Author: yinzh@allwinnertech.com
Version: 0.1
Date: 2015-11-10
Description:
History:
*************************************************/
#include "window/window.h"
#include "window/user_msg.h"
#include "device_model/media/recorder/recorder.h"
#include "device_model/storage_manager.h"
#include "device_model/system/event_manager.h"
#include "device_model/system/power_manager.h"
#include "common/app_log.h"
#include "device_model/system/rtc.h"
#include "device_model/media/media_file.h"
#include "device_model/menu_config_lua.h"
#include <mpi_venc.h>

#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "recorder.cpp"
#endif

using namespace EyeseeLinux;
using namespace std;

bool Recorder::delay_record_flag = false;
std::map<int, std::string> Recorder::file_list_;
uint32_t Recorder::main_file_cnt_ = 0;
uint32_t Recorder::sub_file_cnt_ = 0;
uint32_t Recorder::record_time_ = 0;

class EyeseeLinux::MediaFile;

Recorder::Recorder(Camera *camera)
    : camera_(camera)
    , current_status_(RECORDER_IDLE)
    , enc_data_cb_(NULL)
    , enable_raw_data_(false)
    , raw_data_readable_(false)
    , file_allocate_size_(0)
    , is_silent_(false)
    , m_MaxDurationMs(10*1000)//10s
    , m_startRecord(false)
    , m_mtion_flag(false)
    , last_file_allocate_size_(0)
    , park_file_(NULL)
   // , lock_file_flag(false)
{
    pthread_mutex_init(&lock_, NULL);
    pthread_mutex_init(&alloc_file_lock,NULL);
	param_.mux_mode = 0;
    recorder_ = new EyeseeRecorder();
    recorder_->setOnInfoListener(this);
    recorder_->setOnDataListener(this);
    recorder_->setOnErrorListener(this);

    sps_info_.nLength = 0;
    sps_info_.pBuffer = NULL;
    pps_info_.nLength = 0;
    pps_info_.pBuffer = NULL;

    muxer_id_map_.clear();

    enc_data_cb_param_.rec_ = this;

    mVencId = 0;
}

Recorder::~Recorder()
{
    db_msg("recorder decontructor");

    if (recorder_ != NULL)
    delete recorder_;

    if (sps_info_.pBuffer != NULL)
        free(sps_info_.pBuffer);
    if (pps_info_.pBuffer != NULL)
        free(pps_info_.pBuffer);

    muxer_id_map_.clear();

    pthread_mutex_destroy(&lock_);
    pthread_mutex_destroy(&alloc_file_lock);
}

int Recorder::ParseHeadData()
{
    unsigned char *bufptr = NULL;
    unsigned int bufptrLeng;
    int ret = -1;

    db_msg("ParseHeadData start");
    VencHeaderData *spspps_data = (VencHeaderData*)calloc(sizeof(VencHeaderData), 1);

    ret = recorder_->getEncDataHeader(mVencId, spspps_data);
    if(ret < 0)
    {
        db_error("get enc data header failed, [%d]", ret);
        free(spspps_data);
        return -1;
    }

    bufptr     = spspps_data->pBuffer;
    bufptrLeng = spspps_data->nLength;
    db_msg("bufptrLeng=%d", bufptrLeng);
    if(NULL == bufptr )
    {
        db_warn("sps data buffer is null\n");
        free(spspps_data);
        return -1;
    }

    int j;
    int sps_max_size = bufptrLeng - 4;
    for(j = 4; j < sps_max_size; j++) {
        if(*(bufptr + j) == 0 && *(bufptr + j + 1) == 0
                && *(bufptr + j + 2) == 0 && *(bufptr + j + 3) == 1) {
            break;
        }
    }

    sps_info_.nLength = j;
    if(sps_info_.nLength <= 0){
        db_error("sps_info_.nLength=%d", sps_info_.nLength);
        free(spspps_data);
        return -1;
    }
    if(sps_info_.pBuffer != NULL){
        db_error("free pbuffer");
        free(sps_info_.pBuffer);
        sps_info_.pBuffer = NULL;
    }

    sps_info_.pBuffer = (unsigned char*)calloc(sps_info_.nLength, 1);
    memcpy(sps_info_.pBuffer, bufptr, sps_info_.nLength);

    pps_info_.nLength = bufptrLeng - sps_info_.nLength;
    if(pps_info_.nLength <= 0){
        db_error("pps.nLength=%d",pps_info_.nLength);
        if(sps_info_.pBuffer != NULL){
           db_error("free sps pbuffer");
           free(sps_info_.pBuffer);
           sps_info_.pBuffer = NULL;
        }
        free(spspps_data);
        return -1;
    }

    if(pps_info_.pBuffer != NULL){
        db_error("free pps pbuffer");
        free(pps_info_.pBuffer);
        pps_info_.pBuffer = NULL;
    }

    pps_info_.pBuffer = (unsigned char*)calloc(pps_info_.nLength, 1);

    memcpy(pps_info_.pBuffer, bufptr + sps_info_.nLength, pps_info_.nLength);

    free(spspps_data);

    return 0;
}

int Recorder::GetVencHeaderData(VencHeaderData &info)
{
    return recorder_->getEncDataHeader(mVencId, &info);
}

void Recorder::GetVencHeaderData(VencHeaderData &sps_info, VencHeaderData &pps_info)
{
    sps_info.nLength = sps_info_.nLength;
    sps_info.pBuffer = sps_info_.pBuffer;

    pps_info.nLength = pps_info_.nLength;
    pps_info.pBuffer = pps_info_.pBuffer;
}

void Recorder::checkRecordModeInParking()
{
    //Acc off || DC off , check record mode
    param_.recorder_mode = 0;//normal
    if(EventManager::GetInstance()!=NULL)
    {
        //Acc support && acc off mode
        #if 0
        if((EventManager::GetInstance()->mAccStatus==0) 
            && (m_MonitoryType==Normal_Monitor || m_MonitoryType==Ac_Impact_Monitor || m_MonitoryType==Delay_Monitor))
        {
            param_.recorder_mode = m_ParkingRecordMode;
        }
        #endif
        //DC support && DC off mode
        //if((EventManager::GetInstance()->mDcStatus==0) && (m_MonitoryType==Battery_Impact_Monitor))

        if(PowerManager::GetInstance()->getPowenOnType()==0)
        {
            db_warn("checkRecordModeInParking current is DC status");
            param_.recorder_mode = 1;
        }
        if(m_mtion_flag)
        {
            db_error("[debug_690]: motion detect happen set record mode is park\n");
            param_.recorder_mode = 1;
        }

    }
}


bool Recorder::GetRecordStartFlag()
{
    return m_startRecord;
}

int Recorder::StartRecord(bool p_FileVisible)
{
    db_msg("start record: %p", this);
    int ret = -1;
    pthread_mutex_lock(&lock_);
    m_startRecord = true;
    pthread_mutex_unlock(&lock_);

    this->DumpRecorderParm();
    // 由于响应UI操作为异步的方式，所以考虑会出现同时start未完成时调用stop
    // 这里需加锁保护
    checkRecordModeInParking();
    AW_MPI_VENC_SetVEFreq(MM_INVALID_CHN, 534);
    
	param_.m_fileVisible = p_FileVisible;
	MenuConfigLua *menuconfiglua=MenuConfigLua::GetInstance();
	param_.audio_record = menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_VOLUME_SWITCH);
	db_warn("[debug_jason]:InitRecorder param.audio_record = %d\n",param_.audio_record);
    ret = this->InitRecorder(param_);
    if(ret == RECORDER_SD_FULL){
        db_error("sd is full,stop rec");
        return RECORDER_SD_FULL;
    }

    pthread_mutex_lock(&lock_);
    if (ret < 0 || current_status_ != RECORDER_PREPARED) {
        db_error("recorder is not prepared, may be init first");
        m_startRecord = false;
        pthread_mutex_unlock(&lock_);
        return ret;
    }
    pthread_mutex_unlock(&lock_);

    if (param_.enc_type != 2) // not mjpeg
        this->ParseHeadData();
#ifdef WRITE_RAW_H264_FILE
    char filename[256] = {0};
    snprintf(filename, sizeof(filename),"/home/test_%p_%dx%d.h264", this, param_.video_size.width, param_.video_size.height);
    file_ = fopen(filename, "w");
#endif

    ret = recorder_->start();
    if (ret != NO_ERROR) {
        db_error("StartRecord Failed(%d)", ret);
        m_startRecord = false;
        return ret;
    }

    pthread_mutex_lock(&lock_);
    current_status_ = RECORDER_RECORDING;
    m_startRecord = false;
    pthread_mutex_unlock(&lock_);
    // maybe send notify only when csi camera start record
	if(id_ == 0 )
	    this->Notify(MSG_RECORD_START);
	if((!menuconfiglua->GetMenuIndexConfig(SETTING_RECORD_LOOP_SWITCH)) /*&& param_.rec_type == REC_MAIN*/
	        && param_.recorder_mode == 0){
	    db_error("close record loop function,run sd capacity check thread!!");
	    if(!StorageManager::GetInstance()->GetCapacityCheckThreadStatus()){
	        db_error("set check capacity switch true");
	        StorageManager::GetInstance()->SetCapacityCheckThreadStatus(true);
	    }
	}
    return ret;
}

int Recorder::StopRecord()
{
    db_msg("stop record current_status_ is %d ",current_status_);

    int ret = -1;

	pthread_mutex_lock(&lock_);
    if (current_status_ == RECORDER_RECORDING
            || current_status_ == RECORDER_IMPACT_DONE) {

        enc_data_cb_param_.need_exit_ = true;
        current_status_ = RECORDER_STOPPED;
        file_list_.clear();
        std::map<int, std::string> empty_map1;
        file_list_.swap(empty_map1);
        main_file_cnt_ = 0;
        sub_file_cnt_ = 0;
        pthread_mutex_unlock(&lock_);
        //disble gpsinfo package
        recorder_->gpsInfoEn(0);

        ret = recorder_->stop();

#ifdef WRITE_RAW_H264_FILE
        sync();
        fclose(file_);
#endif
    }else {
    	pthread_mutex_unlock(&lock_);
    }
    param_.MF = NULL;
    if(StorageManager::GetInstance()->GetCapacityCheckThreadStatus()){
        db_error("stop rec,stop sd capacity check thread!!");
        StorageManager::GetInstance()->SetCapacityCheckThreadStatus(false);
    }
    return ret;
}

int Recorder::CheckStorage(void)
{
    int ret = 0;
    int status = 0;
    StorageManager *sm = StorageManager::GetInstance();
    // update storage status
    status = sm->GetStorageStatus();
    if (status == MOUNTED)
    {
        storage_status_ = STORAGE_OK;
    }
    else if (status == STORAGE_LOOP_COVERAGE )
    {
        if (sm->FreeSpace() < 0) {
            storage_status_ = STORAGE_NOT_OK;
            return -1;
        }
        storage_status_ = STORAGE_OK;
    }
    else
    {
        storage_status_ = STORAGE_NOT_OK;
        db_error("sd card is not ok, can not start record");
        ret = -1;
    }

    db_debug("storage status: %d", status);

    return ret;
}

int Recorder::ResetRecorder()
{
    return recorder_->reset();
}

int Recorder::SetMute(bool mute)
{
    if (recorder_->setMuteMode(mute) != NO_ERROR) {
        db_error("set mute mode failed, value: %d", mute);
        return -1;
    }

    return 0;
}

int Recorder::SetVideoEncBitrate(uint32_t bitrate)
{
#if 0
    int ret = recorder_->setVideoEncodingBitRate(bitrate);

    if (ret != NO_ERROR) {
        db_error("dynamic set bitrate failed, ret = [%d]", ret);
        return -1;
    }
#endif
    return 0;
}

int Recorder::SetVideoEncQPRange(int min, int max)
{
#if 0
    int ret = recorder_->SetVideoEncodingQpRange(min, max);

    if (ret != NO_ERROR) {
        db_error("dynamic set QP failed, ret = [%d]", ret);
        return -1;
    }
#endif
    return 0;
}

int Recorder::SetVideoEncIFrameInterval(int val)
{
    status_t ret;
    VencParameters stVencParameters;
    ret = recorder_->getVencParameters(mVencId, stVencParameters);
    if(ret != NO_ERROR)
    {
        db_error("dynamic set I frame interval failed, ret = [%d]", ret);
    }
    stVencParameters.setVideoEncodingIFramesNumberInterVal(val);
    ret = recorder_->setVencParameters(mVencId, &stVencParameters);
    if(ret != NO_ERROR)
    {
        db_error("dynamic set I frame interval failed, ret = [%d]", ret);
    }

    if(ret != NO_ERROR)
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

int Recorder::SetDelayRecordTime(int val)
{
    delay_record_flag = true;

    switch(val)
    {
        case 0:
            param_.delay_fps = 0;
            delay_record_flag = false;
            break;
        case 1:
            param_.delay_fps =((double) 1/0.5);
            break;
        case 2:
            param_.delay_fps = ((double) 1/1);
            break;
        case 3:
            param_.delay_fps = ((double) 1/2);
            break;
        case 4:
            param_.delay_fps = ((double) 1/5);
            break;
        case 5:
            param_.delay_fps = ((double) 1/10);
            break;
        case 6:
            param_.delay_fps = ((double) 1/30);
            break;
       case 7:
            param_.delay_fps = ((double) 1/60);
            break;
       default:
            db_msg("[debug_jason]:invild parm");
            delay_record_flag = false;
            return -1;
    }
    db_msg("[debug_jason]:the param_.delay_fps = %f",param_.delay_fps);
    this->Notify(MSG_RECORD_TIMELAPSE_CHANGE);
    return 0;
}

int Recorder::SetVideoClcRecordTime(int val)
{
    uint16_t clc_record_time = 2;
    switch(val)
    {
        case 0:
            clc_record_time = 1;
            break;
        case 1:
            clc_record_time = 2;
            break;
        case 2:
            clc_record_time = 5;
            break;
    }
    param_.pack_strategy = PACK_BY_DURATION;
    param_.cycle_time = clc_record_time*(60*1000)+100;
    // 更新record_time_值, 以便子录像通过GetVideoRecordTime获取
    record_time_ = clc_record_time * 60;
    db_warn("record time: %d, bitrate: %d", param_.cycle_time, param_.bitrate);
    db_warn("[debug_jason]:record clc time is %d",param_.cycle_time/(60*1000));
    this->Notify(MSG_RECORD_LOOP_CHANGE);
    return 0;
}

int Recorder::SetRecordAudioOnOff(int val)
{
    param_.audio_record = val;
    this->Notify(MSG_AUDIO_RECORD_CHANGE);

    return 0;
}

int Recorder::setLockFileFlag(bool lock_flag)
{
    param_.lock_file_flag = lock_flag;
    db_warn("setLockFileFlag current flag is %d",param_.lock_file_flag);
    return 0;
}

int Recorder::GetCurrentClcRecordTime()
{
    return record_time_;
}

int Recorder::SetRemoteAudioRecordType(int p_AudioType)
{
	switch(p_AudioType )
	{
		case 0:
			param_.audio_record	= 1;
			break;
		case 1:
			param_.audio_record = 0;
			break;
		case 2:
			param_.audio_record = 2;
			break;
		default:
			return -1;
	}

	return 0;
}

int Recorder::SetRecordTime(int p_Time)
{
	param_.cycle_time = p_Time*1000 + 100;

	return 0;
}

int Recorder::SetRecordEncodeSize(int val)
{
    Size video_size;
    int framte = 0;
    uint32_t bitrate;
    switch(val){
#if 0
        case VIDEO_QUALITY_4K30FPS:
            video_size.width  = 3840;
            video_size.height = 2160;
            framte = 25;
#if 0
            if (param_.enc_type == VENC_H265)
                bitrate = (1 << 20)*30;
            else
                bitrate = (1 << 20)*45;
#endif
            bitrate = (1 << 20)*25;
            break;

        case VIDEO_QUALITY_2_7K30FPS:
            video_size.width  = 2688;
            video_size.height = 1520;
            framte = 30;
            bitrate = (1 << 20)*20;
            break;

        case VIDEO_QUALITY_1080P120FPS:
            video_size.width  = 1920;
            video_size.height = 1080;
            framte = 120;
            bitrate = (1 << 20)*35;
            break;
        case VIDEO_QUALITY_1080P60FPS:
            video_size.width  = 1920;
            video_size.height = 1080;
            framte = 60;
            bitrate = (1 << 20)*30;
            break;
        case VIDEO_QUALITY_1080P30FPS:
            video_size.width  = 1920;
            video_size.height = 1080;
            framte = 30;
            bitrate = (1 << 20)*10;
            break;
        case VIDEO_QUALITY_720P240FPS:
            video_size.width  = 1280;
            video_size.height = 720;
            framte = 240;
            bitrate = (1 << 20)*35;
            break;
        case VIDEO_QUALITY_720P120FPS:
            video_size.width  = 1280;
            video_size.height = 720;
            framte = 120;
            bitrate = (1 << 20)*30;
            break;
        case VIDEO_QUALITY_720P60FPS:
            video_size.width  = 1280;
            video_size.height = 720;
            framte = 60;
            bitrate = (1 << 20)*25;
            break;
        case VIDEO_QUALITY_720P30FPS:
            video_size.width  = 1280;
            video_size.height = 720;
            framte = 30;
            bitrate = (1 << 20)*6;
            break;
#endif
        case VIDEO_QUALITY_1080P30FPS:
            video_size.width  = 1920;
            video_size.height = 1080;
            framte = 30;
            bitrate = (1 << 20)*12;
            break;
        case VIDEO_QUALITY_2_7K30FPS:
//            video_size.width  = 2688;
//            video_size.height = 1520;
            video_size.width  = 2560;
            video_size.height = 1440;
            framte = 25;
            bitrate = (1 << 20)*15;
            break;
        default:
            video_size.width  = 1920;
            video_size.height = 1080;
            framte = 30;
            bitrate = (1 << 20)*12;
            db_warn("use default encode video size");
            break;
    }
    param_.video_size = video_size;
    param_.framerate = framte;
    param_.bitrate = bitrate;
    if(delay_record_flag == true)
    {
       param_.delay_fps = 0;
    }
    delay_record_flag = false;
    db_error("param dump:video_size: [%dx%d], fps: %d, bitrate: %d",
            param_.video_size.width, param_.video_size.height,
            param_.framerate, param_.bitrate);

    return 0;
}

int Recorder::SetMuxEncodeMode(int val)
{
	param_.mux_mode = val;
	db_msg("mux_mode %d\n",param_.mux_mode);

	return 0;
}

int Recorder::SetVideoEncoderType(int val)
{
    switch (val) {
        case 0:
            param_.enc_type = VENC_H265;
            break;
        case 1:
            param_.enc_type = VENC_H264;
            break;
        default:
            db_warn("unsupported encode type");
            break;
    }
    //recorder_->setVideoEncoder(enc_type);
    db_error("==============EncoderType %d==============",param_.enc_type);
    return 0;
}
int Recorder::ForceIFrame()
{
    int ret = recorder_->reencodeIFrame(mVencId);

    if (ret != NO_ERROR) {
        db_error("force encode a I frame failed");
        return -1;
    }

    return 0;
}


bool Recorder::GetVoiceStatus()
{
    return is_silent_;
}

uint16_t Recorder::GetStatus()
{
    uint16_t status;

    pthread_mutex_lock(&lock_);
    status = current_status_;
    pthread_mutex_unlock(&lock_);

    return status;
}

void Recorder::GetParam(RecorderParam &param) const
{
    param = this->param_;
}

void Recorder::SetParam(const RecorderParam &param)
{
    pthread_mutex_lock(&lock_);
    /*
    if (current_status_ != RECORDER_IDLE) {
        db_error("recorder is not idle, can not set param");
        pthread_mutex_unlock(&lock_);
        return;
    }
*/
    this->param_ = param;
    //current_status_ = RECORDER_CONFIGURED;
    pthread_mutex_unlock(&lock_);
}

void Recorder::SetFileName(const string &filename)
{
    this->param_.file = filename;
}

Camera *Recorder::GetCamera(void) const
{
    return camera_;
}

EyeseeRecorder *Recorder::GetEyeseeRecorder(void) const
{
    return recorder_;
}

void Recorder::onError(EyeseeRecorder *pMr, int what, int extra)
{
    switch (what) {
        case MPP_EVENT_ERROR_ENCBUFFER_OVERFLOW:
            db_error("Error: enc buffer overflow!!!");
            break;
        case EyeseeRecorder::MEDIA_ERROR_VENC_TIMEOUT:
            db_error("Error: enc timeout!!!");
            break;
        case EyeseeRecorder::MEDIA_ERROR_WRITE_DISK_ERROR:
            db_error("Error: enc write disk error!!!");
            break;
        case EyeseeRecorder::MEDIA_RECORDER_ERROR_UNKNOWN:
            db_error("Error: MEDIA_RECORDER_ERROR_UNKNOWN what = %d!!!\n",what);
            StopRecord();
            break;
        case EyeseeRecorder::MEDIA_ERROR_VENC_BUFFER_FULL:
            db_error("Error: MEDIA_ERROR_VENC_BUFFER_FULL");
            break;
        default:
            db_error("Error: Unknown what = %d!!!\n",what);
            break;
    }
}

void Recorder::onData(EyeseeRecorder *pMr, int what, int extra)
{
    enc_data_cb_param_.what_ = what;
    enc_data_cb_param_.extra_ = extra;
    if( enc_data_cb_ != NULL)
	    (*enc_data_cb_)(&enc_data_cb_param_);
}

void Recorder::SetEncodeDataCallback(EncodeDataCallback func, void *context)
{
    if (func == NULL) return;

    enc_data_cb_ = func;
    enc_data_cb_param_.context_ = context;
    enable_raw_data_ = true;
}

unsigned int Recorder::GetID() const
{
    return id_;
}

void Recorder::SetID(unsigned int id)
{
    Recorder::id_ = id;
}

char Recorder::GetStreamSenderType() const
{
    return stream_sender_type;
}

void Recorder::SetStreamSenderType(char type)
{
    Recorder::stream_sender_type = type;
}

int Recorder::GetClcRecordTime(int &val)
{
	val = this->param_.cycle_time;
    return 0;
}

int Recorder::addOutputFormatAndOutputSink()
{
    return 0;
}
int Recorder::removeOutputFormatAndOutputSink()
{
    return 0;
}

bool Recorder::RecorderIsBusy()
{
	if( current_status_ == RECORDER_IDLE || current_status_ == RECORDER_STOPPED )
	{
		return false;
	}

	//db_msg("recorder %d is busy\n",id_);

	return true;
}

int Recorder::GetRecordingFileName(std::string &p_RecFileName)
{
	p_RecFileName.clear();
	if( param_.MF != NULL)
	{
		p_RecFileName = param_.MF->GetMediaFileName();
	}

	return 0;
}

int Recorder::allocRecorderFile(char *p_fileName, int64_t p_fileSize, char *p_ReNameFile)
{
    pthread_mutex_lock(&alloc_file_lock);
    db_error("allocRecorderFile rename file name %s,file name %s",
            p_ReNameFile, p_fileName);
    //1:预分配   2:判断文件大小   为0进行预分配  不为零只进行清除动作  
    int fd = -1;
    if(!p_fileName){
        db_error("fatal error! Invalid file name!!");
        return -1;
    }
    if(p_ReNameFile != NULL)
    {
        db_error("ReNameFilelen %d,filenamelen %d",strlen(p_ReNameFile),strlen(p_fileName));
        if(strlen(p_ReNameFile))
        {
            db_warn("ReName:%s to %s",p_ReNameFile, p_fileName);
            fd = open(p_ReNameFile, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if( fd != -1 )
                close(fd);
            rename(p_ReNameFile, p_fileName);
        } else if(strlen(p_fileName)) {
            fd = open(p_fileName, O_RDWR | O_CREAT , 0666);
            if( fd != -1 )
            {
                if(fallocate(fd, 0x01, 0, p_fileSize) < 0)
                {
                    db_error("fatal error! Failed to fallocate size %lld, (%s)",p_fileSize, strerror(errno));
                }
                close(fd);
                db_error("allocate file success");
            }
        }
    }
    pthread_mutex_unlock(&alloc_file_lock);
    return 0;
}


void Recorder::setRecordMotionFlag(bool flag)
{
    m_mtion_flag = flag;
}

void Recorder::SetParkFile(MediaFile *park_file)
{
    park_file_ = park_file;
}


/**
  judge VENC_RC_MODE_E by encodeType and param_.bitrate_ctrl.
  param_ is RecorderParam.

  @param bitrate_ctrl 0:CBR 1:VBR 2:FIXQP
*/
VENC_RC_MODE_E RecorderParam::judgeVENC_RC_MODE_E()
{
    VENC_RC_MODE_E eRcMode = VENC_RC_MODE_H264CBR;
    if(VENC_H264 == enc_type)
    {
        switch(bitrate_ctrl)
        {
            case 0:
            {
                eRcMode = VENC_RC_MODE_H264CBR;
                break;
            }
            case 1:
            {
                eRcMode = VENC_RC_MODE_H264VBR;
                break;
            }
            case 2:
            {
                eRcMode = VENC_RC_MODE_H264FIXQP;
                break;
            }
            default:
            {
                eRcMode = VENC_RC_MODE_H264CBR;
                break;
            }
        }
    }
    else if(VENC_H265 == enc_type)
    {
        switch(bitrate_ctrl)
        {
            case 0:
            {
                eRcMode = VENC_RC_MODE_H265CBR;
                break;
            }
            case 1:
            {
                eRcMode = VENC_RC_MODE_H265VBR;
                break;
            }
            case 2:
            {
                eRcMode = VENC_RC_MODE_H265FIXQP;
                break;
            }
            default:
            {
                eRcMode = VENC_RC_MODE_H265CBR;
                break;
            }
        }
    }
    else if(VENC_MJPEG == enc_type)
    {
        switch(bitrate_ctrl)
        {
            case 0:
            {
                eRcMode = VENC_RC_MODE_MJPEGCBR;
                break;
            }
            case 1:
            {
                eRcMode = VENC_RC_MODE_MJPEGVBR;
                break;
            }
            case 2:
            {
                eRcMode = VENC_RC_MODE_MJPEGFIXQP;
                break;
            }
            default:
            {
                eRcMode = VENC_RC_MODE_MJPEGCBR;
                break;
            }
        }
    }
    else
    {
        aloge("fatal error! unknown encType:%d", enc_type);
    }
    return eRcMode;
}

