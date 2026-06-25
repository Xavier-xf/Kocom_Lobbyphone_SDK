#include "device_model/media/camera/camera_factory.h"
#include "device_model/media/media_definition.h"
#include "device_model/display.h"
#include "device_model/menu_config_lua.h"
#include "common/setting_menu_id.h"
#include "common/app_log.h"

#include <media/mm_comm_sys.h>
#include <media/mpi_sys.h>

#undef LOG_TAG
#define LOG_TAG "camera_factory.cpp"

using namespace EyeseeLinux;
using namespace std;

Camera *CameraFactory::CreateCamera(CameraID cam_id)
{
    db_msg("create camera: %d", cam_id);
    Camera *camera = NULL;
    switch (cam_id) {
        case CAM_IPC_0:
            camera = this->CreateIPCCamera(CAM_A);
            break;
        case CAM_IPC_1:
            camera = this->CreateIPCCamera(CAM_B);
            break;
        case CAM_STITCH_0:
            camera = this->CreateStitchCamera(CAM_A);
            break;
        case CAM_STITCH_1:
            camera = this->CreateStitchCamera(CAM_B);
            break;
        case CAM_NORMAL_0:
            camera = this->CreateNormalCamera(CAM_A);
            break;
        case CAM_NORMAL_1:
            camera = this->CreateNormalCamera(CAM_B);
            break;
        case CAM_UVC_0:
            camera = this->CreateUVCCamera(CAM_A);
            break;
        case CAM_UVC_1:
            camera = this->CreateUVCCamera(CAM_B);
            break;
        default:
            break;
    }
    //assert(camera != NULL);
    return camera;
}

Camera *CameraFactory::CreateCamera(PhysicalCameraID phy_cam_id, CameraInitParam &param)
{
    int ret = -1;
    Camera *camera = NULL;
    #if 1
    try{
        db_error("yxl: CreateCamera 00");
        camera = new Camera(phy_cam_id);
    }catch(std::bad_alloc& ba){
        db_error("yxl: CreateCamera capture std_bad_alloc");
        //assert(camera != NULL);
    }
    #endif
    if(camera == NULL)
    {
       db_error("yxl_debug: CreateCamera error");
       goto out;
    }
   // db_error("yxl: CreateCamera 11");
   // Camera *camera = new Camera(phy_cam_id);
    //db_error("yxl: CreateCamera 22");
    // add ConfigCamera
	camera->ConfigCamera(phy_cam_id,ISP_0,VIPP_0,VIPP_2);

    ret = camera->Open();
    if (ret < 0) {
        db_error("open camera failed");
        goto out;
    }
    camera->SetVflip(0);//set playback rolation
    ret = camera->InitCamera(param);
    if (ret < 0) {
        db_error("camera init failed");
        goto out;
    }
//    camera->StartPreview();

    cam_cnt_++;

    return camera;

out:
    delete camera;
    return NULL;
}

uint8_t CameraFactory::GetCameraCnt()
{
    return cam_cnt_;
}

CameraFactory::CameraFactory()
        : cam_cnt_(0)
{
    /*
    int ret;
    MPP_SYS_CONF_S sys_conf;
    sys_conf.nAlignWidth = 32;
    ret = ::AW_MPI_SYS_SetConf(&sys_conf);
    if (ret != SUCCESS) {
        db_error("AW_MPI_SYS_SetConf failed");
    }

    ret = ::AW_MPI_SYS_Init();
    if (ret != SUCCESS) {
        db_error("AW_MPI_SYS_Init failed");
        _exit(-1);
    }
    */
}


CameraFactory::~CameraFactory()
{
    /*
    db_msg("destruct");
    ::AW_MPI_SYS_Exit();
    */
}

Camera *CameraFactory::CreateIPCCamera(PhysicalCameraID phy_cam_id)
{
    CameraInitParam param;

    SIZE_S size = {1920, 1080};
    param.cam_param_.setVideoSize(size);

#ifdef GUI_SUPPORT
    if (phy_cam_id == CAM_B) {
        param.sur_.x = 158;
        param.sur_.y = 180;
        param.sur_.w = 443;
        param.sur_.h = 400;
    } else if (phy_cam_id == CAM_A) {
        param.sur_.x = 666;
        param.sur_.y = 180;
        param.sur_.w = 443;
        param.sur_.h = 400;
    }
#endif

    return CreateCamera(phy_cam_id, param);
}

