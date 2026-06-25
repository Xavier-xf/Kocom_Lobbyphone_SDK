## 功能说明

验证图语的人脸识别功能

## 测试通路
----------------------------------------------------------------------------------------------------
显示通路   vipp0-------------->vir_channle0----->vo_channel0,vo_layer=0----->lcd
                                   \
                                    \
                                     \            
						              \ 若识别成功，通过AW_MPI_RGN_AttachToChn在
						               \人脸上添加绿框，否则没有其他提示
						   		        \			
                                         \           
npu通路    vipp4----->vir_channle0---->npu 识别人脸结果和人脸坐标（坐标转换到vipp0的尺寸下）
									  /
		   vipp1----->vir_channle1----
-----------------------------------------------------------------------------------------------------

## 前置条件 
	1. 开发板连接上摄像头，且摄像头工作正常。
	2. 开发板连接上 SD 卡，且 SD 卡工作正常。
	3. 准备人脸识别的样本图片，图片格式nv21,分辨率1920*1080，可使用smaple_virvi生成，目前只支持一张图片中只有一张脸。
	4  生成对应板子的key.h,替换掉 package/allwinner/libsdk-viplite-driver/third-party/facekit/inc/key.h,不同板子需要不同key，key的获取参考《Mpp_sample_使用说明》
	5. 测试用例和配置文件   
  	    用例：sample_facekit_demo
   		配置：sample_facekit_demo.conf

## 操作步骤
 1. 修改sample_facekit_demo.conf配置文件    
    （1）修改模型位置 
    ```
    模型位于 sample_facekit_demo/mode目录下
    npu_model_file = "/mnt/sdcard/pix_allwinner_facekit_v1.3.bin" 
    ```
    （2）修改样本图片
    ```
    人脸识别样本图片，可支持多张，目前测试支持2张
    再次强调，模型只支持支nv21，所以准备的图片必须是nv21和1920*1080
    person_pic_file1 = "/mnt/sdcard/person1.nv21"
    person_pic_id1 = 1
    person_pic_name1 = "person1"
    person_pic_width1 = 1920
    person_pic_height1 = 1080

    person_pic_file2 = "/mnt/sdcard/person2.nv21"
    person_pic_id2 = 2
    person_pic_name2 = "person2"
    person_pic_width2 = 1920
    person_pic_height2 = 108
    ```
    （3） 修改测试时间
    ```
    根据实际需求修改
    test_duration = 30	#unit:s ,0-无限，只有按ctrl+c结束
    ```
    （4）其他参数默认即可

 2. 将测试程序和修改后的配置文件拷贝到 SD 卡（或者用 adb push 推送到 SD卡）。
 3. 在串口执行命令进行测试     
   /mnt/extsd/sample_facekit_demo -path /mnt/extsd/sample_facekit_demo.conf

## sample_facekit_demo.conf 配置文件说明
```
# 显示通路的配置
capture_width = 1920
capture_height = 1080
display_x = 0
display_y = 0
display_width = 640
display_height = 360
layer_num = 0
isp_dev = 0
vipp_dev = 0
pic_format = "nv21"
frame_rate = 20

#npu通路配置  
npu_isp_dev = 0
npu_vipp_dev = 4
npu_capture_width = 1920
npu_capture_height = 1080
# 模型位于 sample_facekit_demo/mode目录下
npu_model_file = "/mnt/sdcard/pix_allwinner_facekit_v1.3.bin" 
# 模型只支持nv21 所以必须是nv21
npu_pic_format = "nv21"
npu_frame_rate = 20 # 与显示通路配置一样

#人脸识别样本图片，可支持多张，目前测试支持2张
# 再次强调，模型只支持支nv21，所以准备的图片必须是nv21和1920*1080
person_pic_file1 = "/mnt/sdcard/person1.nv21"
person_pic_id1 = 1
person_pic_name1 = "person1"
person_pic_width1 = 1920
person_pic_height1 = 1080

person_pic_file2 = "/mnt/sdcard/person2.nv21"
person_pic_id2 = 2
person_pic_name2 = "person2"
person_pic_width2 = 1920
person_pic_height2 = 1080


# disp_type is lcd, hdmi, cvbs
disp_type = "lcd"
test_duration = 30	#unit:s

```

## 预期结果
  人脸识别，匹配成功则绿色框显示并且打印提示匹配成功