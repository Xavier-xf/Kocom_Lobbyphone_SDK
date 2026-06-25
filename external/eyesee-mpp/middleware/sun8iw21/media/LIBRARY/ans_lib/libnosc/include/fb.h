#include <stdlib.h>
//#include <stdio.h>
void* fb_create(int Bsize);
int fb_analysis(void* handle, short* in, int len, short* outL, short* outH);
int fb_synthesis(void* handle, short* inL, short* inH, int len,  short* out);
void fb_destroy(void* handle);