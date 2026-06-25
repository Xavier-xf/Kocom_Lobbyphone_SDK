
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
/* TODO: C++ the below head files. */
#include <string.h>
#include "uvoice_ecnr_api.h"
#include "uvoice_license.h"

#define ECNR_TEST_OK 0
#define ECNR_TEST_ERROR -1


/* ---------------typedef--------------------- */
typedef unsigned long long timestamp_t;

/* --------------global var---------------------- */
static volatile int kKeepRunning = 1;

/* --------------local funcs---------------------- */
static void intHandler(int dummy) { kKeepRunning = 0; }

static timestamp_t get_timestamp() {
  struct timeval now;
  gettimeofday(&now, NULL);

  return (timestamp_t)(now.tv_usec + now.tv_sec * 1e6);
}
const char* my_demo_auth_cb(const char* body)
{
    printf("Body is:%s\n",body);
    /*curl -k -s --data-binary $body https://srv01.51asr.com:8007/asrsn_active2"*/
    char curl_cmd[1024];
    int rc = snprintf(curl_cmd,1024,"curl -k -s --data-binary '%s' %s",body, "https://srv01.51asr.com:8007/asrsn_active2");
    if ( rc >0 ) {
    	FILE* https_req = popen(curl_cmd,"r");
	rc = fread(curl_cmd,1,1024,https_req);
	if ( rc>0 ) curl_cmd[rc]=0;
	return strdup(curl_cmd);
    }
    return "";
}
int main(int argc, char *argv[]) {
  int i = 0;

  /* audio */
  char in_file_path[512];
  char out_file_path[512];
  short *pOutAudio = NULL;
  size_t hopRead = 0;

  float process_duration = .0f;
  float audio_duration = .0f;
  timestamp_t start_tick;

  uv_ecnr_audio_buf audio;
  int frame_len = 160;


  strcpy(in_file_path, argv[1]);
  strcpy(out_file_path, argv[2]);

  //int in_mode = uvoice_ecnr_mode_2mic1ref;
  int in_mode = uvoice_ecnr_mode_1mic1ref;
  int out_mode = uvoice_ecnr_mode_phone;
  int sample_rate = 16000;

  void *handle_ecnr;

  // 使用云端授权方式调用API
  uv_activate_param p;
  p.license = "03810010100080006f34f506b5b732ca9e7426b0f3096bb5";
  p.license_path = "./demo.lic";
  p.uuid = "93ax1122334455";
  p.activate_type = 0;
  p.auth_cb = my_demo_auth_cb;
  handle_ecnr = UvEcnr_Create((UvEcnr_InputMode)in_mode,(UvEcnr_OutputMode)out_mode,sample_rate, &p);

  // 不适用云端授权调用API，process在半小时后将返回错误信息，处理输出为全0
  //handle_ecnr = UvEcnr_Create((UvEcnr_InputMode)in_mode,(UvEcnr_OutputMode)out_mode,sample_rate, NULL);
  
  // 授权失败时也会返回空句柄，注意看LOG
  if (NULL == handle_ecnr) return -1;


  uvoice_ecnr_status st = UvEcnr_Init(handle_ecnr);
  if( st!=UV_ECNR_OK ){
      printf("[UVOICE] ECNR init error!\n");
      UvEcnr_Destroy(handle_ecnr);
      return -1;
  }
  FILE* inPcm = fopen(in_file_path,"rb");
  FILE* outPcm = fopen(out_file_path, "wb");
  short buf[480];
  short mic1[160],mic2[160],ref[160];

  fseek(inPcm,44,SEEK_SET);

  while (kKeepRunning) {
    //int rc = fread(buf,sizeof(short),480,inPcm);
    //if ( rc != 480 ) break;
    //for(i=0;i<160;i++) {
    //    mic1[i] = buf[i*3];
    //    mic2[i] = buf[i*3+1];
    //	ref[i] = buf[i*3+2];	
    //}

    //audio.audioin[0] = mic1;
    //audio.audioin[1] = mic2;
    //audio.audioref[0] = ref;

    int rc = fread(buf,sizeof(short),320,inPcm);
    if ( rc != 320 ) break;
    for(i=0;i<160;i++) {
	mic1[i] = buf[i*2];
    	ref[i] = buf[i*2+1];	
    }

    audio.audioin[0] = mic1;
    audio.audioref[0] = ref;

    start_tick = get_timestamp();
    audio_duration += 0.01f;
    st = UvEcnr_Process(handle_ecnr, &audio, &pOutAudio);
    process_duration += (get_timestamp() - start_tick) / 1000000.0f;

    if( UV_ECNR_OK!=st ){
      if( UV_ECNR_VERIFY_ERROR==st ){
          printf("ERROR: ecnr license time passed.\n");
          fwrite(pOutAudio,sizeof(short),160,outPcm);
          continue;
      }
      printf("ERROR: process wav failed.");
      return ECNR_TEST_ERROR;
    } else {
      fwrite(pOutAudio,sizeof(short),160,outPcm);
    }
  }

  printf("RTF: %.4f (%.2f / %.2f)\n", process_duration / audio_duration, process_duration, audio_duration);

  fclose(outPcm);
  fclose(inPcm);
  UvEcnr_Destroy(handle_ecnr);
  return 0;
}
