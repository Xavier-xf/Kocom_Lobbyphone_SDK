#ifndef _NOSC_DRC_H_
#define _NOSC_DRC_H_

/* dynamic range control parameters */
typedef struct
{
	/* sampling rate */
	int sampling_rate;
	/* */
	int target_level;
	/* */
	int max_gain;
	/* */
	int min_gain;
	/* */
	int attack_time;
	/* */
	int release_time;
	/* */
	int noise_threshold;

}drcLog_prms_t;
/*
function :drcinit
init function
input:
  fs:采样率
return :
   dynamic range controller handle 
*/
void* nosc_drcLog_create(drcLog_prms_t* prms);
/*
function :drcdec
drc function
input:
  handle:	dynamic range controlller handle
  xout			:处理数据首地址；
  samplenum	:处理sample数，单通路samplenum = 处理数据总长度/2(short size);双声道samplenum = 处理数据总长度/4;
  nch       :通道数
return :
   无返回值
*/
void nosc_drcLog_process(void* handle, short* xout,int samplenum, int nch);

/*
function: drcfree
deinit the heap etc.
input:
	handle:	dynamic range controller handle
return value:
	none
*/
void nosc_drcLog_destroy(void* handle);

#endif//_NOSC_DRC_H_