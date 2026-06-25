#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <vip_lite.h>
#include "pix_face_api.h"
#include "key.h"
#include "ai_facekit.h"

#define PIX_V853_HARDWARE_INFO_BYTES 32
#define PIX_V853_MODEL_BYTES 2451836

#define ALLWINNER_PIC_WEIGHT 1920
#define ALLWINNER_PIC_HEIGHT 1080

static void save_binary_file(unsigned char *buf, int len);
static unsigned int get_file_size(const char *name);
static void info_printf();
static void dump_pix_coord_info(pix_coord_info_t *coord_info);
static int get_face_info(pix_image_face_info_t *face_info, const unsigned char *nir_img_ptr, unsigned char *rgb_img_ptr, int is_regest);


static void save_binary_file(unsigned char *buf, int len)
{
	FILE *filewrite = fopen("./bonding.bin", "wb+");

	if(filewrite == NULL)
	{
        aloge("fatal error, create file failure.");
        return;
	}

	fseek(filewrite, 0, SEEK_SET);

	if(fwrite(buf, 1, len, filewrite) != len)
	{
		aloge("write file failure.");
		fclose(filewrite);
		return;
	}

	fflush(filewrite);
	fsync(fileno(filewrite));
	fclose(filewrite);

	return;
}

void save_bonding()
{
    unsigned char idcode[32];
    memset(idcode,0x00,32);
    pix_get_hardware_info(idcode,32);
    for(int count=0;count<32;count++){
        printf("0x%02x",idcode[count]);
    }
    printf("\n");
    save_binary_file(idcode,32);
}

static unsigned int get_file_size(const char *name)
{
    FILE    *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fclose(fp);
    }
    else {
        aloge("Checking file %s failed.\n", name);
    }
    return size;
}

static void info_printf()
{
    alogd("***********************************************************\n");
    const char *facekit_version = pix_facekit_version();    //获取算法版本
    alogd("The version of the facekit is:%s\n",facekit_version);

    int face_feature_bytes = pix_get_fr_feature_bytes();    //获取⼈脸特征值的⼤小
    alogd("face_feature_bytes: %d\n",face_feature_bytes);

    float liveness_threshold = pix_get_liveness_threshold();//获取活体分数阀值
    alogd("liveness_threshold : %d\n",liveness_threshold);

    float fr_threshold = pix_get_fr_threshold();//获取⼈脸识别阀值
    alogd("fr_threshold : %d\n",fr_threshold);
    alogd("***********************************************************\n");
}

static void dump_pix_coord_info(pix_coord_info_t *coord_info){

    alogd("Face box number %d\n", coord_info->face_number);
    for(int i = 0; i < coord_info->face_number; i++){
        alogd("\nFace %d coord info ", i);
        for(int j = 0; j < API_PER_BOX_ELEMENTS; j++){
            alogd("%f, ", coord_info->face_box[i * API_PER_BOX_ELEMENTS + j]);
        }
        alogd("\n");
    }
    alogd("\n");
}


