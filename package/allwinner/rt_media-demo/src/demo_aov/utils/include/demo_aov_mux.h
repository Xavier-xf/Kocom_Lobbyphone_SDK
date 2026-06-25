#ifndef __DEMO_AOV_MUX_H__
#define __DEMO_AOV_MUX_H__

enum RECORD_TYPE {
    RECORD_TYPE_FRAMELOOP = 0,
    RECORD_TYPE_TIMELAPSE,
    RECORD_TYPE_NORMAL,
};

struct demo_aov_mux_config {
    int width;
    int height;
    int framerate;
    int max_key_interval;
    int file_duration;
    int ongoing_duration;
    int max_cache_len;
    enum RECORD_TYPE record_type;
    int (*release_stream_callback)(void *stream);
    void (*request_idr_frame)(void);
};

int demo_aov_mux_init(struct demo_aov_mux_config *config);
int demo_aov_mux_destroy(void);
int demo_aov_mux_add_stream(void *stream);
void demo_aov_mux_check_cache(int algo_detected);
int demo_aov_mux_wait_complete(int timeout);
void demo_aov_mux_set_spspps(void *info);
void demo_aov_mux_dump_cache_info(void);

#endif
