Test sample of UAC, make by wuguanling

1. Need to enable [CONFIG_USB_CONFIGFS_F_UAC1] on kernel config.
   Need to enable [CONFIG_SND_PROC_FS] on kernel config.

2.running command to enable uac function of USB:
./setusbconfig-configfs uac1

3.Now will creat new audio [AC Interface] in PC , board will creat a new audio card at the same time .
check the audio card:
------------------
cat /proc/asound/cards
 0 [sun8iw19codec  ]: sun8iw19-codec - sun8iw19-codec
      sun8iw19-codec
 1 [snddaudio0     ]: snddaudio0 - snddaudio0
      snddaudio0
 2 [UAC1Gadget     ]: UAC1_Gadget - UAC1_Gadget
     UAC1_Gadget 0
------------------

The "2 [UAC1Gadget     ]" is uac audio card,
you need copy it's name to modify {uac_audio_card_name = "hw:UAC1Gadget"} on ./sample_uac.conf

4. running sample : [mic --> uac --> PC]
./sample_uac -path ./sample_uac.conf


5.Using Audio tools of to capture uac pcm in pc.

===============================================
20231118更新：
支持UAC1 in 和UAC1 out功能测试。

参数说明:
pcm_sample_rate: 音频采样率。默认16000。
pcm_channel_cnt: 音频声道数。默认 1。
pcm_bit_width: 音频位宽。默认16。
pcm_frame_size: 音频样本大小。默认1024。
aec_en: 音频采集AEC回声消除功能。默认1开启。
ans_en: 音频采集ANS降噪功能。默认1开启。
ans_mode: 音频采集ANS降噪功能模式。默认1。
agc_en: 音频采集自动增益功能。默认1开启。
agc_float_target_db: 音频采集自动增益功能目标分贝。默认0取值[-30, 0]。
agc_float_max_gain_db: 音频采集自动增益功能目标增益分贝。默认30取值[0, 30]。


enable_uac1_in = 1
enable_uac1_out = 1


注:
1. UAC1 in: 作为USB麦克风。即板子通过MIC采集音频再通过USB声卡发送至PC
2. UAC1 out: 作为USB音响。即板子通过USB声卡采集电脑播放的音频，再通过板子上的喇叭进行播放
3. 该功能需要内核开启CONFIG_USB_CONFIGFS_F_UAC1选项启用UAC1功能。其中双向功能需要两个ISOC端点，请参考Tina_Linux_USB_开发指南文档检查配置。