static int get_face_info(pix_image_face_info_t *face_info, const unsigned char *nir_img_ptr, unsigned char *rgb_img_ptr, int is_regest){
    int ret = pix_set_normal_analyze_image(nir_img_ptr, rgb_img_ptr);
    if(VIP_SUCCESS != ret){
        aloge("Fail to pix_set_normal_analyze_image with %d\n", ret);
        return ret;
    }

	// alogd("face_info:%p,nir_img_ptr:%p,rgb_img_ptr:%p,is_regest:%d\n",face_info,nir_img_ptr,rgb_img_ptr,is_regest);

    // ret = pix_fast_fd(nir_img_ptr, &face_info->coord, 1);
    ret = pix_normal_fd(&face_info->coord, &face_info->rgb_coord); // 10ms
    if(VIP_SUCCESS != ret){
    //    aloge("Fail to pix_normal_fd with %d\n", ret);
        return ret;
    }

//    dump_pix_coord_info(&face_info->rgb_coord);

	// alogd("Face box number %d\n",  face_info->rgb_coord.face_number);

    for(int i = 0; i < face_info->rgb_coord.face_number; i++){
    //    alogd("Get %d face liveness and fr feature\n",i);
		alogd("get face:%d,[%p:%f]",i,&face_info->liveness[i].liveness_score,face_info->liveness[i].liveness_score);
		ret = pix_liveness_check(i, &face_info->liveness[i].liveness_score);//20ms
        if(VIP_SUCCESS != ret){
            aloge("Fail to pix_liveness_check with %d\n", ret);
            return ret;
        }

		if(face_info->liveness[i].liveness_score < pix_get_liveness_threshold()){
            alogd("check liveness fail\n");
        }

        ret = pix_get_face_fr_feature(i, &face_info->fr_feature[i], is_regest);//30ms
        if(VIP_SUCCESS != ret){
            aloge("Fail to pix_get_face_fr_feature with %d\n", ret);
            return ret;
        }

    //     for(int j = 0; j < API_PER_BOX_ELEMENTS; j++){
    //         alogd("%f, ", face_info->rgb_coord.face_box[i * API_PER_BOX_ELEMENTS + j]);
    //    }

	//    for(int j = 0; j < API_PER_BOX_ELEMENTS; j++){
    //         alogd("%f, ", face_info->coord.face_box[i * API_PER_BOX_ELEMENTS + j]);
    //    }

    //     alogd("\n%f \n", face_info->liveness[i].liveness_score);

    //     alogd("pix_get_face_fr_feature end\n");
    }

    return VIP_SUCCESS;
}

int ai_init(char * modeFilePath)
{
	vip_status_e status = VIP_SUCCESS;
	int ret = 0;
	
	status = vip_init(0);
	unsigned char hard_info[PIX_V853_HARDWARE_INFO_BYTES] = {0};
	status = pix_get_hardware_info(hard_info, PIX_V853_HARDWARE_INFO_BYTES);
	unsigned char *pix_model_ptr = NULL;
	unsigned int fileSize = 0;
	//save_bonding();//save bonding.bin
	do
	{
		fileSize = get_file_size(modeFilePath);
		if(fileSize == 0)
		{
			aloge("get_file_size error!!!\n");
			ret = -1;
			break;
		}

		alogd("file:%s,size:%u",modeFilePath,fileSize);
		pix_model_ptr = (unsigned char *)malloc(fileSize);

		status = load_file(modeFilePath, pix_model_ptr);
		if(status != fileSize)
		{
			aloge("load_file %s error status:[%d]!!!\n",modeFilePath,status);
			ret = -1;
			break;
		}

		// 双目摄像头标定矩阵设置
		static float nir_camera_k[6] = {1211.334955687666, 0.f, 1028.254858116448, 0.f, 1187.51676411435, 524.3453819675908};
    	static float rgb_camera_k[6]  = {1212.751990041438, 0.f, 985.1329903127512, 0.f, 1187.770568339236, 498.7751733122192};
    	pix_set_camera_k(nir_camera_k, rgb_camera_k);
		status =  pix_init_facekit_model(hard_info, pix_model_ptr, key_bin, key_bin_len);
		if (status != VIP_SUCCESS)
		{
		 	aloge("pix_init_facekit_model error  status: %d\n", status);
		 	ret = -1;
			break;
		}
		info_printf(); //printf the information of the algorithm

	}while(0);
	return ret;
}

int ai_deinit ()
{
	return pix_release_facekit_model();
}


