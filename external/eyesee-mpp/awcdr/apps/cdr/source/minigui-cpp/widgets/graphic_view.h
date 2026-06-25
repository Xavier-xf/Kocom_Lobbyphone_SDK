/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: graphic_view.h
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:

 History:
*****************************************************************************/

#ifndef _GRAPHIC_VIEW_H_
#define _GRAPHIC_VIEW_H_

#include "system_widget.h"

class GraphicView : public SystemWidget
{
    DECLARE_DYNCRT_CLASS(GraphicView, Runtime)
public:
    enum GraphicViewState {
        NORMAL = 0,
        HIGHLIGHT = 1,
    };
    GraphicView(View *parent);
    virtual ~GraphicView();
    virtual void GetCreateParams(CommonCreateParams &params);
    virtual int HandleMessage(HWND hwnd, int message, WPARAM wparam,
                            LPARAM lparam);
    virtual void SetImage(const std::string &path);
    virtual void SetPosition(int x, int y, int w, int h);
    int SetState(enum GraphicViewState state, bool refresh = false);
    enum GraphicViewState GetState();
    virtual void SetPlayBackImage(const std::string &path);
    virtual int OnMouseUp(unsigned int button_status, int x, int y);

    static void LoadImage(View *ctrl, const char *alias);
    static void LoadImage(View *ctrl, const char *normal, const char *highlight, enum GraphicViewState state);
    static void LoadImageFromAbsolutePath(View *ctrl, const std::string &path);
    static void UnloadImage(View *ctrl);
    static void GetImageInfo(View *ctrl,int *w,int *h);
    NotifyEvent OnClick;
    void SetCaptionColor(DWORD new_color);
private:
    enum GraphicViewState state_;
    std::string normal_image_;
    std::string highlight_image_;
    int SetImage(const BITMAP &image);
protected:
    BITMAP image_;
};

#endif  //_GRAPHIC_VIEW_H_
