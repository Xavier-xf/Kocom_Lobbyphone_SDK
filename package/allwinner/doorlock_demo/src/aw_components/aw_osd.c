#include "aw_osd.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "char_conversion.h"

unsigned int BGRA(unsigned char B, unsigned char G, unsigned char R, unsigned char A)
{
    uint32_t color = 0;

    color |= (B << 0);
    color |= (G << 8);
    color |= (R << 16);
    color |= (A << 24);
    return color;
}

unsigned int RGBA(unsigned char R, unsigned char G, unsigned char B, unsigned char A)
{
    uint32_t color = 0;

    color |= (R << 0);
    color |= (G << 8);
    color |= (B << 16);
    color |= (A << 24);
    return color;
}

unsigned int ARGB(unsigned char A, unsigned char R, unsigned char G, unsigned char B)
{
    uint32_t color = 0;

    color |= (A << 0);
    color |= (R << 8);
    color |= (G << 16);
    color |= (B << 24);
    return color;
}

unsigned int ABGR(unsigned char A, unsigned char B, unsigned char G, unsigned char R)
{
    uint32_t color = 0;

    color |= (A << 0);
    color |= (B << 8);
    color |= (G << 16);
    color |= (R << 24);
    return color;
}

void COLOR_RGB_TO_YUV(uint8_t R, uint8_t G, uint8_t B, uint8_t *Y, uint8_t *U, uint8_t *V)
{
    int tmp_y, tmp_u, tmp_v;

    tmp_y = 0.299 * R + 0.587 * G + 0.114 * B;
    tmp_u = -0.1687 * R - 0.3313 * G + 0.5 * B + 128;
    tmp_v = 0.5 * R - 0.4187 * G - 0.0813 * B + 128;
    tmp_y = tmp_y > 255 ? 255 : tmp_y;
    tmp_u = tmp_u > 255 ? 255 : tmp_u;
    tmp_v = tmp_v > 255 ? 255 : tmp_v;
    tmp_y = tmp_y < 0 ? 0 : tmp_y;
    tmp_u = tmp_u < 0 ? 0 : tmp_u;
    tmp_v = tmp_v < 0 ? 0 : tmp_v;

    *Y = tmp_y;
    *U = tmp_u;
    *V = tmp_v;
}

void COLOR_YUV_TO_RGB(uint8_t Y, uint8_t U, uint8_t V, uint8_t *R, uint8_t *G, uint8_t *B)
{
    int tmp_r, tmp_g, tmp_b;

    tmp_r = Y + 1.402 * (V - 128);
    tmp_g = Y - 0.34414 * (U - 128) - 0.71414 * (V - 128);
    tmp_b = Y + 1.772 * (U - 128);
    tmp_r = tmp_r > 255 ? 255 : tmp_r;
    tmp_g = tmp_g > 255 ? 255 : tmp_g;
    tmp_b = tmp_b > 255 ? 255 : tmp_b;
    tmp_r = tmp_r < 0 ? 0 : tmp_r;
    tmp_g = tmp_g < 0 ? 0 : tmp_g;
    tmp_b = tmp_b < 0 ? 0 : tmp_b;

    *R = tmp_r;
    *G = tmp_g;
    *B = tmp_b;
}

void draw_point(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
               unsigned int pix_h, int x, int y, unsigned int value)
{
    if (x < 0 || y < 0 || x >= pix_w || y > pix_h) {
        DOORLOCK_DBG("error, check your param!\n");
        return;
    }
    if (bytes_per_pix > 4 || bytes_per_pix == 0) {
        DOORLOCK_DBG("bytes_per_pix error %d\n", bytes_per_pix);
        return;
    }
    unsigned char *dst_data = &data_array[((y * pix_w) + x) * bytes_per_pix];
    unsigned char *src_data = (unsigned char *)&value;
    memcpy(dst_data, src_data, bytes_per_pix);
}

void draw_point_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                   unsigned int pix_h, int x, int y, color_yuv_t nv21_value)
{
    if (x < 0 || y < 0 || x >= pix_w || y > pix_h) {
        DOORLOCK_DBG("error, check your param!\n");
        return;
    }
    int y_offset = (y * pix_w) + x;
    y_data_array[y_offset] = nv21_value.y;

    int v_offset = (y / 2) * pix_w + x / 2 * 2;
    int u_offset = v_offset + 1;

    uv_data_array[v_offset] = nv21_value.v;
    uv_data_array[u_offset] = nv21_value.u;
}

