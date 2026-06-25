
#ifndef __AW_OSD_H__
#define __AW_OSD_H__

#include "doorlock_common.h"
#ifdef __cplusplus
extern "C" {
#endif
// little endian for qgchip

typedef struct color_argb_s {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
} color_argb_t;

typedef struct color_abgr_s {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} color_abgr_t;

typedef struct color_rgba_s {
    unsigned char alpha;
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} color_rgba_t;

typedef struct color_bgra_s {
    unsigned char alpha;
    unsigned char red;
    unsigned char green;
    unsigned char blue;
} color_bgra_t;

typedef struct color_yuv_s {
    unsigned char y;
    unsigned char v;
    unsigned char u;
} color_yuv_t;

typedef struct font_info_s {
    // ascii
    char font_path[64];
    int font_width;
    int font_height;
    // gb2312
    char gb_font_path[64];
    int gb_font_width;
    int gb_font_height;

    int font_size;
    char font_dir[64];
} font_info_t;

void draw_point(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
               unsigned int pix_h, int x, int y, unsigned int value);
void draw_point_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                   unsigned int pix_h, int x, int y, color_yuv_t nv21_value);
void draw_point_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x, int y,
                   unsigned char gray_value);
void draw_point_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x,
                     int y, color_argb_t argb_value);
void draw_point_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x,
                     int y, color_abgr_t abgr_value);

void draw_line(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
              unsigned int pix_h, int x1, int y1, int x2, int y2, unsigned int value,
              unsigned int line_width);
void draw_line_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                  unsigned int pix_h, int x1, int y1, int x2, int y2, color_yuv_t nv21_value,
                  unsigned int line_width);
void draw_line_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1, int y1,
                  int x2, int y2, unsigned char gray_value, unsigned int line_width);
void draw_line_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1,
                    int y1, int x2, int y2, color_argb_t argb_value, unsigned int line_width);
void draw_line_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int x1,
                    int y1, int x2, int y2, color_abgr_t abgr_value, unsigned int line_width);

void draw_rect(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
              unsigned int pix_h, int left, int top, int right, int bottom, unsigned int value,
              unsigned int line_width);
void draw_rect_nv21(unsigned char *y_data_array, unsigned char *uv_data_array, unsigned int pix_w,
                  unsigned int pix_h, int left, int top, int right, int bottom,
                  color_yuv_t nv21_value, unsigned int line_width);
void draw_rect_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                  int top, int right, int bottom, unsigned char gray_value,
                  unsigned int line_width);
void draw_rect_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                    int top, int right, int bottom, color_argb_t argb_value,
                    unsigned int line_width);
void draw_rect_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h, int left,
                    int top, int right, int bottom, color_abgr_t abgr_value,
                    unsigned int line_width);

void draw_text_utf8(unsigned char *data_array, unsigned char bytes_per_pix, unsigned int pix_w,
                  unsigned int pix_h, char *text_utf8, int x, int y, font_info_t *pFontInfo,
                  unsigned int ft_color, unsigned int bk_color);
void draw_text_utf8_nv21(unsigned char *data_array_y, unsigned char *data_array_vu, unsigned int pix_w,
                      unsigned int pix_h, char *text_utf8, int x, int y, font_info_t *pFontInfo,
                      color_yuv_t ft_color, color_yuv_t bk_color);
void draw_text_utf8_gray(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                      char *text_utf8, int x, int y, font_info_t *pFontInfo, unsigned int ft_color,
                      unsigned int bk_color);
void draw_text_utf8_argb32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                        char *text_utf8, int x, int y, font_info_t *pFontInfo,
                        color_argb_t ft_argb_value, color_argb_t bk_argb_value);
void draw_text_utf8_abgr32(unsigned char *data_array, unsigned int pix_w, unsigned int pix_h,
                        char *text_utf8, int x, int y, font_info_t *pFontInfo,
                        color_abgr_t ft_abgr_value, color_abgr_t bk_abgr_value);

void draw_font_init(font_info_t *font_info, unsigned char font_size, char *font_dir);

unsigned int BGRA(unsigned char B, unsigned char G, unsigned char R, unsigned char A);
unsigned int RGBA(unsigned char R, unsigned char G, unsigned char B, unsigned char A);
unsigned int ARGB(unsigned char A, unsigned char R, unsigned char G, unsigned char B);
unsigned int ABGR(unsigned char A, unsigned char B, unsigned char G, unsigned char R);
void COLOR_RGB_TO_YUV(unsigned char R, unsigned char G, unsigned char B, unsigned char *Y,
                      unsigned char *U, unsigned char *V);
void COLOR_YUV_TO_RGB(unsigned char Y, unsigned char U, unsigned char V, unsigned char *R,
                      unsigned char *G, unsigned char *B);
#ifdef __cplusplus
}
#endif

#endif /*End of file*/