/*****************************************************************************
 Copyright (C), 2015, AllwinnerTech. Co., Ltd.
 File name: progress_bar.cpp
 Author: yangy@allwinnertech.com
 Version: v1.0
 Date: 2015-11-24
 Description:

 History:
*****************************************************************************/

#include "widgets/progress_bar.h"
#include "widgets/ctrlclass.h"
#include "window/user_msg.h"
#include "common/style.h"
#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "ProgressBar"
#include "debug/app_log.h"
#include "progress_bar.h"

#define IDC_LABEL_LEFT  601
#define IDC_LABEL_RIGHT 602
#define IDC_PROGRESSBAR 603

static WNDPROC oldProc;


IMPLEMENT_DYNCRT_CLASS(ProgressBar)

ProgressBar::ProgressBar(View *parent)
    : CustomWidget(parent)
{
    progress_data_.nMin = 0;
    progress_data_.nMax = 100;
    progress_data_.nPos = 0;
    progress_data_.nStepInc = 1;
    progress_data_.seek_value = 0;
}

ProgressBar::~ProgressBar()
{

}

void ProgressBar::GetCreateParams(CommonCreateParams &params)
{
    params.class_name = CTRL_SEEKBAR;
    params.alias      = GetClassName();
    params.style      = WS_VISIBLE;
    params.exstyle    = WS_EX_USEPARENTFONT | transparent_style_;
    params.x          = 0;
    params.y          = 0;
    params.w          = DEFAULT_CTRL_WIDTH;
    params.h          = DEFAULT_CTRL_HEIGHT;
}


/************* not support vertical ******************/
static void my_draw_progress (HWND hwnd, HDC hdc, int nMax, int nMin,
                                        int nPos)
{
    RECT    rcClient;
    int     x, y, w, h;
    ldiv_t   ndiv_progress;
    unsigned int     nAllPart;
    unsigned int     nNowPart;
    int     whOne, nRem;
    int     ix;
    unsigned int     i;
    int     step;
    int pbar_border = 0;
    gal_pixel old_color;

    if (nMax == nMin)
        return;

    if ((nMax - nMin) > 5)
        step = 1;
    else
        step = 1;

    //GetClientRect (hwnd, &rcClient);

    x = rcClient.left + pbar_border;
    y = rcClient.top + pbar_border;
    w = RECTW (rcClient) - (pbar_border << 1);
    h = RECTH (rcClient) - (pbar_border << 1);

    //SetWindowBkColor(hwnd, COLOR_PROGRESS_BAR_INNER_BG);
    /*if (hwnd != HWND_NULL)
        old_color = SetBrushColor (hdc, GetWindowBkColor (hwnd));
    else
        old_color = SetBrushColor (hdc,
                    GetWindowElementPixel(HWND_DESKTOP, WE_BGC_DESKTOP));*/

    //draw the erase background
    /*FillBox (hdc, rcClient.left, rcClient.top,
            RECTW (rcClient), RECTH (rcClient));

    SetPenColor(hdc, old_color);

    ndiv_progress = ldiv (nMax - nMin, step);
    nAllPart = ndiv_progress.quot;

    ndiv_progress = ldiv (nPos - nMin, step);
    nNowPart = ndiv_progress.quot;

    ndiv_progress = ldiv (w, nAllPart);*/ /* calculate the with for each step*/

    /* set the fill color */
    //SetBrushColor(hdc, COLOR_PROGRESS_BAR_INNER_FG);

#if 0
    whOne = ndiv_progress.quot;
    nRem = ndiv_progress.rem;

    /* dislay the % */
    if (whOne >= 4) {
        for (i = 0, ix = x + 1; i < nNowPart; ++i) {
            if ((ix + whOne) > (x + w))
                whOne = x + w - ix;

            FillBox (hdc, ix, y + 1, whOne, h - 2);
            ix += whOne;
/*
            if(nRem > 0) {
                ix ++;
                nRem --;
            }
*/
        }
    } else
#endif
    {
        int prog = w * nNowPart/nAllPart;

        //FillBox (hdc, x, y, prog, h);
    }
}