void draw_point_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x, int y,
                   unsigned char gray_value)
{
    draw_point(data_array, 1, pix_w, pix_h, x, y, (unsigned int)gray_value);
}

void draw_point_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x,
                     int y, color_argb_t argb_value)
{
    unsigned int *color_value = (unsigned int *)&argb_value;

    draw_point(data_array, 4, pix_w, pix_h, x, y, *color_value);
}

void draw_point_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x,
                     int y, color_abgr_t abgr_value)
{
    unsigned int *color_value = (unsigned int *)&abgr_value;

    draw_point(data_array, 4, pix_w, pix_h, x, y, *color_value);
}

void draw_line_single(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
                    unsigned int pix_h, int x1, int y1, int x2, int y2, unsigned int value)
{
    if (x1 < 0 || y1 < 0 || x1 >= pix_w || y1 > pix_h) {
        DOORLOCK_DBG("error, check your param!, x1 = %d, y1 = %d, pix_w = %d, pix_h = %d\n", x1, y1,
                    pix_w, pix_h);
        return;
    }
    if (x2 < 0 || y2 < 0 || x2 >= pix_w || y2 > pix_h) {
        DOORLOCK_DBG("error, check your param!, x1 = %d, y1 = %d, pix_w = %d, pix_h = %d\n", x1, y1,
                    pix_w, pix_h);
        return;
    }
    if (bytes_per_pix > 4 || bytes_per_pix == 0) {
        DOORLOCK_DBG("bytes_per_pix error %d\n", bytes_per_pix);
        return;
    }
    {
        // 布雷森汉姆算法画线
        unsigned int dx = abs(x1 - x2);
        unsigned int dy = abs(y1 - y2);
        int cur_x = x1, cur_y = y1;
        int n_step = 0;
        int x_step = (x1 < x2) ? 1 : -1;
        int y_step = (y1 < y2) ? 1 : -1;
        if (dx > dy) {
            int eps = 2 * dy - dx;
            while (n_step < dx) {
                if (eps >= 0) {
                    draw_point(data_array, bytes_per_pix, pix_w, pix_h, cur_x, cur_y, value);
                    cur_y = cur_y + y_step;
                    eps = eps + 2 * dy - 2 * dx;
                } else {
                    draw_point(data_array, bytes_per_pix, pix_w, pix_h, cur_x, cur_y, value);
                    eps = eps + 2 * dy;
                }
                cur_x = cur_x + x_step;
                n_step++;
            }
        } else {
            int eps = 2 * dx - dy;
            while (n_step < dy) {
                if (eps >= 0) {
                    draw_point(data_array, bytes_per_pix, pix_w, pix_h, cur_x, cur_y, value);
                    cur_x = cur_x + x_step;
                    eps = eps + 2 * dx - 2 * dy;
                } else {
                    draw_point(data_array, bytes_per_pix, pix_w, pix_h, cur_x, cur_y, value);
                    eps = eps + 2 * dx;
                }
                cur_y = cur_y + y_step;
                n_step++;
            }
        }
    }
}

void draw_line(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
              unsigned int pix_h, int x1, int y1, int x2, int y2, unsigned int value,
              unsigned int line_width)
{
    int x_step = (x1 <= x2) ? 1 : -1;
    int y_step = (y1 <= y2) ? 1 : -1;

    if (x1 == x2) {
        y_step = 0;
    }
    if (y1 == y2) {
        x_step = 0;
    }
    if (line_width <= 0) {
        line_width = 1;
    }

    int tmp_x1, tmp_x2, tmp_y1, tmp_y2;
    for (unsigned int i = 0; i < line_width; i++) {
        tmp_x1 = x1 + x_step * i;
        tmp_y1 = y1 + y_step * i;
        tmp_x2 = x2 + x_step * i;
        tmp_y2 = y2 + y_step * i;
        tmp_x1 = (tmp_x1 < 0) ? 0 : tmp_x1;
        tmp_x1 = (tmp_x1 >= pix_w) ? (pix_w - 1) : tmp_x1;
        tmp_x2 = (tmp_x2 < 0) ? 0 : tmp_x2;
        tmp_x2 = (tmp_x2 >= pix_w) ? (pix_w - 1) : tmp_x2;
        tmp_y1 = (tmp_y1 < 0) ? 0 : tmp_y1;
        tmp_y1 = (tmp_y1 >= pix_h) ? (pix_h - 1) : tmp_y1;
        tmp_y2 = (tmp_y2 < 0) ? 0 : tmp_y2;
        tmp_y2 = (tmp_y2 >= pix_h) ? (pix_h - 1) : tmp_y2;

        draw_line_single(data_array, bytes_per_pix, pix_w, pix_h, tmp_x1, tmp_y1, tmp_x2, tmp_y2,
                       value);
    }
}

