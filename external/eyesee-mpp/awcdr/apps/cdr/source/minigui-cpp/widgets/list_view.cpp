/* *******************************************************************************
 * Copyright (C), 2001-2016, Allwinner Tech. Co., Ltd.
 * *******************************************************************************/
/**
 * @file list_viwe.cpp
 * @brief listview控件
 * @author id:826
 * @version v0.3
 * @date 2016-11-17
 */
#include "widgets/list_view.h"

#undef LOG_TAG
#define LOG_TAG "ListView"

using namespace std;

IMPLEMENT_DYNCRT_CLASS(ListView)

ListView::ListView(View *parent)
    :SystemWidget(parent)   
    #ifdef SETBG
    , bg_image_(NULL)
    #endif
{
    isKeyUp = true;
    downKey = 0;
    itemCont = 0;
    hi_idx_ = -1;
    db_msg(" ");
    pthread_mutex_init(&list_mutex, NULL);
}

ListView::~ListView()
{
#ifdef SETBG
    if (bg_image_) {
        UnloadBitmap(bg_image_);
    }
#endif
}

void ListView::GetCreateParams(CommonCreateParams &params)
{
    params.class_name = CTRL_LISTVIEW;
    params.alias      = GetClassName();
    //params.style      = WS_VISIBLE | WS_CHILD  | WS_VSCROLL;
    params.style      = WS_VISIBLE | WS_CHILD | LVS_NOTIFY;
    params.exstyle    = WS_EX_USEPARENTFONT;//WS_EX_TRANSPARENT;//WS_EX_USEPARENTFONT;
    params.x          = 0;
    params.y          = 0;
    params.w          = DEFAULT_CTRL_WIDTH;
    params.h          = DEFAULT_CTRL_HEIGHT;
}


void ListView::keyProc(int keyCode, int isLongPress)
{
    switch(keyCode){
#if 0
        case SDV_KEY_LEFT:
        {
            int ret = 0;
            if(itemCont >= GetItemCont()){
                itemCont = 0;
            }
            db_error("[debug_jaosn]:SDV_KEY_LEFT itemCont = %d ",itemCont);
            ret = SendMessage(handle_,LVM_CHOOSEITEM,itemCont,0);
            if(ret < 0)
            {
                db_error("[debug_jaosn]:Chose the listview item");
            }
            itemCont++;
            break;
        }
        case SDV_KEY_MODE:
            // this->DoHide();
            // static_cast<Window *>(parent_)->DoShow();
            break;
        case SDV_KEY_OK:
            break;
        case SDV_KEY_RIGHT:
        {
            int ret = 0;
            if(itemCont == 0){
                itemCont = GetItemCont() - 1;
            }else{
                //db_error("[debug_jaosn]:@@@@@SDV_KEY_RIGHT itemCont = %d",itemCont);
                itemCont = itemCont - 1;
                //db_error("[debug_jaosn]:@@@@@SDV_KEY_RIGHT itemCont = %d",itemCont);
            }
            //db_error("[debug_jaosn]:SDV_KEY_RIGHT itemCont = %d ",itemCont);
            ret = SendMessage(handle_,LVM_CHOOSEITEM,itemCont,0);
            break;
        }
        default:
            db_msg("[debug_joson]:invild keycode");
            break;
#endif
    }
}


