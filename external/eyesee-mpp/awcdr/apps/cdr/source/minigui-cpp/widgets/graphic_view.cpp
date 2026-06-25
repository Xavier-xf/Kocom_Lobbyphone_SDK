/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: graphic_view.cpp
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:

 History:
*****************************************************************************/

#include "widgets/graphic_view.h"
#include "debug/app_log.h"
#include "window/user_msg.h"
#include "resource/resource_manager.h"
#include "src/extra/libs/png/lodepng.h"
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "GraphicView"


IMPLEMENT_DYNCRT_CLASS(GraphicView)

GraphicView::GraphicView(View *parent)
    :  SystemWidget(parent)
{
    memset(&image_, 0, sizeof(BITMAP));
    memset(&normal_image_, 0, sizeof(std::string));
    memset(&highlight_image_, 0, sizeof(std::string));
    option_style_ = SS_CENTER | SS_VCENTER | SS_REALSIZEIMAGE;

}

GraphicView::~GraphicView()
{
/*    if (image_.bmBits) {
        UnloadBitmap(&image_);
    }

    if (normal_image_.bmBits) {
        UnloadBitmap(&normal_image_);
    }

    if (highlight_image_.bmBits) {
        UnloadBitmap(&highlight_image_);
    }*/
}

void GraphicView::SetImage(const std::string &path)
{
    if (path.empty())
        return;
/*
    if (image_.bmBits) {
        UnloadBitmap(&image_);
    }

    ::LoadBitmapFromFile(HDC_SCREEN, &image_, path.c_str());
    SendMessage(GetHandle(), STM_SETIMAGE, (WPARAM)&image_, 0);*/
    std::string fs = "S:";
    fs = fs + path;
    if ((GetHandle() != HWND_NULL) && (GetHandle() != HWND_INVALID)) {
	lv_obj_t *img = lv_obj_get_child(GetHandle(), 0);
	printf("SetImage %s\n", fs.c_str());
	lv_img_set_src(img, fs.c_str());
	lv_img_set_size_mode(img, LV_IMG_SIZE_MODE_REAL);
	//lv_img_t * img = lv_img_get_src(GetHandle());
	//lv_img_set_offset_x(GetHandle(), (lv_obj_get_width(GetHandle()) - img->width) / 2);
	//lv_img_set_offset_y(GetHandle(), (lv_obj_get_height(GetHandle()) - img->height) / 2);

	/*lv_mem_free((void *)((lv_img_t *)GetHandle())->src);
	((lv_img_t *)GetHandle())->src = NULL;
	((lv_img_t *)GetHandle())->src_type = LV_IMG_SRC_UNKNOWN;*/
    }
    Refresh();
}

int GraphicView::SetImage(const BITMAP &image)
{
    if (image.bmBits == NULL) {
        db_warn("image is empty");
        return -1;
    }

    //SendMessage(GetHandle(), STM_SETIMAGE, (WPARAM)&image, 0);

    return 0;
}

void GraphicView::LoadImage(View *ctrl, const char *alias)
{
    printf("load image: %s\n", alias);
    printf("adsas\n");
    GraphicView* view = reinterpret_cast<GraphicView *>(ctrl);
    if (view) {
        std::string image = R::get()->GetImagePath(alias);
        view->SetImage(image);
    }
}

void GraphicView::SetPosition(int x, int y, int w, int h)
{
    RECT rc;
    ::SetRect( &rc, x, y, w+x, h+y ); //construct a rect
    View::SetPosition( &rc );
    if((GetHandle() != HWND_NULL) && (GetHandle() != HWND_INVALID)) {
        printf("GraphicView::SetPosition:%p\n", GetHandle());
	printf("SetPosition:x=%d y=%d w=%d h=%d\n", x, y, w, h);
	lv_obj_set_x(GetHandle(), x);
        lv_obj_set_y(GetHandle(), y);
        //lv_obj_set_width(GetHandle(), LV_SIZE_CONTENT);
        //lv_obj_set_height(GetHandle(), LV_SIZE_CONTENT);
	lv_obj_set_width(GetHandle(), w);
        lv_obj_set_height(GetHandle(), h);

	lv_obj_t *img = lv_obj_get_child(GetHandle(), 0);
	lv_obj_set_width(img, LV_SIZE_CONTENT);
        lv_obj_set_height(img, LV_SIZE_CONTENT);
    }
}

