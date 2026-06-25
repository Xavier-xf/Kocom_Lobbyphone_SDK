/* *******************************************************************************
 * Copyright (c), 2001-2016, Allwinner Tech. All rights reserved.
 * *******************************************************************************/
/**
 * @file    ADASEventUIProc.cpp
 * @brief
 * @author  sh
 * @version v1.0
 * @date    2018-01-02
 */
#include "ADASEventUIProc.h"

#include "debug/app_log.h"
#include "common/app_def.h"
#include "resource/resource_manager.h"
#include "common/message.h"
#include "common/posix_timer.h"
#include "bll_presenter/audioCtrl.h"
#include "application.h"
#include <math.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "ADASEventUIProc"
#define START_X_POSTION  480

using namespace EyeseeLinux;

#ifdef ENABLE_ADAS

int ADASEventUIProc::ConvertCoordinate(int disp_val, int img_val, int val)
{
    return (int)(1.0 * disp_val * val / img_val); //1280/960
}


//绘画矫正的线
void ADASEventUIProc::DrawALignLine(HDC hdc)
{
    float midRow = (float)65 * 320 / 100;
    float midCol = (float)adas_event_.subWidth / 2;
    float vLen = (float)320 - midRow - 2;
    float ang = (float)(45.0 / 180 * PI);
    float hLen = (float)((float)vLen * tan(ang));
    int UpX = ConvertCoordinate(screen_w_, adas_event_.subWidth, midCol);
    int UpY = midRow;//ConvertCoordinate(screen_h_, adas_event_.subHeight, midRow);
    int leftDnX = ConvertCoordinate(screen_w_, adas_event_.subWidth, midCol - hLen);
    int leftDnY = midRow + vLen;//ConvertCoordinate(screen_h_, adas_event_.subHeight, midRow + vLen);
    int rightDnX = ConvertCoordinate(screen_w_, adas_event_.subWidth, midCol + hLen);
    int rightDnY = midRow + vLen;//ConvertCoordinate(screen_h_, adas_event_.subHeight, midRow + vLen);
//    db_error("UpX %d UpY %d leftDnX %d leftDnY %d rightDnX %d rightDnY %d w %d h %d",
//            UpX,UpY,leftDnX,leftDnY,rightDnX,rightDnY,adas_event_.subWidth,adas_event_.subHeight);
    //SetPenWidth(hdc,5);
    //SetPenColor(hdc, RGBA2Pixel(hdc, 0x00, 0x00, 0xff, 0xff));
    //LineEx(hdc,UpX, UpY, leftDnX, leftDnY);
    //LineEx(hdc,UpX, UpY, rightDnX, rightDnY);
}

//绘画车道线
void ADASEventUIProc::DrawRoadLine(HDC hdc, int crop, CropDirection direction)
{
    int lx0 = 0,ly0 = 0,lx1 = 0,ly1 = 0,rx0 = 0,ry0 = 0,rx1 = 0,ry1 = 0;
    lx0 = adas_event_.nADASOutData_v2.lane.ltIdxs[0].x;
    ly0 = adas_event_.nADASOutData_v2.lane.ltIdxs[0].y;
    lx1 = adas_event_.nADASOutData_v2.lane.ltIdxs[1].x;
    ly1 = adas_event_.nADASOutData_v2.lane.ltIdxs[1].y;
    rx0 = adas_event_.nADASOutData_v2.lane.rtIdxs[0].x;
    ry0 = adas_event_.nADASOutData_v2.lane.rtIdxs[0].y;
    rx1 = adas_event_.nADASOutData_v2.lane.rtIdxs[1].x;
    ry1 = adas_event_.nADASOutData_v2.lane.rtIdxs[1].y;
#if 0
    lx0 = ConvertCoordinate(screen_w_, adas_event_.subWidth, lx0);
    ly0 = ly0 - crop;
//    ly0 = ConvertCoordinate(screen_h_, adas_event_.subHeight, ly0);
    lx1 = ConvertCoordinate(screen_w_, adas_event_.subWidth, lx1);
    ly1 = ly1 - crop;
//    ly1 = ConvertCoordinate(screen_h_, adas_event_.subHeight, ly1);
    rx0 = ConvertCoordinate(screen_w_, adas_event_.subWidth, rx0);
    ry0 = ry0 - crop;
//    ry0 = ConvertCoordinate(screen_h_, adas_event_.subHeight, ry0);
    rx1 = ConvertCoordinate(screen_w_, adas_event_.subWidth, rx1);
    ry1 = ry1 - crop;
//    ry1 = ConvertCoordinate(screen_h_, adas_event_.subHeight, ry1);
#endif
    lx0 = ConvertCoordinate(screen_w_, adas_event_.subWidth, lx0);
    if(direction == CropDirection_UP)
        ly0 = ly0 + crop;
    else if(direction == CropDirection_Center)
        ly0 = ly0 - crop;
    else if(direction == CropDirection_Down)
        ly0 = ly0 - crop;
    lx1 = ConvertCoordinate(screen_w_, adas_event_.subWidth, lx1);
    lx1 += 50;
    if(direction == CropDirection_UP)
        ly1 = ly1 + crop;
    else if(direction == CropDirection_Center)
        ly1 = ly1 - crop;
    else if(direction == CropDirection_Down)
        ly1 = ly1 - crop;
    rx0 = ConvertCoordinate(screen_w_, adas_event_.subWidth, rx0);
    if(direction == CropDirection_UP)
        ry0 = ry0 + crop;
    else if(direction == CropDirection_Center)
        ry0 = ry0 - crop;
    else if(direction == CropDirection_Down)
        ry0 = ry0 - crop;
    rx1 = ConvertCoordinate(screen_w_, adas_event_.subWidth, rx1);
    rx1 -= 20;
    if(direction == CropDirection_UP)
        ry1 = ry1 + crop;
    else if(direction == CropDirection_Center)
        ry1 = ry1 - crop;
    else if(direction == CropDirection_Down)
        ry1 = ry1 - crop;
    DrawLine(hdc,lx0,ly0,lx1,ly1);
    DrawLine(hdc,rx0,ry0,rx1,ry1);
}