int ListView::HandleMessage(HWND hwnd, int message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
#ifdef SETBG
        case MSG_ERASEBKGND: {
            HDC hdc = (HDC)wparam;
            const RECT* clip = (const RECT*) lparam;
            BOOL fGetDC = FALSE;
            RECT rcTemp;

            if (hdc == 0) {
                hdc = GetClientDC (hwnd);
                fGetDC = TRUE;
            }

            if (clip) {
                rcTemp = *clip;
                ScreenToClient (hwnd, &rcTemp.left, &rcTemp.top);
                ScreenToClient (hwnd, &rcTemp.right, &rcTemp.bottom);
                IncludeClipRect (hdc, &rcTemp);
            }
            else
                GetClientRect (hwnd, &rcTemp);

            if (bg_image_ != NULL) {
                db_msg("zhb---------retemp.left = %d  retemp.right = %d   RECTW(rcTemp) = %d    RECTH(rcTemp) = %d",rcTemp.left,rcTemp.right,RECTW(rcTemp), RECTH(rcTemp));
                FillBoxWithBitmap (hdc, 0, 0,
                                   RECTW(rcTemp), RECTH(rcTemp), bg_image_);
            }else {                    // <C7><E5><B3><FD><CE><DE>Ч<C7><F8><D3><F2>, <C8><E7><B9><FB>û<D3><D0>Ϊ<B4><B0><BF><DA><C9><E8><D6>ñ<B3><BE><B0>ͼƬ<A3><AC><D4><F2><D2><D4><U+0378><C3><U+1F1CFE><B0><CC><EE><B3><E4>
                       db_msg("zhb-------------111--------MSG_ERASEBKGND");
                                       SetBrushColor (hdc, RGBA2Pixel (hdc, 0xFF, 0x0d, 0x02, 0x46));
                       FillBox (hdc, rcTemp.left, rcTemp.top, RECTW(rcTemp), RECTH(rcTemp));
                       }

            if (fGetDC)
                ReleaseDC (hdc);
            return 0;
        }
#endif
        case MSG_KEYDOWN:
            {
                db_msg("[debug_jaosn]:short MSG_KEYDOWN");
                keyProc(wparam, SHORT_PRESS);
            }
            break;
#if 0
        case MSG_KEYUP:
            {
                db_msg("[debug_jaosn]:short MSG_KEYDOWN");
                if(isKeyUp == true) {
                    downKey = wparam;
                    SetTimer(hwnd, ID_LISTVIEW_TIMER_KEY, LONG_PRESS_TIME);
                    isKeyUp = false;
                }
                break;
            }
        case MSG_KEYLONGPRESS:
            {
                db_msg("[debug_jaosn]:long press\n");
                downKey = -1;
                keyProc(wparam, LONG_PRESS);
                break;
            }
        case MSG_TIMER:
            {
                if(wparam == ID_LISTVIEW_TIMER_KEY) {
                    db_msg("[debug_jaosn]:short MSG_TIMER");
                    isKeyUp = true;
                    SendMessage(hwnd, MSG_KEYLONGPRESS, downKey, 0);
                    KillTimer(hwnd, ID_LISTVIEW_TIMER_KEY);
                }
                break;
            }
#endif
        case MSG_LBUTTONDOWN:
			break;
        case MSG_LBUTTONUP:
            break;
        case MSG_MOUSEMOVE:
            break;
        default:
            return SystemWidget::HandleMessage( hwnd, message, wparam, lparam );
            break;

    }
    return HELP_ME_OUT;
}

int ListView::OnMouseUp(unsigned int button_status, int x, int y)
{
    #if 0
    db_msg("button status: %d, x: %d, y: %d", button_status, x, y);

    if(OnItemClick)
        OnItemClick(this);

     return HELP_ME_OUT;
    #endif
 }

int ListView::SetColumns(std::vector<LVCOLUMN> columns, bool auto_width)
{
    int ret = 0;
    vector<LVCOLUMN>::iterator it;

    // set column/head height
    //ret = SendMessage (handle_, LVM_SETHEADHEIGHT, 0, 0);
    if (ret < 0) {
        db_error("set column head height failed");
    }
    column = columns;
    for (it = column.begin(); it != column.end(); ++it) {
            int h;
	    int x_offset = 0;
	    if (auto_width || it->width <= 0) {
                RECT rect;
                this->GetRect(&rect);
                it->width = RECTW(rect) / column.size();
                db_msg("column width: %d", it->width);
            }
    }
    // set column data
    /*
    for (it = column.begin(); it != column.end(); ++it) {
        int h;
	if (auto_width || it->width <= 0) {
            RECT rect;
            this->GetRect(&rect);
            it->width = RECTW(rect) / column.size();
            db_msg("column width: %d", it->width);
        }

        it->nCols = it - column.begin();
        printf("SetColumns:%p\n", handle_);
	printf("nCols:%d\n", it->nCols);
	printf("width:%d\n", it->width);
	if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	    int x_offset = 0;
	    for(int i = 0; i < lv_obj_get_child_cnt(handle_); i++) {
                x_offset += (int)lv_obj_get_user_data(lv_obj_get_child(handle_, i));
	    }
	    lv_obj_t *column_handle = lv_obj_create(handle_);
	    lv_obj_set_user_data(column_handle, (void *)it->width);
	    lv_obj_set_x(column_handle, x_offset);
            lv_obj_set_y(column_handle, 0);
            lv_obj_set_width(column_handle, it->width);
            lv_obj_set_height(column_handle, lv_pct(100));
	    x_offset+=it->width;
	    lv_obj_clear_flag(column_handle, LV_OBJ_FLAG_SCROLLABLE);
	    lv_obj_set_style_pad_top(column_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_bottom(column_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_left(column_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_right(column_handle, 0, LV_PART_MAIN);
	    lv_obj_set_style_border_width(column_handle, 0, 0);
	    lv_obj_set_style_outline_width(column_handle, 0, 0);
	    lv_obj_set_style_outline_pad(column_handle, 0, 0);
	    lv_obj_set_style_shadow_width(column_handle, 0, 0);
	    lv_obj_set_style_bg_color(column_handle, lv_color_hex(back_color_), 0);
            lv_obj_add_flag(column_handle, LV_OBJ_FLAG_EVENT_BUBBLE);
	    lv_obj_set_scrollbar_mode(column_handle, LV_SCROLLBAR_MODE_AUTO);
	    lv_obj_add_flag(column_handle, LV_OBJ_FLAG_SCROLLABLE);
	    //lv_obj_add_event_cb(column_handle, scroll_event_cb, LV_EVENT_SCROLL, NULL);
	}*/
	//ret = SendMessage (handle_, LVM_ADDCOLUMN, 0, (LPARAM) & (*it));
 

    return ret;
}

