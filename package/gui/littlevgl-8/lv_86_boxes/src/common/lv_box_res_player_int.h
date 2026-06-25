#ifndef _PLAYER_INT_H_
#define _PLAYER_INT_H_

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>

#include "lv_box_res_mid_list.h"

int tplayer_init(void);
int tplayer_exit(void);
int tplayer_play_url_first(const char *parth);

int tplayer_play_url(const char *parth);
int tplayer_play(void);
int tplayer_pause(void);
int tplayer_seekto(int nSeekTimeMs);
int tplayer_stop(void);
int tplayer_setvolumn(int volumn);
int tplayer_getvolumn(void);
int tplayer_setlooping(bool bLoop);
int tplayer_getduration(int *msec);
int tplayer_getcurrentpos(int *msec);
int tplayer_getcompletestate(void);

int tplayer_videodisplayenable(int enable);
int tplayer_setsrcrect(int x, int y, unsigned int width, unsigned int height);
int tplayer_setbrightness(unsigned int grade);
int tplayer_setcontrast(unsigned int grade);
int tplayer_sethue(unsigned int grade);
int tplayer_setsaturation(unsigned int grade);
int tplayer_getplaying(void);

int lv_box_res_music_init(void);
int lv_box_res_music_list_init(void);
void lv_box_res_music_play_mode(int mode , int index);
int lv_box_res_music_play(void);
int lv_box_res_music_pause(void);
int lv_box_res_music_get_volumn(void);
int lv_box_res_music_set_volumn(int volume);
void lv_box_res_music_get_percent(double *percent);
void lv_box_res_music_get_time(char *curTime, char *totalTime);
list_head_t* lv_box_res_music_get_media_list();
bool lv_box_res_music_get_playing();

#endif