int ai_save_face(char *file_name_ir, char *file_name_rgb, int w, int h, unsigned char *fea,int fea_len)
{
	int ret = 0;
	unsigned int fileSize = 0;
	unsigned int load_file_szie = 0;
    unsigned char *rgb_ptr = NULL;
	unsigned char *ir_ptr = NULL;
	vip_status_e status = VIP_SUCCESS;
	pix_image_face_info_t sample_face_info ;

	memset(&sample_face_info,0x00,sizeof(pix_image_face_info_t));

	ir_ptr = (unsigned char *)malloc(sizeof(unsigned char) * w * h * 3 / 2);	//格式nv21
	if(!ir_ptr)
	{
		aloge("malloc error!!!\n");
		ret = -1;
		return ret;
	}

	fileSize = get_file_size(file_name_ir);
	if(fileSize == 0)
	{
		aloge("get_file_size error!!!\n");
		free(ir_ptr);
		ir_ptr = NULL;
		ret = -1;
		return ret;
	}

	load_file_szie = load_file(file_name_ir, ir_ptr);
	if (load_file_szie != fileSize)
	{
	 	aloge("Fail to load_file\n");
		free(ir_ptr);
		ir_ptr = NULL;
		ret = -1;
		return ret;
	}

	rgb_ptr = (unsigned char *)malloc(sizeof(unsigned char) * w * h * 3 / 2);	//格式nv21
	if(!rgb_ptr)
	{
		aloge("malloc error!!!\n");
		ret = -1;
		return ret;
	}

	fileSize = get_file_size(file_name_rgb);
	if(fileSize == 0)
	{
		aloge("get_file_size error!!!\n");
		free(rgb_ptr);
		rgb_ptr = NULL;
		ret = -1;
		return ret;
	}

	load_file_szie = load_file(file_name_rgb, rgb_ptr);
	if (load_file_szie != fileSize)
	{
	 	aloge("Fail to load_file\n");
		free(rgb_ptr);
		rgb_ptr = NULL;
		ret = -1;
		return ret;
	}

	// 获取人脸信息，并注册人脸，get_face_info最后一个参数为1，表示获取人脸信息，并注册；为0时，只是获取人脸信息
	status = get_face_info(&sample_face_info, ir_ptr, rgb_ptr, 1);
	if (status != VIP_SUCCESS)
	{
        aloge("Fail to get img face info\n");
        free(rgb_ptr);
        free(ir_ptr);
		rgb_ptr = NULL;
		ir_ptr = NULL;
		ret = -1;
		return ret;
    }
	// printf("!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	// printf("fea2 liveness_score = %f\n", sample_face_info.liveness[0].liveness_score);
	// printf("fea2 face_number = %d\n", sample_face_info.coord.face_number);
	// printf("fea2 fea_length = %d\n", sample_face_info.fr_feature->fea_length);
	// printf("!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

	alogd("face_number:%d\n", sample_face_info.rgb_coord.face_number);

	for(int i = 0; i < sample_face_info.rgb_coord.face_number; i++){
		memcpy(fea, sample_face_info.fr_feature[i].fea,fea_len);
		alogd("save %s ok!!!\n", file_name_rgb);
		break;
    }

	for(int i = 0; i < sample_face_info.coord.face_number; i++){
		memcpy(fea, sample_face_info.fr_feature[i].fea,fea_len);
		alogd("save %s ok!!!\n", file_name_ir);
		break;
    }

	free(rgb_ptr);
	free(ir_ptr);
	rgb_ptr = NULL;
	ir_ptr = NULL;

	return ret;
}

int ai_det_face(unsigned char *person_nir,unsigned char *person_rgb, pix_image_face_info_t *face_info)
{
    vip_status_e status = VIP_SUCCESS;

    status = get_face_info(face_info, person_nir, person_rgb, 0);
	if(status != VIP_SUCCESS)
	{
		//alogw("status:%u\n",status);
		return -1;
	}

    return 0;
}


int ai_get_face_score(unsigned char* fea1,unsigned char* fea2,float * score)
{
	vip_status_e status = VIP_SUCCESS;
	status = pix_cal_fea_sim(fea1,fea2,score);
	if(status != VIP_SUCCESS)
	{
		aloge("status:%u\n",status);
		return -1;
	}
	return 0;
}

float ai_get_threshold()
{
	return pix_get_fr_threshold();
}
