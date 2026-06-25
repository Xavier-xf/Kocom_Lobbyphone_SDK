/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    ADASEventWindow.h
 * @brief
 * @author  sh
 * @version v1.0
 * @date    2018-01-02
 */

#pragma once

#include "window/window.h"
#include "window/user_msg.h"
#include "widgets/graphic_view.h"
#include "widgets/text_view.h"
#include "media/camera/AdasData.h"

#define SHOW_ALIGNIMAGE_TIME    2*60
#define PI                      3.14159
#define OFFSET_SH               60*2
#define CAR_NUMBER              10
#define WARN_IMAGE_NUMBER       9
#define DIST_IMAGE_NUMBER       45
#define RES_PATH                "/usr/share/minigui/res/images"

class TextView;

typedef enum LaneLine
{
    LANELINE_NO_DETECT = 0, //未检测到
    LANELINE_DETECTED = 128,//检测到
    LANELINE_CRIMPING_ALARM = 255,//压线报警
};

typedef enum CropDirection
{
    CropDirection_UP = 1, //向上滑动调整视角
    CropDirection_Center = 2,
    CropDirection_Down = 3,
};

typedef enum CarWarn
{
  CAR_WARN_INVAILED = 0,
  CAR_WARN_LINE_DEPARTURE = 128,//车道偏移预警
  CAR_WARN_LINE_COLLISION = 255,//碰撞预警

};

typedef enum LineWarn
{
    CRASH_WARN=0,//前方保持车距
    LEFT_WARN_RED,
    LEFT_WARN_GREEN,
    LEFT_WARN_YELLOW,
    RIGHT_WARN_RED,
    RIGHT_WARN_GREEN,
    RIGHT_WARN_YELLOW,
    LEFT_CRASH_WARN,//left保持车距
    RIGHT_CRASH_WARN,//right 保持车距
    NO_WARN,
};

class ADASEventUIProc
{
public:
        ADASEventUIProc();
        virtual ~ADASEventUIProc();
        void UpdateADASEventMsg(AW_AI_ADAS_DETECT_R__v2 *adas_event);
        void DrawFrame(HDC hdc,int x1,int y1,int x2,int y2);
        void DrawLine(HDC hdc,int x1,int y1,int x2,int y2);
        void DrawALignLine(HDC hdc);
        void DrawRoadLine(HDC hdc,int crop,CropDirection direction);
        void DrawCarsInfo(HDC hdc,int crop,CropDirection direction);
        void DrawLaneWarning(HDC hdc);
        inline void SetFullWarnShowFlag(bool flag) { show_fullwarn_ = flag; }
        inline bool GetFullWarnShowFlag() { return show_fullwarn_; }
        void ADASPlayWarning();
        bool show_fullwarn_;
private:
        void InitDistanceImage();
        void InitWarnImage(int line_warn);
        void DeInitDistanceImage();
        void DeInitWarnImage();
        int ConvertCoordinate(int disp_val, int img_val, int val);
        int ShowDistanceImage(HDC hdc,int index_,int x,int y);
        void ShowWarnBitmap(HDC hdc,int warn,int x,int y);
        float CalculateDistance(int vanishY,int bottomY,int srcHeight,float cameraHeight);
        AW_AI_ADAS_DETECT_R__v2 adas_event_;
        pthread_mutex_t adas_lock_;
        BITMAP dist_image_[DIST_IMAGE_NUMBER];
        BITMAP warn_image_;
        LineWarn line_warn_;
        LineWarn old_line_warn_;
        unsigned char lwarn_;
        unsigned char rwarn_;
        bool car_warn_;
        int screen_w_;
        int screen_h_;
        float distance_;
};

