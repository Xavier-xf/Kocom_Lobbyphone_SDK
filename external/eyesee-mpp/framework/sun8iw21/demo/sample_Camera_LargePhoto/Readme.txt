演示相机拍摄大分辨率照片的最快速度流程。只用一个VIPP通道，平常使用低分辨率预览。VIPP通道就直接选择第一个VIPP通道了。
拍照时先关预览，再关VIPP通道，再配置大分辨率参数，再开VIPP通道，拍照（包括连拍）结束后，保留JPEG编码器，关VIPP通道，再
配置低分辨率参数，再开VIPP通道，再开预览。需要注意：v853的isp最大处理能力是3072x3072，故大分辨率图像4032x3016必须拆成左
右两张图，让isp处理两次才可以。isp分时复用成isp0和isp1分别处理，然后合成一幅图从一个VIPP通道输出。
如果要开启大分辨率拆图处理的功能，需VIChannel.h中打开宏定义STITCH_MODE，同时mpp的libisp库也要做特殊处理。
整个流程不析构EyeseeCamera和其内的VIChannel，只重置。以达到最快速度。

支持zoom、预览旋转。为简化流程复杂度，不考虑crop区域了。

运行：
./sample_Camera_LargePhoto -path /mnt/extsd/sample_Camera_LargePhoto.conf

参数说明：
preview_width: 预览时的VIPP的采集分辨率
preview_height:
preview_frame_rate: 预览时的采集帧率
preview_frame_num: 预览时的vipp通道设置的frame数量
preview_pic_format: 预览时的vipp通道设置的帧格式。支持nv21,lbc2.5等
preview_mirror: 预览镜像的设置。0: no mirror ratation, 1: horizonta mirror rotation, 2: vertical mirror rotation
preview_rotation: 预览旋转的设置。0, 90, 180, 270，顺时针旋转。
display_frame_rate: 预览时的显示帧率，0表示和采集帧率一致。
disp_width: 预览时的显示区域宽度
disp_height: 预览时的显示区域高度

capture_width: 拍照时的VIPP的采集分辨率
capture_height:
capture_frame_rate: 拍照时的采集帧率
capture_frame_num: 拍照时的vipp通道设置的frame数量
capture_pic_format: 拍照时的vipp通道设置的帧格式。支持nv21,lbc2.5等

digital_zoom: 预览和拍照的数字变焦，其实就是设定crop区域进行显示和放大编码。

take_photo_times: 拍照测试的次数。
keep_jpeg_encoder: 是否保留jpeg编码器。1:保留，多次拍照时会更快。0:不保留，一次拍照结束后就析构，减少内存占用。
jpeg_width:jpeg编码目标分辨率
jpeg_height:
jpeg_quality:jpeg编码质量
jpeg_thumb_width: 缩略图宽度，0表示不要缩略图。
jpeg_thumb_height: 缩略图高度，0表示不要缩略图。
jpeg_thumb_quality: 缩略图质量
jpeg_num: 一次拍照过程中拍摄的图像数量。 0: 不拍，1:单拍，>1:连拍n张
jpeg_interval: 如果连拍，2张图片的时间间隔，单位毫秒。0表示每帧都编码。
jpeg_folder = "/mnt/extsd/sample_Camera_LargePhoto_Files"

test_duration: 测试时间，单位秒。

