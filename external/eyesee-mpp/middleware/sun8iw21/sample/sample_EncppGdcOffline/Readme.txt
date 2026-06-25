sample_EncppGdcOffline：
	用于测试encpp gdc 离线功能，包括LDC、LDC Pro、Pano180、Pano360、Normal、Fish2Wide、Perspective、BirdsEye。
	根据配置文件读取指定YUV图片，根据矫正模式进行处理。目前参数固定为AW_GDC调试指南_v1.2（可在一号通获取该文档）中的典型参数。

读取测试参数的流程：
	sample提供了sample_EncppGdcOffline.conf，测试参数都写在该文件中。
	启动sample_EncppGdcOffline时，在命令行参数中给出sample_EncppGdcOffline.conf的具体路径，sample_EncppGdcOffline会读取sample_EncppGdcOffline.conf，完成参数解析。
	然后按照参数运行测试。
	从命令行启动sample_EncppGdcOffline的指令：
	./sample_EncppGdcOffline -path /mnt/extsd/sample_EncppGdcOffline.conf
	"-path /mnt/extsd/sample_EncppGdcOffline.conf"指定了测试参数配置文件的路径。

测试参数的说明：
src_width: 指定输入YUV图片的宽度
src_height: 指定输入YUV图片的高度
src_fmt：指定输入YUV图片的像素格式
src_pic: 指定输入YUV图片的路径

dst_width: 指定输出YUV图片的宽度
dst_height: 指定输出YUV图片的高度
dst_fmt：指定输出YUV图片的像素格式
dst_pic: 指定输出YUV图片的路径

gdc_warp_type：矫正模式
gdc_mount_type：相机安装模式
gdc_mirror：gdc处理是否开启镜像
gdc_ldc_pro_lut_bin：LDC Pro模式下参数文件路径

注：
1. 目前支持nv21或者nv12格式YUV图片
2. 矫正模式支持如下：
	LDC:0 LDC_Pro:1 Pano180:2 Pano360:3 Normal:4 Fish2Wide:5 Perspective:6 BirdsEye:7
3. 相机安装模式支持如下：
	Top:0 Wall:1 Bottom:2
4. 详细参数效果调整请参考AW_GDC调试指南_v1.2，可在一号通获取。
5. 测试YUV图片可使用sample_venc下的gdc_function_test_data.tar.xz测试YUV图片。