int GraphicView::SetState(enum GraphicViewState state, bool refresh)
{
    state_ = state;
    if (state_ == HIGHLIGHT) {
        if (highlight_image_ != "")
            SetImage(highlight_image_);
        else
            return -1;
    } else {
        SetImage(normal_image_);
    }

    if (refresh) {
        Refresh();
    }

    return 0;
}

enum GraphicView::GraphicViewState GraphicView::GetState()
{
    return state_;
}

void GraphicView::LoadImage(View *ctrl, const char *normal, const char *highlight, enum GraphicViewState state)
{
    GraphicView* view = reinterpret_cast<GraphicView *>(ctrl);
    if (view) {
        /*if (view->normal_image_.bmBits) {
            UnloadBitmap(&view->normal_image_);
        }
        if (view->highlight_image_.bmBits) {
            UnloadBitmap(&view->highlight_image_);
        }*/
	view->normal_image_ = R::get()->GetImagePath(normal);
        //::LoadBitmapFromFile(HDC_SCREEN, &(view->normal_image_), normal_path.c_str());
	
        view->highlight_image_ = R::get()->GetImagePath(highlight);
        //::LoadBitmapFromFile(HDC_SCREEN, &(view->highlight_image_), highlight_path.c_str());
        view->SetState(state);
    }
}

void GraphicView::LoadImageFromAbsolutePath(View *ctrl, const std::string &path)
{
    GraphicView* view = reinterpret_cast<GraphicView *>(ctrl);
    if (view) {
        view->SetImage(path);
    }
}

void GraphicView::UnloadImage(View *ctrl)
{
    GraphicView* view = reinterpret_cast<GraphicView *>(ctrl);
    printf("UnloadImage\n");
/*
    if (view->image_.bmBits) {
        UnloadBitmap(&view->image_);
    }*/
}

void GraphicView::GetImageInfo(View *ctrl,int *w,int *h)
{
    GraphicView* view = reinterpret_cast<GraphicView *>(ctrl);
    if (view->image_.bmBits) {
        *w = view->image_.bmWidth;
	*h = view->image_.bmHeight;
    }
}

void GraphicView::GetCreateParams(CommonCreateParams &params)
{
    params.class_name = CTRL_STATIC;
    params.alias      = GetClassName();
    params.style      = WS_VISIBLE | SS_NOTIFY | SS_BITMAP | SS_CENTERIMAGE
                        | option_style_;
    params.exstyle    = WS_EX_USEPARENTFONT | transparent_style_;
    params.x          = 0;
    params.y          = 0;
    params.w          = DEFAULT_CTRL_WIDTH;
    params.h          = DEFAULT_CTRL_HEIGHT;
}

int GraphicView::OnMouseUp(unsigned int button_status, int x, int y)
{
    if (OnClick)
        OnClick(this);
    return HELP_ME_OUT;;
}

int GraphicView::HandleMessage(HWND hwnd, int message, WPARAM wparam,
                                    LPARAM lparam)
{
    switch ( message )
    {
        default:
            return SystemWidget::HandleMessage(hwnd, message, wparam, lparam);
    }
}

void GraphicView::SetPlayBackImage(const std::string &path)
{
    if (path.empty())
        return;
/*
    if (image_.bmBits) {
        UnloadBitmap(&image_);
    }

    ::LoadBitmapFromFile(HDC_SCREEN, &image_, path.c_str());
    SendMessage(GetHandle(), STM_SETIMAGE, (WPARAM)&image_, 0);*/
}


void GraphicView::SetCaptionColor(DWORD new_color)
{
    //::SetWindowElementAttr(handle_, WE_FGC_WINDOW, new_color);
}