int ListView::SetColHead(std::vector<LVCOLUMN> columns)
{
	//LVM_MODIFYHEAD
	 int ret = 0;
        vector<LVCOLUMN>::iterator it;	
	for (it = columns.begin(); it != columns.end(); ++it) {
        it->nCols = it - columns.begin();
        //ret = SendMessage (handle_, LVM_MODIFYHEAD, 0, (LPARAM) & (*it));
        if (ret < 0) {
            db_error("SetColHead  failed");
            break;
        }
    }
    return ret;
}


int ListView::AddItems(std::vector<LVITEM> items)
{
    int ret = 0;

    vector<LVITEM>::iterator it;
    for ( it = items.begin(); it != items.end(); ++it) {

        it->nItem = it - items.begin();
	//int column_handle = lv_obj_get_child(handle_, it->nItem);
	//printf("column handle:%p\n", column_handle);
        //ret = SendMessage (handle_, LVM_ADDITEM, 0, (LPARAM) & (*it));

        if (ret == 0) {
            db_error("add item failed, ret: %d", ret);
        }
    }

    return ret;
}

int ListView::SetItemText(std::string text, int row, int col)
{
    int ret = 0;

    LVSUBITEM data;
    data.nItem = row;
    data.subItem = col;
    data.pszText = const_cast<char*>(text.c_str());
    printf("SetItemText:%s\n", text.c_str());
    pthread_mutex_lock(&list_mutex);
    lv_obj_t *item = lv_obj_get_child(handle_, row);
    if (item == NULL) {
        db_warn("can not get row");
        return -1;
    }

    //ret = SendMessage(handle_, LVM_GETSUBITEMTEXT, 0, (LPARAM) &item_data);
    lv_obj_t *sub_item = lv_obj_get_child(item, col);
    if (sub_item == NULL) {
        db_error("get row: %d, sub: %d item text failed, ret: %d", row, col, ret);
    }
    lv_label_set_text(lv_obj_get_child(sub_item, 0), text.c_str());
    pthread_mutex_unlock(&list_mutex);

    //ret = SendMessage (handle_, LVM_SETSUBITEMTEXT, 0, (LPARAM) & (data));
    if (ret < 0) {
        db_error("set item text failed, ret: %d", ret);
    }

    return ret;
}
/*
void ListView::scroll_event_cb(lv_event_t * event)
{
    lv_obj_t *obj = lv_event_get_target(event);
    if (lv_event_get_code(event) == LV_EVENT_SCROLL) {
        printf("scroll\n");
        printf("y = %d\n", lv_obj_get_scroll_y(obj));
    }
}*/