void ADASEventUIProc::DrawFrame(HDC hdc,int x1,int y1,int x2,int y2)
{
    //SetPenWidth(hdc, 3);
    //SetPenColor(hdc, RGBA2Pixel(hdc, 0xff, 0x00, 0x00, 0xff));
    //MoveTo(hdc,x1,y1);
    //LineTo(hdc,x2,y2);
    //LineEx (hdc, x1, y1, x2, y2);
}

void ADASEventUIProc::DrawLine(HDC hdc,int x1,int y1,int x2,int y2)
{
    //SetPenWidth (hdc, 5);
    //SetPenColor(hdc, RGBA2Pixel(hdc, 0x00, 0xff, 0x00, 0xff));
    //LineEx (hdc, x1, y1, x2, y2);
}

//计算车距
float ADASEventUIProc::CalculateDistance(int vanishY,int bottomY,int srcHeight,float cameraHeight)
{
    float k,b;
    float w;
    float dist;
    k = -0.2548*(cameraHeight/100)+0.703;//输入单位:cm
    b = -k*(float)vanishY;
    w = k*(float)bottomY+b;
    dist = 567.2069/(720.0*w/(float)srcHeight+3.0285)-0.5;//(单位：m)
    return dist;
}

int ADASEventUIProc::ShowDistanceImage(HDC hdc,int index_,int x,int y)
{
    //alogd("距离前车车距还有:%d m", index_);
    if(index_ >DIST_IMAGE_NUMBER || index_ < 1){
        return -1;
    }
    BITMAP *image;
    image = &dist_image_[index_-1];
    //FillBoxWithBitmap(hdc,x,y,image->bmWidth,image->bmHeight,image);
}

void ADASEventUIProc::ShowWarnBitmap(HDC hdc,int warn,int x,int y)
{
    BITMAP *image;
    //UnloadBitmap(&warn_image_);
    InitWarnImage(warn);
    image = &warn_image_;
    if(image != NULL){
        db_error("w %d h %d",image->bmWidth,image->bmHeight);
        //FillBoxWithBitmap(hdc,x,y,image->bmWidth,image->bmHeight,image);
    }
}