void draw_line_single_nv21(unsigned char *y_data_array, unsigned char *uv_data_array,
                        unsigned int pix_w, unsigned int pix_h, int x1, int y1, int x2, int y2,
                        color_yuv_t nv21_value)
{
    if (x1 < 0 || y1 < 0 || x1 >= pix_w || y1 > pix_h) {
        DOORLOCK_DBG("error, check your param!, x1 = %d, y1 = %d, pix_w = %d, pix_h = %d\n", x1, y1,
                    pix_w, pix_h);
        return;
    }
    if (x2 < 0 || y2 < 0 || x2 >= pix_w || y2 > pix_h) {
        DOORLOCK_DBG("error, check your param!, x1 = %d, y1 = %d, pix_w = %d, pix_h = %d\n", x1, y1,
                    pix_w, pix_h);
        return;
    }

    {
        // 布雷森汉姆算法画线
        unsigned int dx = abs(x1 - x2);
        unsigned int dy = abs(y1 - y2);
        int cur_x = x1, cur_y = y1;
        int n_step = 0;
        int x_step = (x1 < x2) ? 1 : -1;
        int y_step = (y1 < y2) ? 1 : -1;
        if (dx > dy) {
            int eps = 2 * dy - dx;
            while (n_step < dx) {
                if (eps >= 0) {
                    draw_point_nv21(y_data_array, uv_data_array, pix_w, pix_h, cur_x, cur_y,
                                  nv21_value);
                    cur_y = cur_y + y_step;
                    eps = eps + 2 * dy - 2 * dx;
                } else {
                    draw_point_nv21(y_data_array, uv_data_array, pix_w, pix_h, cur_x, cur_y,
                                  nv21_value);
                    eps = eps + 2 * dy;
                }
                cur_x = cur_x + x_step;
                n_step++;
            }
        } else {
            int eps = 2 * dx - dy;
            while (n_step < dy) {
                if (eps >= 0) {
                    draw_point_nv21(y_data_array, uv_data_array, pix_w, pix_h, cur_x, cur_y,
                                  nv21_value);
                    cur_x = cur_x + x_step;
                    eps = eps + 2 * dx - 2 * dy;
                } else {
                    draw_point_nv21(y_data_array, uv_data_array, pix_w, pix_h, cur_x, cur_y,
                                  nv21_value);
                    eps = eps + 2 * dx;
                }
                cur_y = cur_y + y_step;
                n_step++;
            }
        }
    }
}

void draw_line_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                  unsigned int pix_h, int x1, int y1, int x2, int y2, color_yuv_t nv21_value,
                  unsigned int line_width)
{
    int x_step = (x1 <= x2) ? 1 : -1;
    int y_step = (y1 <= y2) ? 1 : -1;

    if (x1 == x2) {
        y_step = 0;
    }
    if (y1 == y2) {
        x_step = 0;
    }
    if (line_width <= 0) {
        line_width = 1;
    }

    int tmp_x1, tmp_x2, tmp_y1, tmp_y2;
    for (unsigned int i = 0; i < line_width; i++) {
        tmp_x1 = x1 + x_step * i;
        tmp_y1 = y1 + y_step * i;
        tmp_x2 = x2 + x_step * i;
        tmp_y2 = y2 + y_step * i;
        tmp_x1 = (tmp_x1 < 0) ? 0 : tmp_x1;
        tmp_x1 = (tmp_x1 >= pix_w) ? (pix_w - 1) : tmp_x1;
        tmp_x2 = (tmp_x2 < 0) ? 0 : tmp_x2;
        tmp_x2 = (tmp_x2 >= pix_w) ? (pix_w - 1) : tmp_x2;
        tmp_y1 = (tmp_y1 < 0) ? 0 : tmp_y1;
        tmp_y1 = (tmp_y1 >= pix_h) ? (pix_h - 1) : tmp_y1;
        tmp_y2 = (tmp_y2 < 0) ? 0 : tmp_y2;
        tmp_y2 = (tmp_y2 >= pix_h) ? (pix_h - 1) : tmp_y2;

        draw_line_single_nv21(y_data_array, uv_data_array, pix_w, pix_h, tmp_x1, tmp_y1, tmp_x2,
                           tmp_y2, nv21_value);
    }
}