int ListView::AddItem(LVITEM &item)
{
    int ret = 0;
    //ret = SendMessage (handle_, LVM_ADDITEM, 0, (LPARAM) & item);
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	pthread_mutex_lock(&list_mutex);
	int y_offset = 0;
	printf("item cnt brfore : %d\n", lv_obj_get_child_cnt(handle_));

	for(int i = 0; i < lv_obj_get_child_cnt(handle_); i++) {
                y_offset += (int)lv_obj_get_user_data(lv_obj_get_child(handle_, i));
        }
	lv_obj_t *item_handle = lv_obj_create(handle_);
	lv_obj_set_user_data(item_handle, (void *)item.nItemHeight);
	lv_obj_set_x(item_handle, 0);
        lv_obj_set_y(item_handle, y_offset);
        printf("y_offset : %d\n", y_offset);
        lv_obj_set_width(item_handle, lv_pct(100));
        lv_obj_set_height(item_handle, item.nItemHeight);
        lv_obj_set_style_bg_color(item_handle, lv_color_hex(back_color_), 0);
        lv_obj_set_style_radius(item_handle, 0, 0);
	lv_obj_clear_flag(item_handle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(item_handle, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(item_handle, LV_OBJ_FLAG_EVENT_BUBBLE);
	lv_obj_set_style_pad_top(item_handle, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(item_handle, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(item_handle, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_right(item_handle, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(item_handle, 1, 0);
	lv_obj_set_style_border_color(item_handle, lv_color_black(), 0);
	//lv_obj_set_style_border_opa(item_handle, 0, 0);
	//lv_obj_set_style_outline_width(item_handle, 0, 0);
	//lv_obj_set_style_outline_pad(item_handle, 0, 0);
	//lv_obj_set_style_shadow_width(item_handle, 0, 0);
	lv_obj_add_event_cb(item_handle, ListSelectCb, LV_EVENT_CLICKED, NULL);
	vector<LVCOLUMN>::iterator it;
	int x_offset = 0;
        for (it = column.begin(); it != column.end(); ++it) {
            int h;
            it->nCols = it - column.begin();

            lv_obj_t *sub_handle = lv_obj_create(item_handle);
	    lv_obj_set_x(sub_handle, x_offset);
            lv_obj_set_y(sub_handle, 0);
            lv_obj_set_width(sub_handle, it->width);
            lv_obj_set_height(sub_handle, lv_pct(100));
	    x_offset+=it->width;
	    lv_obj_clear_flag(sub_handle, LV_OBJ_FLAG_SCROLLABLE);
	    lv_obj_set_style_pad_top(sub_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_bottom(sub_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_left(sub_handle, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_right(sub_handle, 0, LV_PART_MAIN);
	    lv_obj_set_style_border_width(sub_handle, 0, 0);
	    lv_obj_set_style_outline_width(sub_handle, 0, 0);
	    lv_obj_set_style_outline_pad(sub_handle, 0, 0);
	    lv_obj_set_style_shadow_width(sub_handle, 0, 0);
	    lv_obj_set_style_bg_opa(sub_handle, 0, 0);
            lv_obj_add_flag(sub_handle, LV_OBJ_FLAG_EVENT_BUBBLE);
	    lv_obj_set_scrollbar_mode(sub_handle, LV_SCROLLBAR_MODE_OFF);
	    lv_obj_add_flag(sub_handle, LV_OBJ_FLAG_SCROLLABLE);
	    lv_obj_clear_flag(sub_handle, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_radius(sub_handle, 0, 0);
        }
	pthread_mutex_unlock(&list_mutex);
    }
    if (ret == 0) {
        db_error("add item failed, ret: %d", ret);
        return ret;
    }
    return ret;
}


int ListView::FillSubItem(LVSUBITEM &data)
{
    int ret = 0;
    //ret = SendMessage (handle_, LVM_FILLSUBITEM, 0, (LPARAM) &data);
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	pthread_mutex_lock(&list_mutex);
        lv_obj_t *item_handle = lv_obj_get_child(handle_, data.nItem);
        lv_obj_t *sub_handle = lv_obj_get_child(item_handle, data.subItem);
	if (lv_obj_get_child_cnt(sub_handle) != 0)
		lv_obj_del(lv_obj_get_child(sub_handle, 0));
        if (data.flags != LVFLAG_BITMAP) {
	    lv_obj_t *text_handle = lv_label_create(sub_handle);
	    lv_obj_center(text_handle);
	    lv_label_set_text(text_handle, data.pszText);
	    lv_obj_set_style_text_color(text_handle, lv_color_hex(data.nTextColor), 0);
	    lv_label_set_long_mode(text_handle, LV_LABEL_LONG_SCROLL);
	    printf("data.pszText:%s\n", data.pszText);
	} else {
            lv_obj_t *img_handle = lv_img_create(sub_handle);
	    lv_obj_center(img_handle);
	    printf("data.image:%s\n", data.image);
	    lv_img_set_src(img_handle, data.image);
	}
	pthread_mutex_unlock(&list_mutex);
        if (ret < 0) {
            db_error("LVM_FILLSUBITEM failed, ret: %d", ret);
            return ret;
        }
    }
        return ret;
}

int ListView::UpdateItemData(LVSUBITEM data, int row, int col)
{
    int ret = 0;

    data.nItem = row;
    data.subItem = col;

    //ret = SendMessage (handle_, LVM_SETSUBITEM, 0, (LPARAM) & (data));
    if (ret < 0) {
        db_error("update item data failed, ret: %d", ret);
    }

    return ret;
}

int ListView::InsertItemDatas(std::vector<LVSUBITEM> datas, int row)
{
    int ret = 0;

    vector<LVSUBITEM>::iterator it;
    for ( it = datas.begin(); it != datas.end(); ++it) {
        it->nItem = row;
        it->subItem = it - datas.begin();
        //ret = SendMessage (handle_, LVM_SETSUBITEM, 0, (LPARAM) & (*it));
	
        if (ret < 0) {
            db_error("insert item datas failed, ret: %d", ret);
        }
    }

    return ret;
}

int ListView::AddItemWithDatas(LVITEM &item, std::vector<LVSUBITEM> datas)
{
    int ret = 0;

    //ret = SendMessage (handle_, LVM_ADDITEM, 0, (LPARAM) & item);

    if (ret == 0) {
        db_error("add item failed, ret: %d", ret);
        return ret;
    }

    ret = InsertItemDatas(datas, item.nItem);

    return ret;
}

int ListView::RemoveItem(int row)
{
    int ret = 0;
    printf(" ListView::RemoveItem\n");

    //ret = SendMessage(handle_, LVM_DELITEM, (WPARAM)row, 0);

    if (ret < 0) {
        db_error("remove item [%d] failed, ret: %d", row, ret);
    }

    return ret;
}

int ListView::RemoveItem(HLVITEM item)
{
    int ret = 0;
    printf(" ListView::RemoveItem\n");
    //ret = SendMessage(handle_, LVM_DELITEM, 0, (LPARAM)item);

    if (ret < 0) {
        db_error("remove item [%d] failed, ret: %d", item, ret);
    }

    return ret;
}

int ListView::RemoveAllItems()
{
    int ret = 0;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
        //int item_num = lv_obj_get_child_cnt(handle_);
        /*while (lv_obj_get_child_cnt(handle_)) {
            lv_obj_del(lv_obj_get_child(handle_, 0));
        }*/
        //printf("cnt %d\n", lv_obj_get_child_cnt(handle_));
	pthread_mutex_lock(&list_mutex);
	if (lv_obj_get_child_cnt(handle_) > 0) {
            lv_obj_clean(handle_);
	}
	pthread_mutex_unlock(&list_mutex);

    }
    //printf("ListView::RemoveItem %d\n", lv_obj_get_child_cnt(handle_));


    //ret = SendMessage(handle_, LVM_DELALLITEM, 0, 0);

    if (ret < 0) {
        db_error("remove all item failed, ret: %d", ret);
    }

    return ret;
}


int ListView::GetItemCont()
{
    int ret = 0;
    //ret = SendMessage(handle_,LVM_GETITEMCOUNT,0,0);
    db_msg("[debug_joasn]: item cont is %d",ret);
    if(ret <= 0)
    {
        db_error("[debug_jaosn]: get item is filed");
        return -1;
    }

    return ret;
}

void ListView::SetWindowBackImage(const char *bmp)
{
#ifdef SETBG

    if (bmp == NULL) {
        db_error("failed, bmp = null");
        return;
    }

    if (bg_image_ == NULL) {
        bg_image_ = (BITMAP*)malloc(sizeof(BITMAP));
    }

    LoadBitmap(HDC_SCREEN, bg_image_, bmp);
#endif
}


int ListView::CancleAllHilight()
{
    return 0;
}
int ListView::SelectItem(int row)
{
    int ret = 0;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
        lv_obj_set_user_data(handle_, (void *)row);
    }

    //ret = SendMessage(handle_, LVM_CHOOSEITEM, row, 0);
    /*if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	int column_num = lv_obj_get_child_cnt(handle_);
	for (int i = 0; i < column_num; i++) {
            lv_obj_t *column_handle = lv_obj_get_child(handle_, i);
	    for (i = 0; i < lv_obj_get_child_cnt(column_handle); i++) {
                lv_obj_t *item_handle = lv_obj_get_child(column_handle, i);
                if (i == row) {
		    lv_obj_add_state(item_handle, LV_STATE_CHECKED);
                } else {
                    lv_obj_clear_state(item_handle, LV_STATE_CHECKED);
                }
	    //lv_obj_set_style_bg_color(item_handle, lv_color_blue(), 0);
	    }
	}
     }
    if (ret < 0) {
        db_error("set select item failed row %d",row);
    } else {
        hi_idx_ = row;
    }*/

    return ret;
}
int ListView::SetSelectedItemKeyDown()
{
    //SendMessage(handle_, MSG_KEYDOWN, 0, 0);
    return 0;
}

int ListView::GetSelectedItem(LVITEM &lvitem)
{
    HLVITEM item_handle;

    return GetSelectedItem(item_handle, lvitem);
}

int ListView::GetSelectedItem(HLVITEM &item_handle, LVITEM &lvitem)
{
    //item_handle = (HLVITEM)SendMessage(handle_, LVM_GETSELECTEDITEM, 0, (LPARAM) &lvitem);
    lvitem.nItem = (int)lv_obj_get_user_data(handle_);
    if (item_handle == 0) {
        db_error("get selected item failed, item handle == 0");
        return -1;
    }

    return 0;
}

void ListView::ListSelectCb(lv_event_t * event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
	lv_obj_t *item = lv_event_get_target(event);
	lv_obj_t *list = lv_obj_get_parent(item);
	int item_pre = (int)lv_obj_get_user_data(list);
        int i = 0;
        for(i = 0; i < lv_obj_get_child_cnt(list); i++) {
            if (lv_obj_get_child(list, i) == item) {
		lv_obj_set_user_data(list, (void *)i);
		break;
	    }

        }
	//if (i  != item_pre) {
            lv_event_send(lv_obj_get_parent(list), LV_EVENT_REFRESH, NULL);
	//}
    }
}

string ListView::GetItemText(HLVITEM item, int col)
{
    int ret = 0;
    LVSUBITEM item_data;

    item_data.subItem = col;

    int len = 0;
    //SendMessage(handle_, LVM_GETSUBITEMLEN, (WPARAM)item, (LPARAM) &item_data);

    if (len <= 0) {
        db_warn("why subitem len <= 0 ?");
        return "";
    }

    item_data.pszText = new char[len + 1];
    if (item_data.pszText == nullptr) {
        db_warn("alloc failed");
        return "";
    }
    //ret = SendMessage(handle_, LVM_GETSUBITEMTEXT, (WPARAM)item, (LPARAM) &item_data);

    if (ret < 0) {
        db_error("get item sub: %d item text failed, ret: %d", col, ret);
        return string();
    }

    string text_str = item_data.pszText;
    delete []item_data.pszText;

    return text_str;
}

string ListView::GetItemText(int row, int col)
{
    int ret = 0;
    LVSUBITEM item_data;

    item_data.nItem = row;
    item_data.subItem = col;

    int len = 0;
    //SendMessage(handle_, LVM_GETSUBITEMLEN, 0, (LPARAM) &item_data);
    lv_obj_t *item = lv_obj_get_child(handle_, row);
    if (item == NULL) {
        db_warn("can not get row");
        return "";
    }

    //ret = SendMessage(handle_, LVM_GETSUBITEMTEXT, 0, (LPARAM) &item_data);
    lv_obj_t *sub_item = lv_obj_get_child(item, col);

    if (sub_item == NULL) {
        db_error("get row: %d, sub: %d item text failed, ret: %d", row, col, ret);
    }
    const char* str = lv_label_get_text(lv_obj_get_child(sub_item, 0));
    std::string text_str(str);

    return text_str;
}
int ListView::GetHilight()
{
    return hi_idx_;
}

int ListView::SetHilight(int row)
{
    int ret = 0;
    if ((handle_ != HWND_NULL) && (handle_ != HWND_INVALID)) {
	int item_num = lv_obj_get_child_cnt(handle_);
	for (int i = 0; i < item_num; i++) {
            //lv_obj_t *item_handle = lv_obj_get_child(column_handle, row);
	    if (i == row)
		lv_obj_set_style_bg_color(lv_obj_get_child(handle_, i), lv_color_hex(highlight_color), 0);
	    else
	        lv_obj_set_style_bg_color(lv_obj_get_child(handle_, i), lv_color_hex(back_color_), 0);
	}
     }
    //ret = SendMessage(handle_, LVM_SELECTITEM, row, 0);
    if (ret < 0) {
        db_error("set select item failed");
    } else {
        //ret = SendMessage(handle_, LVM_SHOWITEM, row, 0);
        if (ret < 0) {
            db_error("show item failed");
        } else {
            hi_idx_ = row;
        }
    }

    return ret;
}
void ListView::SetHilightColor(DWORD color)
{
	highlight_color = color;
}
