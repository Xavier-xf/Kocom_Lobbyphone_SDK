#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include "nosc.h"

#define NS_SAMPLE_RATE  (16000)
#define FRAME_SIZE      (1024)
#define CHANNUM         (1)
#define BUFLEN          (FRAME_SIZE*CHANNUM)

int main(int argc, char** argv)
{
	FILE* fin;
	FILE* fout;
	int items1;
	int count = 0;
	void* nosc;
	short bufferin[BUFLEN];
	ns_prms_t* prms = (ns_prms_t*)malloc(sizeof(ns_prms_t));

	fin = fopen("audio_data\\audio.pcm", "rb");
	
	fout = fopen("audio_data\\audio_out.pcm","wb");
	
	prms->sampling_rate = NS_SAMPLE_RATE;
	prms->max_suppression = -50;
	//prms->overlap_percent = OVERLAP_FIFTY;
	prms->overlap_percent = OVERLAP_SEVENTY_FIVE;

	prms->nonstat = HIGH_NONSTATIONAL;//MEDIUM_NONSTATIONAL;
	prms->channum = CHANNUM;

	nosc = NOSCinit(prms);

	items1 = fread(bufferin, sizeof(short), BUFLEN, fin);
	while(items1 > 0)
	{
		NOSCdec(nosc, bufferin, items1);
		fwrite(bufferin, sizeof(short), items1, fout);
		items1 = fread(bufferin,sizeof(short), BUFLEN, fin);
	}

	NOSCdestroy(nosc);

	free(prms);

	fclose(fin);
	fclose(fout);

	return 0;
}