long int NewProgressBarProc (HWND hwnd, unsigned int message, WPARAM wparam,
                                        LPARAM lparam)
{
    HDC           hdc;
    PCONTROL      pCtrl;

    //pCtrl = gui_Control (hwnd);

    switch(message) {
    case MSG_CREATE:
        {
        }
        break;
    case MSG_PAINT:
        {
            PROGRESSDATA *data = (PROGRESSDATA *)pCtrl->dwAddData2;
            //hdc = BeginPaint (hwnd);

            my_draw_progress (hwnd, hdc, data->nMax, data->nMin, data->nPos);

            //EndPaint (hwnd, hdc);*/
            return 0;
        }

    default:
        break;
    }

    return (*oldProc) (hwnd, message, wparam, lparam);
}

int ProgressBar::HandleMessage(HWND hwnd, int message, WPARAM wparam,
                                        LPARAM lparam)
{
    switch(message) {
    case MSG_CREATE:
        {
            HWND retWnd;
            RECT rect;
            int x, y, w, h;
            db_msg(" ");
/************************************************************
    |  _________     _____________________     _________  |
    |1| label_w | 5 |    progressbar_w    | 5 | label_w |1|
    |  ---------     ---------------------     ---------  |
*************************************************************/
            /********************** create the left label *******************/
            retWnd = CreateWindowEx(CTRL_STATIC, NULL,
                    WS_VISIBLE | SS_SIMPLE,
                    WS_EX_NONE | WS_EX_TRANSPARENT | SS_LEFT,
                    IDC_LABEL_LEFT,
                    0, 0, 0, 0,
                    hwnd, 0);
            if(retWnd == HWND_INVALID) {
                db_error("create playback progress bar label left failed");
                break;
            }
            //SetWindowBkColor(retWnd, back_color_);
            /**************** create the right label ******************/
            retWnd = CreateWindowEx(CTRL_STATIC, NULL,
                    WS_VISIBLE | SS_SIMPLE,
                    WS_EX_NONE | WS_EX_TRANSPARENT | SS_LEFT,
                    IDC_LABEL_RIGHT,
                    0, 0, 0, 0,
                    hwnd, 0);
            if(retWnd == HWND_INVALID) {
                db_error("create playback progress bar label right failed");
                break;
            }
            //SetWindowBkColor(retWnd, back_color_);

            /***************  create the progress bar*****************/
            /*retWnd = CreateWindowEx(CTRL_PROGRESSBAR, NULL,
                    WS_VISIBLE,
                    WS_EX_NONE,
                    IDC_PROGRESSBAR,
                    0, 0, 0, 0,
                    hwnd, 0);*/
            if(retWnd == HWND_INVALID) {
                db_error("create playback progress bar label right failed");
                break;
            }
            //oldProc = SetWindowCallbackProc(retWnd, NewProgressBarProc);

/*            SetWindowBkColor(hwnd, back_color_);
            SetWindowElementAttr(GetDlgItem(hwnd, IDC_LABEL_LEFT),
                WE_FGC_WINDOW, COLOR_PROGRESS_BAR_TEXT_FG);
            SetWindowElementAttr(GetDlgItem(hwnd, IDC_LABEL_RIGHT),
                WE_FGC_WINDOW, COLOR_PROGRESS_BAR_TEXT_FG);
*/
            PROGRESSDATA prg_data;
            prg_data.nMin = 0;
            prg_data.nMax = 100;
            prg_data.nPos = 0;
            prg_data.nStepInc = 1;

/*            SendMessage(hwnd, PGBM_SETTIME_RANGE, (WPARAM)&prg_data, 0);
            SendMessage(hwnd, PGBM_SETCURTIME, (WPARAM)&prg_data, 0);
            SendMessage(hwnd, PGBM_SETSTEP, (WPARAM)&prg_data, 0);*/
        }
        break;
    case MSG_SIZECHANGED: {
#if 0
            RECT rect;
            GetClientRect(hwnd, &rect);
            int c_x, c_y, c_w, c_h; // control's x, y, w, h
            c_x = rect.left;
            c_y = rect.top;
            c_w = rect.right;
            c_h = rect.bottom;

            // label char length
            int char_len = strlen("000:00");

            // label width
            int label_w = 8*char_len;

            // progress bar width
            int pgbar_w = c_w-(2*label_w+18);

            // visible part height
            int v_h = 13;

            // visible part y
            int v_y = (c_h - v_h) / 2;

            // left label x
            int ll_x = c_x + 4;

            // progress bar x
            int pgbar_x = ll_x + label_w + 5;

            // right label x
            int rl_x = pgbar_x + pgbar_w + 5;

            MoveWindow(GetDlgItem(hwnd, IDC_LABEL_LEFT), ll_x, v_y , label_w, v_h, false);
            MoveWindow(GetDlgItem(hwnd, IDC_LABEL_RIGHT), rl_x, v_y, label_w, v_h, false);
            MoveWindow(GetDlgItem(hwnd, IDC_PROGRESSBAR), pgbar_x, (c_h - 8) / 2, pgbar_w, 8, false);
#endif

            RECT rect;
            //GetClientRect(hwnd, &rect);
            int c_x, c_y, c_w, c_h; // control's x, y, w, h
            c_x = rect.left;
            c_y = rect.top;
            c_w = rect.right;
            c_h = rect.bottom;

            // label char length
            int char_len = strlen("000:00");

            // label width
            int label_w = 8*char_len;

            // progress bar width
            int pgbar_w = c_w-(2*label_w+18);

            // visible part height
            int v_h = 13;

            // visible part y
            int v_y = (c_h - v_h) / 2;

            int pos = c_x;

            // progress bar x
            int pgbar_x = 0;

            // right label x
            int ll_x = pgbar_x + pgbar_w + 5;

            // left label x
            int rl_x = ll_x + label_w;

            //MoveWindow(GetDlgItem(hwnd, IDC_LABEL_LEFT), ll_x, v_y , label_w, v_h, false);
            //MoveWindow(GetDlgItem(hwnd, IDC_LABEL_RIGHT), rl_x, v_y, label_w, v_h, false);
            //MoveWindow(GetDlgItem(hwnd, IDC_PROGRESSBAR), pgbar_x, (c_h - 8) / 2, pgbar_w, 8, false);
        }
        break;
    case LV_EVENT_READY:
        {
            PGBTime_t start_time, end_time;
            char buf_left[20] = {0}, buf_right[20] = {0};

            /** wparam is left label time, lparam is right label time *****/
            int max = lv_slider_get_max_value(lv_obj_get_child((lv_obj_t *)hwnd, 0));

            //start_time.min = prg_data->nMin / 60;
            //start_time.sec = prg_data->nMin % 60;
            end_time.min = max / 60;
            end_time.sec = max % 60;
//            sprintf(buf_left, "%02d:%02d", start_time.min, start_time.sec);
//            sprintf(buf_right, "%02d:%02d", end_time.min, end_time.sec);

            sprintf(buf_left, "%02d:%02d/", start_time.min, start_time.sec);
            sprintf(buf_right, "%02d:%02d", end_time.min, end_time.sec);
	    lv_label_set_text(lv_obj_get_child((lv_obj_t *)hwnd, 2), buf_right);
                        //SetWindowText(GetDlgItem(hwnd, IDC_LABEL_LEFT), buf_left);
            //SetWindowText(GetDlgItem(hwnd, IDC_LABEL_RIGHT), buf_right);
            //SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_SETRANGE, prg_data->nMin, prg_data->nMax);
            //SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_SETPOS, prg_data->nPos, 0);
        }
        break;
    case LV_EVENT_INSERT:
        {
            char buf_cur[20] = {0};
            PGBTime_t cur_time;
            int time = lv_slider_get_value(lv_obj_get_child((lv_obj_t *)hwnd, 0));
            cur_time.min = time / 60;
            cur_time.sec = time % 60;
            sprintf(buf_cur, "%02d:%02d/", cur_time.min, cur_time.sec);
            lv_label_set_text(lv_obj_get_child((lv_obj_t *)hwnd, 1), buf_cur);
            //SetWindowText(GetDlgItem(hwnd, IDC_LABEL_LEFT), buf_cur);
            //SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_SETPOS, prg_data->nPos, 0);
        }
        break;
    case PGBM_SETSTEP: {
        PROGRESSDATA *prg_data = (PROGRESSDATA *)wparam;
        //return SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_SETSTEP, prg_data->nStepInc, 0);
    }
    case PGBM_DELTAPOS:
        //return SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_DELTAPOS, wparam, 0);
    case PGBM_STEPIT: {
        char buf_cur[20] = {0};
        PGBTime_t cur_time;

        PROGRESSDATA *prg_data = (PROGRESSDATA *)wparam;
        prg_data->nPos += prg_data->nStepInc;

        if (prg_data->nPos > prg_data->nMax) break;

        cur_time.min = prg_data->nPos / 60;
        cur_time.sec = prg_data->nPos % 60;
        sprintf(buf_cur, "%02d:%02d/", cur_time.min, cur_time.sec);

        //SetWindowText(GetDlgItem(hwnd, IDC_LABEL_LEFT), buf_cur);
        //return SendMessage(GetDlgItem(hwnd, IDC_PROGRESSBAR), PBM_STEPIT, 0, 0);
    }
    default:
        return CustomWidget::HandleMessage( hwnd, message, wparam, lparam );
    }
    return HELP_ME_OUT;
}
/*
int ProgressBar::OnMouseUp(unsigned int button_status, int x, int y)
{
    RECT rect;
    ::GetClientRect(GetDlgItem(handle_, IDC_PROGRESSBAR), &rect);

    int s_x = 0, s_y = 0;
    ::ClientToScreen(GetDlgItem(handle_, IDC_PROGRESSBAR), &s_x, &s_y);

    float s_w = RECTW(rect);
    double seek = (x- s_x) / s_w * 100;

    int sec = seek / 100 * progress_data_.nMax;

#if 1
    db_debug("seek to: %f%%, sec: %d, progress max: %d", seek, sec, progress_data_.nMax);
#endif
    this->SetProgressSeekValue(sec);

    if (OnProgressSeek)
        OnProgressSeek(this, sec);
    return 0;
}
*/
int ProgressBar::OnMouseUp(unsigned int button_status, int x, int y)
{
    return 0;
}


