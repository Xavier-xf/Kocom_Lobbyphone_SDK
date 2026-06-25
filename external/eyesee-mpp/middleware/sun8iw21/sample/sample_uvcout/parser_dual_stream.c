#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define MJPEG_APPN_MARKER(n)	((0xFF << 8) | n)

#define MJPEG_APP7_MARKER   0xFFE7
#define MJPEG_APP8_MARKER   0xFFE8
#define MJPEG_APP9_MARKER   0xFFE9

#define MJPEG_SOI_MARKER    0xFFD8
#define MJPEG_EOI_MARKER    0xFFD9

static u32 extract_h264_frame(u8 * frame_jpeg, u32 jpeg_frame_len, u8 *cur_frame_h264, u32 remain_h264_buf_len)
{
    u32 h264_frame_len = 0;

    u8 *mjpeg_frame_buf = frame_jpeg;
    u32 offset = 2;
    u32 app0_offset = 2;
    u16 app0_data_len = mjpeg_frame_buf[app0_offset + 2] << 8 | mjpeg_frame_buf[app0_offset + 3];
    u32 appn_offset = app0_offset + 2 + app0_data_len;
    printf("appn_offset %04x\n", appn_offset);
    while (1) {
        u16 APPn_marker = mjpeg_frame_buf[offset] << 8 | mjpeg_frame_buf[offset + 1];
        //u32 appn_data_len = mjpeg_frame_buf[appn_offset + 2] << 8 | mjpeg_frame_buf[appn_offset + 3];
        //printf("APPn_marker is %04x \n",APPn_marker);
        if (APPn_marker == MJPEG_APPN_MARKER(0))
        {
            offset += 2;
            u32 app0_len = mjpeg_frame_buf[offset] << 8 | mjpeg_frame_buf[offset+1] << 8;
            offset += (app0_len + 2) ;
            continue;
        }
        else if ((APPn_marker == MJPEG_APP7_MARKER) || (APPn_marker == MJPEG_APP8_MARKER) || (APPn_marker == MJPEG_APP9_MARKER)) {
        u32 appn_data_len = mjpeg_frame_buf[offset + 2] << 8 | mjpeg_frame_buf[offset + 3];
        u32 appn_video_len = (mjpeg_frame_buf[offset + 12] << 8 | mjpeg_frame_buf[offset + 13]) - 2;

        if (h264_frame_len <= remain_h264_buf_len)
            memcpy(cur_frame_h264 + h264_frame_len, &(mjpeg_frame_buf[offset + 14]), appn_video_len);
        else
            printf("warning: h264_frame_len %d > remain_h264_buf_len %d\n", h264_frame_len, remain_h264_buf_len);

        printf("----------------- got h264 data in %04x ------------------\n", APPn_marker);
        h264_frame_len += appn_video_len;
        offset = offset + 2 + appn_data_len;
        } else {
            //printf("----------------- break ------------------\n");
            break;
        }
        /*appn_offset = appn_offset + 2 + appn_data_len;
        if (appn_offset >= jpeg_frame_len)
            break;*/
    }

    return h264_frame_len;
}

static u32 extract_jpeg_frame(u8 * buf, u32 filelen, u8 *frame_h264, u32 h264_buf_len)
{
    u32 i = 0;
    u8 *mjpeg_frame_buf = buf;
    u16 marker = mjpeg_frame_buf[0] << 8 | mjpeg_frame_buf[1];
    //printf("marker %04x\n", marker);
    u8 *jpeg_buf_start = NULL;
    u8 *jpeg_buf_end = NULL;
    u32 jpeg_frame_len = 0;
    u32 h264_frame_len = 0;

    while (i < filelen)
    {
        marker = mjpeg_frame_buf[i] << 8 | mjpeg_frame_buf[i+1];
        if (marker == MJPEG_SOI_MARKER)
        {
            u8 valid_soi = 1;
            if (NULL != jpeg_buf_start) {
                printf("\n!!!!! warning case: repet SOI: %p !!!!\n", mjpeg_frame_buf + i);
                valid_soi = 0;
            }
            if (i > 2) {
                u16 marker_last = mjpeg_frame_buf[i-2] << 8 | mjpeg_frame_buf[i-1];
                //printf("marker_last: %04x\n", marker_last);
                if (marker_last != MJPEG_EOI_MARKER) {
                    valid_soi = 0;
                    printf("this is invalid SOI: %p, marker_last: %04x\n", mjpeg_frame_buf + i, marker_last);
                } else {
                    valid_soi = 1;
                    //printf("this is valid SOI: %p, marker_last: %04x\n", mjpeg_frame_buf + i, marker_last);
                }
            }
            if (valid_soi) {
                jpeg_buf_start = mjpeg_frame_buf + i;
                printf("SOI: %p-%d     ", jpeg_buf_start, i);
            }
        }
        else if (marker == MJPEG_EOI_MARKER)
        {
            jpeg_buf_end = mjpeg_frame_buf + i;
            if (NULL == jpeg_buf_start) {
                printf("!!!! wrong case: EOI but no SOI !!!!!\n");
                jpeg_frame_len = 0;
            } else{
                jpeg_frame_len = jpeg_buf_end - jpeg_buf_start;
                printf("EOI: %p-%d, jpeg_frame_len: %d \n", jpeg_buf_end, i, jpeg_frame_len);
            }
        }
        if (NULL != jpeg_buf_start && 0 < jpeg_frame_len)
        {
            u8 *cur_frame_h264 = frame_h264 + h264_frame_len;
            int remain_h264_buf_len = h264_buf_len - h264_frame_len;
            if (0 < remain_h264_buf_len)
                h264_frame_len += extract_h264_frame(jpeg_buf_start, jpeg_frame_len, cur_frame_h264, remain_h264_buf_len);
            else
                printf("h264 frame buf is full !!!!\n");
            jpeg_buf_start = NULL;
            jpeg_buf_end = NULL;
            jpeg_frame_len = 0;
            //break;
        }
        i++;
    }

    return h264_frame_len;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("input parameters fail! ./test input_file outout_file!\n");
        return -1;
    }
    printf("%s %s %s\n", argv[0], argv[1], argv[2]);

    FILE * raw = fopen(argv[1],"r");
    if(raw == NULL){
        printf("open file %s fail!\n", argv[1]);
        return -1;
    }
    FILE * h264 = fopen(argv[2],"wb");
    if(h264 == NULL){
        printf("open file %s fail!\n", argv[2]);
        goto exit;
    }

    fseek(raw, 0L,SEEK_END);
    //printf("open file ok file len:%ld\r\n",ftell(raw));
    u32 filelen = ftell(raw);
    u8 *filebuff = (u8 *)malloc(filelen+10);
    if (NULL == filebuff) {
        printf("malloc filebuff failed!");
        goto exit;
    }
    fseek(raw,0L,SEEK_SET);
    fread(filebuff,filelen,1,raw);
    printf("start decode\r\n");

    u32 h264_buf_len = filelen/2;
    u8 *frame_h264 = (u8 *)malloc(h264_buf_len);
    if (NULL == frame_h264) {
        printf("malloc frame_h264 failed!");
        goto exit;
    }
    memset(frame_h264, 0, h264_buf_len);

    u32 h264_frame_len = extract_jpeg_frame(filebuff, filelen, frame_h264, h264_buf_len);

    fwrite(frame_h264,h264_frame_len,1,h264);

    printf("\r\nend decode\r\n");

exit:
    if (raw)
        fclose(raw);
    if (h264)
        fclose(h264);
    if (filebuff)
        free(filebuff);
    if (frame_h264)
        free(frame_h264);
    return 0;
}