Camera *CameraFactory::CreateStitchCamera(PhysicalCameraID phy_cam_id)
{
    CameraInitParam param;

    SIZE_S size = {1280, 720};
    param.cam_param_.setVideoSize(size);

    return CreateCamera(phy_cam_id, param);
}

Camera *CameraFactory::CreateNormalCamera(PhysicalCameraID phy_cam_id)
{
    CameraInitParam param;

    param.main_penc_size_ = {1920,1080};
    param.main_venc_size_ = {1920,1080};
#ifdef ENABLE_ADAS
//    param.sub_venc_size_  = {960,540};
//    param.sub_penc_size_  = {960,540};
    param.sub_venc_size_  = {1280,720};
    param.sub_penc_size_  = {1280,720};
#else
    param.sub_venc_size_  = {960,540};
    param.sub_penc_size_  = {960,540};
#endif
    param.framate = 30;
    param.buffernumber = 4;
    param.pixel_fmt_ = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;

	if( phy_cam_id == CAM_B)
		param.pixel_fmt_ = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    uint32_t vi_freq = 300000000;

    MenuConfigLua *menu_config = MenuConfigLua::GetInstance();
    int val = menu_config->GetMenuIndexConfig(SETTING_RECORD_RESOLUTION);
    db_error("val %d",val);
    switch(val)
    {
#if 0
        case VIDEO_QUALITY_4K30FPS:
            param.main_venc_size_ = {3840,2160};
            param.framate = 25;
            param.buffernumber = 4;
            break;
        case VIDEO_QUALITY_2_7K30FPS:
            param.main_venc_size_ = {2688,1520};
            param.framate = 30;
            param.buffernumber = 4;
            break;
        case VIDEO_QUALITY_1080P120FPS:
            param.main_venc_size_ = {1920,1080};
            param.framate = 120;
            param.buffernumber = 20;
            vi_freq = 480000000;
            break;
        case VIDEO_QUALITY_1080P60FPS:
            param.main_venc_size_ = {1920,1080};
            param.framate = 60;
            param.buffernumber = 10;
            break;
        case VIDEO_QUALITY_720P240FPS:
            param.main_venc_size_ = {1280,536};
            param.framate = 240;
            param.buffernumber = 40;
            vi_freq = 480000000;
            param.pixel_fmt_ = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
            break;
        case VIDEO_QUALITY_720P120FPS:
            param.main_venc_size_ = {1280,720};
            param.framate = 120;
            param.buffernumber = 30;
            vi_freq = 480000000;
            break;
        case VIDEO_QUALITY_720P60FPS:
            param.main_venc_size_ = {1280,720};
            param.framate = 60;
            param.buffernumber = 10;
            break;
        case VIDEO_QUALITY_720P30FPS:
            param.main_venc_size_ = {1280,720};
            param.framate = 30;
            param.buffernumber = 5;
            break;
#endif
        case VIDEO_QUALITY_1080P30FPS: //1080p下vi图像格式设置LBC,节省内存
            param.main_venc_size_ = {1920,1080};
            param.framate = 30;
            param.buffernumber = 4;
            param.pixel_fmt_ = MM_PIXEL_FORMAT_YUV_AW_LBC_2_5X;
            break;
        case VIDEO_QUALITY_2_7K30FPS:  //ve插值2k,vi图像格式需要设置YUV
            param.main_venc_size_ = {1920,1080};
            param.framate = 30;
            param.buffernumber = 4;
            param.pixel_fmt_ = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        break;
        default:
            db_error("not support image_quality:%d, use 1080P30FPS as default", val);
            param.main_venc_size_ = {1920,1080};
            param.framate = 30;
            param.buffernumber = 4;
        break;
    }

    val = menu_config->GetMenuIndexConfig(SETTING_RECORD_RESOLUTION);
    switch(val) {
#if 0
        case PIC_RESOLUTION_8M:
            {
                param.main_penc_size_ = {3840,2160};
            }
            break;
        case PIC_RESOLUTION_13M:
            {
                param.main_penc_size_ = {4800,2700};
            }
            break;
        case PIC_RESOLUTION_16M:
            {
                param.main_penc_size_ = {5440,3060};
            }
            break;
#endif
        case PIC_RESOLUTION_2M:
            {
                param.main_penc_size_ = {1920,1080};
            }
            break;
        case PIC_RESOLUTION_4M:
            {
                param.main_penc_size_ = {2688,1520};
            }
            break;
        default:
            db_error("not support PicResolution:%d", val);
    }

    int vf = menu_config->GetMenuIndexConfig(SETTING_CAMERA_IMAGEROTATION);
    param.mirror = vf ? 0 : 1;
    param.vflip  = vf ? 0 : 1;
//    if (phy_cam_id == CAM_B) {
//        param.main_chn_ = phy_cam_id + 1;
//    } else if (phy_cam_id == CAM_A) {
//        param.main_chn_ = phy_cam_id;
//    }
//
//    param.sub_chn_ = param.main_chn_ + 1;
    if(phy_cam_id == CAM_A){
        param.main_chn_ = 0;
        param.sub_chn_  = 2;
    }else if(phy_cam_id == CAM_B){
        param.main_chn_ = 1;
        param.sub_chn_  = 3;
    }
#ifdef GUI_SUPPORT
    if (phy_cam_id == CAM_B) {
        param.sur_.x = SCREEN_WIDTH/2;
        param.sur_.y = SCREEN_HEIGHT/2;
        param.sur_.w = SCREEN_WIDTH/2;
        param.sur_.h = SCREEN_HEIGHT/2;
    } else if (phy_cam_id == CAM_A) {
        param.sur_.x = 0;
        param.sur_.y = 0;
        param.sur_.w = SCREEN_WIDTH;
        param.sur_.h = SCREEN_HEIGHT;
    }
#endif
    //AW_MPI_VI_SetVIFreq(0, vi_freq);
    return CreateCamera(phy_cam_id, param);
}