void ADASEventUIProc::DrawCarsInfo(HDC hdc,int crop,CropDirection direction)
{
    int carX = 0,carY = 0,carW = 0,carH = 0;
    int mCarLines[32];
    int i = 0;
    float car_warntime = 0.0,car_warndist = 0.0;
    car_warn_ = false,show_fullwarn_ = false;
    lwarn_ = LANELINE_NO_DETECT,rwarn_ = LANELINE_NO_DETECT; //每次绘制车辆信息需要对变量重新赋初值,否则会出现ADAS误判的情况
//    db_error("car num %d",adas_event_.nADASOutData_v2.cars.Num);
    /********************************检查车辆相关参数**********************************/
    //#ifdef DRAW_CAR_DIST_RECT
    for (i = 0; i < adas_event_.nADASOutData_v2.cars.Num && i < CAR_NUMBER ; ++i)//
    {
        db_error("x %d y %d w %d h %d",adas_event_.nADASOutData_v2.cars.carP[i].idx.x,adas_event_.nADASOutData_v2.cars.carP[i].idx.y,
                adas_event_.nADASOutData_v2.cars.carP[i].idx.width,adas_event_.nADASOutData_v2.cars.carP[i].idx.height);
        //检车到车的大小已经超出了屏幕显示的范围，就不用继续往下执行
        if(adas_event_.nADASOutData_v2.cars.carP[i].idx.x + adas_event_.nADASOutData_v2.cars.carP[i].idx.width > adas_event_.subWidth ||
                adas_event_.nADASOutData_v2.cars.carP[i].idx.y + adas_event_.nADASOutData_v2.cars.carP[i].idx.height > adas_event_.subHeight)
        {
            break;
        }
        carX = ConvertCoordinate(screen_w_, adas_event_.subWidth,adas_event_.nADASOutData_v2.cars.carP[i].idx.x);
#if 0
        carY = adas_event_.nADASOutData_v2.cars.carP[i].idx.y - crop;
#endif
        if(direction == CropDirection_UP)
            carY = adas_event_.nADASOutData_v2.cars.carP[i].idx.y + crop;
        else if(direction == CropDirection_Center)
            carY = adas_event_.nADASOutData_v2.cars.carP[i].idx.y - crop;
        else if(direction == CropDirection_Down)
            carY = adas_event_.nADASOutData_v2.cars.carP[i].idx.y - crop;
//        carY = ConvertCoordinate(screen_h_, adas_event_.subHeight,adas_event_.nADASOutData_v2.cars.carP[i].idx.y);
        carW = ConvertCoordinate(screen_w_, adas_event_.subWidth,adas_event_.nADASOutData_v2.cars.carP[i].idx.width);
        carH = ConvertCoordinate(screen_h_, adas_event_.subHeight,adas_event_.nADASOutData_v2.cars.carP[i].idx.height);
        db_error("绘画车的边框--[%d] x = %d y=%d w=%d h=%d\n",i,carX,carY,carW,carH);
//        carY+=30; //识别到车的坐标偏高,需要往下调整一下
#if 0 //绘画整个方框方式
        drawRectangle(hdc,carX,carY,carW,carH);
#else //绘画4个角方式
        int len = (int)(carW * 1.0 / 6);
        mCarLines[0] = carX;
        mCarLines[1] = carY;
        mCarLines[2] = carX + len;
        mCarLines[3] = carY;
        mCarLines[4] = carX;
        mCarLines[5] = carY;
        mCarLines[6] = carX;
        mCarLines[7] = carY + len;
        mCarLines[8] = carX + carW - len;
        mCarLines[9] = carY;
        mCarLines[10] = carX + carW;
        mCarLines[11] = carY;
        mCarLines[12] = carX + carW;
        mCarLines[13] = carY;
        mCarLines[14] = carX + carW;
        mCarLines[15] = carY + len;
        mCarLines[16] = carX + carW - len;
        mCarLines[17] = carY + carH;
        mCarLines[18] = carX + carW;
        mCarLines[19] = carY + carH;
        mCarLines[20] = carX + carW;
        mCarLines[21] = carY + carH - len;
        mCarLines[22] = carX + carW;
        mCarLines[23] = carY + carH;
        mCarLines[24] = carX;
        mCarLines[25] = carY + carH;
        mCarLines[26] = carX + len;
        mCarLines[27] = carY + carH;
        mCarLines[28] = carX;
        mCarLines[29] = carY + carH - len;
        mCarLines[30] = carX;
        mCarLines[31] = carY + carH;
        for(int j =3;j<32;j+=4)
        {
            db_error("x1 %d y1 %d x2 %d y2 %d",mCarLines[j-3],mCarLines[j-2],mCarLines[j-1],mCarLines[j]);
            DrawFrame(hdc,mCarLines[j-3],mCarLines[j-2],mCarLines[j-1],mCarLines[j]);
        }
#endif
        db_error("is warn %d",adas_event_.nADASOutData_v2.cars.carP[i].isWarn);
        if(adas_event_.nADASOutData_v2.cars.carP[i].isWarn == CAR_WARN_LINE_COLLISION)//碰撞预警，只要检测到任意一辆车有碰撞预警，就置carwarn=true
        {
            car_warn_ = true;
            show_fullwarn_ = true;
            car_warndist = adas_event_.nADASOutData_v2.cars.carP[i].dist;//车距
            car_warntime = adas_event_.nADASOutData_v2.cars.carP[i].time;//距离碰撞时间
            //alogd("habo--> carWarnDist[%d]=%d  carWarnTime[%d]=%d",i,carWarnDist,i,carWarnTime);
        }

        //计算车距并且show
//        distance_ = (int)CalculateDistance(adas_event_.subHeight/2+10,adas_event_.nADASOutData_v2.cars.carP[i].idx.y +
//                                            adas_event_.nADASOutData_v2.cars.carP[i].idx.height, adas_event_.subWidth, 120);
        distance_ = (int)adas_event_.nADASOutData_v2.cars.carP[i].dist;//车距
        distance_ -= 2; //去掉车头的距离
        db_error("距离前车:[%d]_车距还有:%f m",i,distance_);
        const float EPSINON = 0.0000001;
        if ((distance_ >= - EPSINON) && (distance_ <= EPSINON)){
            db_error("invalid distance %f",distance_);
            return;
        }
        ShowDistanceImage(hdc, (int)distance_, carX, carY+carH+5);
    }

    /*************************检查是否有偏移警报**************************/
//    static int test = 0;
//    test++;
//    if(test == 15)
//        adas_event_.nADASOutData_v2.lane.rtWarn = LANELINE_CRIMPING_ALARM;

    if(adas_event_.nADASOutData_v2.lane.ltWarn >= LANELINE_DETECTED && adas_event_.nADASOutData_v2.lane.ltWarn <= LANELINE_CRIMPING_ALARM)
    {
        db_error("lwarn %d",adas_event_.nADASOutData_v2.lane.ltWarn);
        lwarn_ = adas_event_.nADASOutData_v2.lane.ltWarn;
        show_fullwarn_ = true;
    }
    else
    {
       if(adas_event_.nADASOutData_v2.lane.rtWarn  >= LANELINE_DETECTED && adas_event_.nADASOutData_v2.lane.rtWarn <= LANELINE_CRIMPING_ALARM)
        {
           db_error("rtWarn %d",adas_event_.nADASOutData_v2.lane.rtWarn);
           rwarn_ = adas_event_.nADASOutData_v2.lane.rtWarn;
           show_fullwarn_ = true;
        }
    }
//    db_error("habo--> test %d leftLaneLineWarn = %d rightLaneLineWarn = %d carwarn=%d",
//            test,adas_event_.nADASOutData_v2.lane.ltWarn,adas_event_.nADASOutData_v2.lane.rtWarn,car_warn_);
}