void draw_line_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1, int y1,
                  int x2, int y2, unsigned char gray_value, unsigned int line_width)
{
    draw_line(data_array, 1, pix_w, pix_h, x1, y1, x2, y2, (unsigned int)gray_value, line_width);
}

void draw_line_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1,
                    int y1, int x2, int y2, color_argb_t argb_value, unsigned int line_width)
{
    unsigned int *color_value = (unsigned int *)&argb_value;

    draw_line(data_array, 4, pix_w, pix_h, x1, y1, x2, y2, *color_value, line_width);
}

void draw_line_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1,
                    int y1, int x2, int y2, color_abgr_t abgr_value, unsigned int line_width)
{
    unsigned int *color_value = (unsigned int *)&abgr_value;

    draw_line(data_array, 4, pix_w, pix_h, x1, y1, x2, y2, *color_value, line_width);
}

void draw_rect(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
              unsigned int pix_h, int left, int top, int right, int bottom, unsigned int value,
              unsigned int line_width)
{
    draw_line(data_array, bytes_per_pix, pix_w, pix_h, left, top, left, bottom, value, line_width);
    draw_line(data_array, bytes_per_pix, pix_w, pix_h, right, top, right, bottom, value, line_width);
    draw_line(data_array, bytes_per_pix, pix_w, pix_h, left, top, right, top, value, line_width);
    draw_line(data_array, bytes_per_pix, pix_w, pix_h, left, bottom, right + line_width, bottom,
             value, line_width);
}

void draw_rect_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                  int top, int right, int bottom, unsigned char gray_value, unsigned int line_width)
{
    draw_rect(data_array, 1, pix_w, pix_h, left, top, right, bottom, (unsigned int)gray_value,
             line_width);
}

void draw_rect_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                    int top, int right, int bottom, color_argb_t argb_value,
                    unsigned int line_width)
{
    unsigned int *color_value = (unsigned int *)&argb_value;

    draw_rect(data_array, 4, pix_w, pix_h, left, top, right, bottom, *color_value, line_width);
}

void draw_rect_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                    int top, int right, int bottom, color_abgr_t abgr_value,
                    unsigned int line_width)
{
    unsigned int *color_value = (unsigned int *)&abgr_value;

    draw_rect(data_array, 4, pix_w, pix_h, left, top, right, bottom, *color_value, line_width);
}

void draw_rect_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                  unsigned int pix_h, int left, int top, int right, int bottom,
                  color_yuv_t nv21_value, unsigned int line_width)
{
    draw_line_nv21(y_data_array, uv_data_array, pix_w, pix_h, left, top, left, bottom, nv21_value,
                 line_width);
    draw_line_nv21(y_data_array, uv_data_array, pix_w, pix_h, right, top, right, bottom, nv21_value,
                 line_width);
    draw_line_nv21(y_data_array, uv_data_array, pix_w, pix_h, left, top, right, top, nv21_value,
                 line_width);
    draw_line_nv21(y_data_array, uv_data_array, pix_w, pix_h, left, bottom, right + line_width,
                 bottom, nv21_value, line_width);
}

