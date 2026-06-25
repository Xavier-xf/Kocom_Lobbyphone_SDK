#ifndef _AI_FACEKIT_H_
#define _AI_FACEKIT_H_
#include <utils/plat_log.h>
#include "pix_face_api.h"
int ai_init(char * modeFilePath);
int ai_deinit ();
int ai_save_face(char *file_name_ir, char *file_name_rgb, int w, int h, unsigned char *fea,int fea_len);
int ai_det_face(unsigned char *person_nir,unsigned char *person_rgb, pix_image_face_info_t *face_info);
int ai_get_face_score(unsigned char* fea1,unsigned char* fea2,float * score);
float ai_get_threshold();
void save_bonding();
#endif