//检查是否有偏移警报
void ADASEventUIProc::DrawLaneWarning(HDC hdc)
{
    if(lwarn_ != LANELINE_NO_DETECT && car_warn_)  //左边车请保持车距
    {                                              //提示音请保持车距,显示图片left_crash_warn.jpg
        db_error("Please keep the distance from the left car!!");
        ShowWarnBitmap(hdc,LEFT_CRASH_WARN,START_X_POSTION,0);
        line_warn_ = LEFT_CRASH_WARN;
        show_fullwarn_ = true;
    }
    else if (rwarn_ != LANELINE_NO_DETECT && car_warn_)  //右边车请保持车距
    {                                                    //提示音请保持车距,显示图片right_crash_warn.jpg
        db_error("Please keep the distance from the right car!!");
        ShowWarnBitmap(hdc,RIGHT_CRASH_WARN,START_X_POSTION,0);
        line_warn_ = RIGHT_CRASH_WARN;
        show_fullwarn_ = true;
    }
    else if(lwarn_  == LANELINE_CRIMPING_ALARM)  //压到左车道线
    {                                            //提示音请不要压左线,显示图片left_warn_red.jpg
        db_error("Press to the left lane!!");
        ShowWarnBitmap(hdc,LEFT_WARN_RED,START_X_POSTION,0);
        line_warn_ = LEFT_WARN_RED;
        show_fullwarn_ = true;
    }
    else if(rwarn_ == LANELINE_CRIMPING_ALARM)  //压到右车道线
    {                                           //提示音请不要压右线,显示图片right_warn_red.jpg
        db_error("Press to the right lane!!");
        ShowWarnBitmap(hdc,RIGHT_WARN_RED,START_X_POSTION,0);
        line_warn_ = RIGHT_WARN_RED;
        show_fullwarn_ = true;
    }
    else if(car_warn_)                          //正前方车距警报
    {                                           //提示音请保持车距,显示图片crash_warn.jpg
        db_error("Car warning!!");
        ShowWarnBitmap(hdc,CRASH_WARN,START_X_POSTION,0);
        line_warn_ = CRASH_WARN;
        show_fullwarn_ = true;
    }

}