void draw_text_utf8(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
                  unsigned int pix_h, char *text_utf8, int x, int y, font_info_t *pFontInfo,
                  unsigned int ft_color, unsigned int bk_color)
{
    if (bytes_per_pix > 4 || bytes_per_pix == 0) {
        DOORLOCK_ERR("bytes_per_pix error %d\n", bytes_per_pix);
        return;
    }
    FILE *ap = NULL;
    if ((ap = fopen(pFontInfo->font_path, "rb")) == NULL) {
        DOORLOCK_DBG("Can't Open ASC %s\n", pFontInfo->font_path);
        // return;
    }
    FILE *gp = NULL;
    if ((gp = fopen(pFontInfo->gb_font_path, "rb")) == NULL) {
        DOORLOCK_DBG("Can't Open HZK %s\n", pFontInfo->gb_font_path);
        // return;
    }
    unsigned char *dst_data = NULL;
    unsigned char *src_data = NULL;
    int h_pos = x;
    int v_pos = y;
    for (int m_count = 0; m_count < strlen(text_utf8); m_count++) {
        int final_font_width, final_font_height;
        int tmp_font_width, tmp_font_height;
        int per_char_size;
        unsigned char *final_font_data = NULL;
        unsigned char *tmp_font_data = NULL;

        if (text_utf8[m_count] & 0x80) {
            char chinese[4] = {0};
            chinese[0] = text_utf8[m_count++];
            chinese[1] = text_utf8[m_count++];
            chinese[2] = text_utf8[m_count];
            char word[4] = {0};
            utf8ToGb2312(word, sizeof(word), chinese, strlen(chinese));

            unsigned char qh = word[0] - 0xa0;
            unsigned char wh = word[1] - 0xa0;

            tmp_font_width = pFontInfo->gb_font_width;
            tmp_font_height = pFontInfo->gb_font_height;
            per_char_size = tmp_font_width * tmp_font_height / 8;
            tmp_font_data = (unsigned char *)malloc(per_char_size);
            if (tmp_font_data == NULL) {
                DOORLOCK_ERR("malloc %d failed\n", per_char_size);
                goto exit;
            }
            memset(tmp_font_data, 0, per_char_size);
            unsigned int fontOffset =
                (94 * (unsigned int)(qh - 1) + (unsigned int)(wh - 1)) * per_char_size;
            if (gp) {
                fseek(gp, fontOffset, SEEK_SET);
                fread(tmp_font_data, per_char_size, 1, gp);
            }
            // DOORLOCK_INFO("[%s] qh=%d, wh=%d\n", chinese, qh, wh);
        } else {
            int word = text_utf8[m_count];
            tmp_font_width = pFontInfo->font_width;
            tmp_font_height = pFontInfo->font_height;
            per_char_size = tmp_font_width * tmp_font_height / 8;
            tmp_font_data = (unsigned char *)malloc(per_char_size);
            if (tmp_font_data == NULL) {
                DOORLOCK_ERR("malloc %d failed\n", per_char_size);
                goto exit;
            }
            memset(tmp_font_data, 0, per_char_size);
            unsigned int fontOffset = word * per_char_size;
            if (ap) {
                fseek(ap, fontOffset, SEEK_SET);
                fread(tmp_font_data, per_char_size, 1, ap);
            }
        }
        final_font_width = tmp_font_width;
        final_font_height = tmp_font_height;
        final_font_data = tmp_font_data;
        tmp_font_data = NULL;

        if (text_utf8[m_count] == '\n') {
            h_pos = x;
            v_pos += final_font_height;
            goto clean_res;
        } else if (text_utf8[m_count] == '\r') {
            h_pos = x;
            goto clean_res;
        }
        for (int bitH = 0; bitH < final_font_height; bitH++) {
            for (int byteW = 0; byteW < final_font_width / 8; byteW++) {
                unsigned char byteValue = final_font_data[bitH * final_font_width / 8 + byteW];
                for (int b = 0; b < 8; b++) {
                    int dotOffset = 0;
                    if (h_pos >= pix_w - final_font_width) {  // 当前行已结束，换行
                        v_pos += final_font_height;
                        // h_pos = 0;
                        h_pos = x;
                    }
                    if (v_pos >= pix_h - final_font_height) {
                        if (final_font_data) {
                            free(final_font_data);
                        }
                        if (tmp_font_data) {
                            free(tmp_font_data);
                        }
                        DOORLOCK_ERR("!!! string too long !!!\n");
                        goto exit;
                    }
                    dotOffset = bitH * pix_w + byteW * 8 + b + v_pos * pix_w + h_pos;
                    dst_data = &data_array[(dotOffset)*bytes_per_pix];
                    // if (byteValue & (0x80>>b)) {//MSB
                    if (byteValue & (1 << b)) {  // LSB
                        src_data = (unsigned char *)&ft_color;
                    } else {
                        src_data = (unsigned char *)&bk_color;
                        if (bk_color == 0x0) {
                            //
                            continue;
                        }
                    }
                    memcpy(dst_data, src_data, bytes_per_pix);
                }
            }
        }
        h_pos += final_font_width;
    clean_res:
        if (final_font_data) {
            free(final_font_data);
        }
        if (tmp_font_data) {
            free(tmp_font_data);
        }
    }
exit:
    if (ap) {
        fclose(ap);
    }
    if (gp) {
        fclose(gp);
    }
}