Camera *CameraFactory::CreateUVCCamera(PhysicalCameraID phy_cam_id)
{
    int ret,FlipVal=0;
    CameraInitParam param;
    Camera *camera = new Camera(phy_cam_id);
    assert(camera != NULL);
    camera->ConfigCamera(phy_cam_id,ISP_1,VIPP_1,VIPP_3);
    ret = camera->Open();
    if (ret < 0)
    {
        db_error("open camera failed");
        goto out;
    }

    param.main_penc_size_ = {1920,1080};
    param.main_venc_size_ = {1920,1080};
//    param.sub_venc_size_  = {960,540};
//    param.sub_penc_size_  = {960,540};
    param.sub_venc_size_  = {1280,720};
    param.sub_penc_size_  = {1280,720};
    param.framate = 25;
    param.buffernumber = 4;
//    param.main_chn_ = phy_cam_id;
//    param.sub_chn_ = param.main_chn_ + 1;
//	if(phy_cam_id == CAM_B )
//	{
//		param.main_chn_ = phy_cam_id+1;
//		param.sub_chn_ = param.main_chn_ + 1;
//	}
    if(phy_cam_id == CAM_B){
        param.main_chn_ = 1;
        param.sub_chn_  = 3;
    }

#ifdef GUI_SUPPORT
		if (phy_cam_id == CAM_B)
		{
			param.sur_.x = 0;
			param.sur_.y = 0;
			param.sur_.w = SCREEN_WIDTH;
			param.sur_.h = SCREEN_HEIGHT;
		}
#endif
    if(phy_cam_id == CAM_B)
        param.pixel_fmt_ = MM_PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    if(!MenuConfigLua::GetInstance()->GetMenuIndexConfig(SETTING_CAMERA_IMAGEROTATION));
    {
        FlipVal = 1;
    }
    param.mirror = FlipVal;
    param.vflip = FlipVal;
    ret = camera->InitCamera(param);
    if (ret < 0)
    {
        db_error("camera init failed");
        goto out;
    }
	printf("@@CAM_B InitCamera ok!\n");
    cam_cnt_++;

    return camera;

    out:
        delete camera;
        return NULL;
}