void ADASEventUIProc::ADASPlayWarning()
{
    if(line_warn_ == LEFT_CRASH_WARN)
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);
    else if(line_warn_ == RIGHT_CRASH_WARN)
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);
    else if(line_warn_ == LEFT_WARN_RED)
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_LEFT_CRASH_WARN);
    else if(line_warn_ == RIGHT_WARN_RED)
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_RIGHT_CRASH_WARN);
    else if(line_warn_ == CRASH_WARN)
        AudioCtrl::GetInstance()->PlaySound(AudioCtrl::ADAS_SOUND_CRASH_WARN);

}

ADASEventUIProc::ADASEventUIProc()
{
    line_warn_ = NO_WARN;
    lwarn_ = LANELINE_NO_DETECT;
    rwarn_ = LANELINE_NO_DETECT;
    car_warn_ = false;
    screen_w_ = 1280;
    screen_h_ = 320;
    show_fullwarn_ = false;
    distance_ = 0.0;
    memset(&adas_event_,0,sizeof(AW_AI_ADAS_DETECT_R__v2));
    InitDistanceImage();
    InitWarnImage(0);
}

ADASEventUIProc::~ADASEventUIProc()
{
    DeInitWarnImage();
    DeInitDistanceImage();
}

void ADASEventUIProc::InitDistanceImage()
{
    char filepath[128]={0};
    snprintf(filepath,sizeof(filepath)-1,"%s/dist_",RES_PATH);

    char fpath[64]={0};
    int ret;
    for(int i =0; i < DIST_IMAGE_NUMBER; i++)
    {
        sprintf(fpath,"%s%d.png",filepath,i+1);
        //alogd("<***fpath=%s*****filepath=%s**>",fpath,filepath);
        //ret = LoadBitmapFromFile(HDC_SCREEN, &dist_image_[i], fpath);
        if(ret!= ERR_BMP_OK)
            db_error("load the %d Dist bitmap error",ret);
    }
}

void ADASEventUIProc::InitWarnImage(int line_warn)
{
    int ret;
    char filepath[128]={0};
    int i = line_warn;
    switch(i)
    {
        case CRASH_WARN:
            snprintf(filepath,sizeof(filepath)-1,"%s/crash_warn.jpg",RES_PATH);
            break;
        case LEFT_WARN_RED:
            snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_red.jpg",RES_PATH);
            break;
//        case LEFT_WARN_GREEN:
//            snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_green.jpg",RES_PATH);
//            break;
//        case LEFT_WARN_YELLOW:
//            snprintf(filepath,sizeof(filepath)-1,"%s/left_warn_yellow.jpg",RES_PATH);
//            break;
        case RIGHT_WARN_RED:
            snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_red.jpg",RES_PATH);
            break;
//        case RIGHT_WARN_GREEN:
//            snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_green.jpg",RES_PATH);
//            break;
//        case RIGHT_WARN_YELLOW:
//            snprintf(filepath,sizeof(filepath)-1,"%s/right_warn_yellow.jpg",RES_PATH);
//            break;
        case LEFT_CRASH_WARN:
            snprintf(filepath,sizeof(filepath)-1,"%s/left_crash_warn.jpg",RES_PATH);
            break;
        case RIGHT_CRASH_WARN:
            snprintf(filepath,sizeof(filepath)-1,"%s/right_crash_warn.jpg",RES_PATH);
            break;
        default:
            snprintf(filepath,sizeof(filepath)-1,"%s/crash_warn.jpg",RES_PATH);
            break;
    }
    //ret = LoadBitmapFromFile(HDC_SCREEN, &warn_image_, filepath);
    if(ret!= ERR_BMP_OK)
    {
        db_error("load the  %s %d warn bitmap error",filepath, ret);
    }
}

void ADASEventUIProc::DeInitDistanceImage()
{
    int i = 0;
    //for(i = 0;i < DIST_IMAGE_NUMBER;i++)
    //    UnloadBitmap(&dist_image_[i]);
}

void ADASEventUIProc::DeInitWarnImage()
{
    //UinloadBitmap(&warn_image_);
}

void ADASEventUIProc::UpdateADASEventMsg(AW_AI_ADAS_DETECT_R__v2 *adas_event)
{
    if(adas_event != NULL){
        memset(&adas_event_, 0, sizeof(AW_AI_ADAS_DETECT_R__v2));
        memcpy(&adas_event_, adas_event, sizeof(AW_AI_ADAS_DETECT_R__v2));
    }
}

#endif