void draw_text_utf8_nv21(unsigned char *data_array_y, unsigned char *data_array_vu, unsigned int pix_w,
                      unsigned int pix_h, char *text_utf8, int x, int y, font_info_t *pFontInfo,
                      color_yuv_t ft_color, color_yuv_t bk_color)
{
    FILE *ap = NULL;

    if ((ap = fopen(pFontInfo->font_path, "rb")) == NULL) {
        DOORLOCK_DBG("Can't Open ASC %s\n", pFontInfo->font_path);
        // return;
    }
    FILE *gp = NULL;
    if ((gp = fopen(pFontInfo->gb_font_path, "rb")) == NULL) {
        DOORLOCK_DBG("Can't Open HZK %s\n", pFontInfo->gb_font_path);
        // return;
    }
    unsigned char *dst_data = NULL;
    unsigned char *src_data = NULL;
    int h_pos = x;
    int v_pos = y;
    for (int m_count = 0; m_count < strlen(text_utf8); m_count++) {
        int final_font_width, final_font_height;
        int tmp_font_width, tmp_font_height;
        int per_char_size;
        unsigned char *final_font_data = NULL;
        unsigned char *tmp_font_data = NULL;

        if (text_utf8[m_count] & 0x80) {
            char chinese[4] = {0};
            chinese[0] = text_utf8[m_count++];
            chinese[1] = text_utf8[m_count++];
            chinese[2] = text_utf8[m_count];
            char word[4] = {0};
            utf8ToGb2312(word, sizeof(word), chinese, strlen(chinese));

            unsigned char qh = word[0] - 0xa0;
            unsigned char wh = word[1] - 0xa0;

            tmp_font_width = pFontInfo->gb_font_width;
            tmp_font_height = pFontInfo->gb_font_height;
            per_char_size = tmp_font_width * tmp_font_height / 8;
            tmp_font_data = (unsigned char *)malloc(per_char_size);
            if (tmp_font_data == NULL) {
                DOORLOCK_ERR("malloc %d failed\n", per_char_size);
                goto exit;
            }
            memset(tmp_font_data, 0, per_char_size);
            unsigned int fontOffset =
                (94 * (unsigned int)(qh - 1) + (unsigned int)(wh - 1)) * per_char_size;
            if (gp) {
                fseek(gp, fontOffset, SEEK_SET);
                fread(tmp_font_data, per_char_size, 1, gp);
            }
            // DOORLOCK_INFO("[%s] qh=%d, wh=%d\n", chinese, qh, wh);
        } else {
            int word = text_utf8[m_count];
            tmp_font_width = pFontInfo->font_width;
            tmp_font_height = pFontInfo->font_height;
            per_char_size = tmp_font_width * tmp_font_height / 8;
            tmp_font_data = (unsigned char *)malloc(per_char_size);
            if (tmp_font_data == NULL) {
                DOORLOCK_ERR("malloc %d failed\n", per_char_size);
                goto exit;
            }
            memset(tmp_font_data, 0, per_char_size);
            unsigned int fontOffset = word * per_char_size;
            if (ap) {
                fseek(ap, fontOffset, SEEK_SET);
                fread(tmp_font_data, per_char_size, 1, ap);
            }
        }
        final_font_width = tmp_font_width;
        final_font_height = tmp_font_height;
        final_font_data = tmp_font_data;
        tmp_font_data = NULL;

        if (text_utf8[m_count] == '\n') {
            h_pos = x;
            v_pos += final_font_height;
            goto clean_res;
        } else if (text_utf8[m_count] == '\r') {
            h_pos = x;
            goto clean_res;
        }
        for (int bitH = 0; bitH < final_font_height; bitH++) {
            for (int byteW = 0; byteW < final_font_width / 8; byteW++) {
                unsigned char byteValue = final_font_data[bitH * final_font_width / 8 + byteW];
                for (int b = 0; b < 8; b++) {
                    if (h_pos >= pix_w - final_font_width) {  // 当前行已结束，换行
                        v_pos += final_font_height;
                        // h_pos = 0;
                        h_pos = x;
                    }
                    if (v_pos >= pix_h - final_font_height) {
                        if (final_font_data) {
                            free(final_font_data);
                        }
                        if (tmp_font_data) {
                            free(tmp_font_data);
                        }
                        DOORLOCK_ERR("!!! string too long !!!\n");
                        goto exit;
                    }
                    int dot_h_pos = h_pos + byteW * 8 + b;
                    int dot_v_pos = v_pos + bitH;

                    // int dotOffsetY = bitH*pix_w + byteW*8 + b + v_pos*pix_w + h_pos;
                    int dotOffsetY = dot_v_pos * pix_w + dot_h_pos;
                    int dotOffsetV = dot_v_pos / 2 * pix_w + dot_h_pos / 2 * 2;
                    int dotOffsetU = dotOffsetV + 1;
                    if (byteValue & (1 << b)) {  // LSB
                        data_array_y[dotOffsetY] = ft_color.y;
                        data_array_vu[dotOffsetV] = ft_color.v;
                        data_array_vu[dotOffsetU] = ft_color.u;
                    } else {
                        if (bk_color.y == 0x0 && bk_color.v == 0x0 && bk_color.u == 0x0) {
                            //
                            continue;
                        }
                        data_array_y[dotOffsetY] = bk_color.y;
                        data_array_vu[dotOffsetV] = bk_color.v;
                        data_array_vu[dotOffsetU] = bk_color.u;
                    }
                }
            }
        }
        h_pos += final_font_width;
    clean_res:
        if (final_font_data) {
            free(final_font_data);
        }
        if (tmp_font_data) {
            free(tmp_font_data);
        }
    }
exit:
    if (ap) {
        fclose(ap);
    }
    if (gp) {
        fclose(gp);
    }
}