void ProgressBar::SetProgressRange(int min, int max)
{
    progress_data_.nMin = min;
    progress_data_.nMax = max;
    progress_data_.seek_value = 0;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	lv_slider_set_range(lv_obj_get_child(handle_, 0), min, max);
    }
    lv_event_send(handle_, LV_EVENT_READY, NULL);
    //::SendMessage(handle_, PGBM_SETTIME_RANGE, (WPARAM)&progress_data_, 0);
}

void ProgressBar::SetProgressSeekValue(int pos)
{
    progress_data_.nPos = pos;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	lv_slider_set_value(lv_obj_get_child(handle_, 0), pos, LV_ANIM_ON);
    }
    lv_event_send(handle_, LV_EVENT_INSERT, NULL);
    //::SendMessage(handle_, PGBM_SETCURTIME, (WPARAM)&progress_data_, 0);
}

void ProgressBar::SetProgressStep(int step)
{
    progress_data_.nStepInc = step;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
        lv_obj_set_user_data(lv_obj_get_child(handle_, 0), (void *)step);
    }
    //SendMessage(handle_, PGBM_SETSTEP, (WPARAM)&progress_data_, 0);
}

void ProgressBar::UpdateProgressByStep()
{
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
        int step = (int)lv_obj_get_user_data(lv_obj_get_child(handle_, 0));
        int cur = lv_slider_get_value(lv_obj_get_child(handle_, 0));
	lv_slider_set_value(lv_obj_get_child(handle_, 0), step + cur, LV_ANIM_ON);
    }
    lv_event_send(handle_, LV_EVENT_INSERT , NULL);
    //::SendMessage(handle_, PGBM_STEPIT, (WPARAM)&progress_data_, 0);
}

int ProgressBar::SetProgressSeekValueByStep(int dir, int step)
{
    if (dir > 0) {
        progress_data_.seek_value += step;
    } else if (dir == 0) {
        progress_data_.seek_value -= step;
    }

    if (progress_data_.seek_value < 0) progress_data_.seek_value = 0;
    if (progress_data_.seek_value >= progress_data_.nMax) progress_data_.seek_value = progress_data_.nMax;

    SetProgressSeekValue(progress_data_.seek_value);
    return progress_data_.seek_value;
}