void draw_text_utf8_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                      char *text_utf8, int x, int y, font_info_t *pFontInfo, unsigned int ft_color,
                      unsigned int bk_color)
{
    draw_text_utf8(data_array, 1, pix_w, pix_h, text_utf8, x, y, pFontInfo, ft_color, bk_color);
}

void draw_text_utf8_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                        char *text_utf8, int x, int y, font_info_t *pFontInfo,
                        color_argb_t ft_argb_value, color_argb_t bk_argb_value)
{
    unsigned int *color_ft_value = (unsigned int *)&ft_argb_value;
    unsigned int *color_bk_value = (unsigned int *)&bk_argb_value;

    draw_text_utf8(data_array, 4, pix_w, pix_h, text_utf8, x, y, pFontInfo, *color_ft_value,
                 *color_bk_value);
}

void draw_text_utf8_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                        char *text_utf8, int x, int y, font_info_t *pFontInfo,
                        color_abgr_t ft_abgr_value, color_abgr_t bk_abgr_value)
{
    unsigned int *color_ft_value = (unsigned int *)&ft_abgr_value;
    unsigned int *color_bk_value = (unsigned int *)&bk_abgr_value;

    draw_text_utf8(data_array, 4, pix_w, pix_h, text_utf8, x, y, pFontInfo, *color_ft_value,
                 *color_bk_value);
}

void draw_font_init(font_info_t *font_info, unsigned char font_size, char *font_dir)
{
    char eng_font_path[200];
    char hzk_font_path[200];

    memset(eng_font_path, 0, sizeof(eng_font_path));
    memset(hzk_font_path, 0, sizeof(hzk_font_path));
    memset(font_info, 0, sizeof(font_info_t));

    font_info->font_size = font_size;
    strcpy(font_info->font_dir, font_dir);
    if (font_dir != NULL && strlen(font_dir) > 0) {
        sprintf(eng_font_path, "%s/ASC%d", font_dir, font_size);
        sprintf(hzk_font_path, "%s/HZK%d", font_dir, font_size);
    } else {
        sprintf(eng_font_path, "ASC%d", font_size);
        sprintf(hzk_font_path, "HZK%d", font_size);
    }
    strcpy(font_info->font_path, eng_font_path);
    font_info->font_width = font_size / 2;
    font_info->font_height = font_size;
    strcpy(font_info->gb_font_path, hzk_font_path);
    font_info->gb_font_width = font_size;
    font_info->gb_font_height = font_size;

    //DOORLOCK_DBG("Font size=%d, eng=[%s] w=%d h=%d, hzk=[%s] w=%d h=%d\n", font_info->font_size, \
        font_info->font_path, font_info->font_width, font_info->font_height, \
        font_info->gb_font_path, font_info->gb_font_width, font_info->gb_font_height);
}
