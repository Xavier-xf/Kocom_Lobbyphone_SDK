/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the people's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../include/isp_ini_parse.h"
#include "../include/isp_manage.h"
#include "../include/isp_debug.h"

#if (ISP_VERSION >= 600)
#if defined CONFIG_SENSOR_GC2053_MIPI || defined CONFIG_SENSOR_GC4663_MIPI || defined CONFIG_SENSOR_SC5336_MIPI || \
	defined CONFIG_SENSOR_GC1084_MIPI || CONFIG_SENSOR_BF2257CS_MIPI || CONFIG_SENSOR_SC2355_MIPI || CONFIG_SENSOR_F37P_MIPI || \
	defined CONFIG_SENSOR_F355P_MIPI || CONFIG_SENSOR_OV02B10_MIPI || CONFIG_SENSOR_SC200AI_MIPI || CONFIG_SENSOR_CV5003_MIPI
#ifdef CONFIG_SENSOR_GC4663_MIPI
#include "SENSOR_H/gc4663_mipi_default_ini_v853.h"
#include "SENSOR_H/gc4663_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/gc4663_120fps_mipi_linear_to_wdr_ini_v853.h"
//#include "SENSOR_H/gc4663_mipi_wdr_default_v853.h"
#include "SENSOR_H/gc4663_mipi_wdr_auto_ratio_v853.h"
#include "SENSOR_REG_H/gc4663_mipi_120fps_720p_day_reg.h"
#include "SENSOR_REG_H/gc4663_mipi_2560_1440_15fps_day_reg.h"
//#include "SENSOR_REG_H/gc4663_mipi_2560_1440_wdr_15fps_day_reg.h"
#include "SENSOR_REG_H/gc4663_mipi_2560_1440_wdr_auto_ratio_15fps_day_reg.h"
#endif // CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_GC2053_MIPI
#ifdef CONFIG_SENSOR_GC2053_8BIT_MIPI
#include "SENSOR_H/gc2053_120fps_mipi_default_ini_v853_8bit.h"
#include "SENSOR_H/gc2053_mipi_isp600_20240425_161007_day_8bit.h"
#include "SENSOR_REG_H/gc2053_mipi_120fps_480p_day_reg_8bit.h"
#include "SENSOR_REG_H/gc2053_mipi_1080p_day_reg_8bit.h"
#else
#include "SENSOR_H/gc2053_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/gc2053_mipi_isp600_20240425_161007_day.h"
#include "SENSOR_REG_H/gc2053_mipi_120fps_480p_day_reg.h"
#include "SENSOR_REG_H/gc2053_mipi_1080p_day_reg.h"
#endif
#ifdef CONFIG_ENABLE_AIISP
#include "SENSOR_H/gc2053_mipi_isp600_20240425_161918_aiisp.h"
#include "SENSOR_REG_H/gc2053_mipi_1080p_aiisp_reg.h"
#endif
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
#include "SENSOR_H/sc5336_mipi_default_ini_v853.h"
#include "SENSOR_H/sc5336_130fps_mipi_default_ini_v853.h"
#include "SENSOR_REG_H/sc5336_mipi_2880_1620_day_reg.h"
#include "SENSOR_REG_H/sc5336_mipi_130fps_1440_400_day_reg.h"
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_GC1084_MIPI
#ifdef CONFIG_SENSOR_GC1084_8BIT_MIPI
#include "SENSOR_H/gc1084_120fps_mipi_default_ini_v853_8bit.h"
#include "SENSOR_H/gc1084_mipi_v853_20230410_164555_day_8bit.h"
#include "SENSOR_REG_H/gc1084_mipi_120fps_360p_day_reg_8bit.h"
#include "SENSOR_REG_H/gc1084_mipi_720p_day_reg_8bit.h"
#else
#include "SENSOR_H/gc1084_120fps_mipi_default_ini_v853.h"
#include "SENSOR_H/gc1084_mipi_v853_20230410_164555_day.h"
#include "SENSOR_REG_H/gc1084_mipi_120fps_360p_day_reg.h"
#include "SENSOR_REG_H/gc1084_mipi_720p_day_reg.h"
#endif
#ifdef CONFIG_ENABLE_AIISP
#include "SENSOR_H/gc1084_mipi_isp600_20230703_152809_aiisp.h"
#include "SENSOR_REG_H/gc1084_mipi_720p_aiisp_reg.h"
#else
#include "SENSOR_H/gc1084_mipi_v853_20230410_164555_ir.h"
#endif
#endif // CONFIG_SENSOR_GC1084_MIPI

#ifdef CONFIG_SENSOR_BF2257CS_MIPI
#include "SENSOR_H/bf2257cs_mipi_2_isp600_20231219_201540_RGB_new.h"
#include "SENSOR_H/bf2257cs_mipi_2_isp600_20231205_154925_ir.h"
#include "SENSOR_REG_H/bf2257cs_mipi_2_1600_1200_30fps_reg_day.h"
#endif // CONFIG_SENSOR_BF2257CS_MIPI

#ifdef CONFIG_SENSOR_SC2355_MIPI
#include "SENSOR_H/sc2355_mipi_isp600_20220726_230636_RGB_LSC_V18.h"
#include "SENSOR_H/sc2355_mipi_isp600_20220720_235901_IR_V42.h"
#include "SENSOR_REG_H/sc2355_mipi_1080p_15fps_reg_day.h"
#endif // CONFIG_SENSOR_SC2355_MIPI

#ifdef CONFIG_SENSOR_F37P_MIPI
#include "SENSOR_H/f37p_mipi_2_isp600_20231220_144145_gj.h"
#include "SENSOR_H/f37p_mipi_2_isp600_20230913_021938_gj_ir.h"
#include "SENSOR_REG_H/f37p_mipi_2_1080p_15fps_reg_day.h"
#endif // CONFIG_SENSOR_F37P_MIPI

#ifdef CONFIG_SENSOR_F355P_MIPI
#include "SENSOR_H/f355p_mipi_2_isp600_20231102_231808_gj.h"
#include "SENSOR_H/f355p_mipi_2_isp600_20231103_124305_gj_ir.h"
#include "SENSOR_REG_H/f355p_mipi_2_1080p_15fps_reg_day.h"
#endif // CONFIG_SENSOR_F355P_MIPI

#ifdef CONFIG_SENSOR_OV02B10_MIPI
#include "SENSOR_H/ov02b10_mipi_isp600_20221114b_color.h"
#include "SENSOR_H/ov02b10_mipi_isp600_20230811_104819_ir_v3.h"
#include "SENSOR_REG_H/ov02b10_mipi_1600_1200_30fps_reg_day.h"
#endif // CONFIG_SENSOR_OV02B10_MIPI

#ifdef CONFIG_SENSOR_SC200AI_MIPI
#ifdef CONFIG_SENSOR_SC200AI_8BIT_MIPI
#include "SENSOR_H/sc200ai_mipi_isp600_20240115_182208_day_8bit.h"
#else
#include "SENSOR_H/sc200ai_mipi_isp600_20240115_182208_day.h"
#endif
#if CONFIG_ENABLE_AIISP
#include "SENSOR_H/sc200ai_mipi_isp600_20240125_203452_602_aiisp.h"
#endif
#endif //CONFIG_SENSOR_SC200AI_MIPI

#ifdef CONFIG_SENSOR_CV5003_MIPI
#include "SENSOR_H/cv5003_mipi_isp600_20250227_140027_V20.h"
#include "SENSOR_H/cv5003_mipi_isp600_20250227_111937_V20_IR.h"
#include "SENSOR_H/cv5003_mipi_isp600_100fps_20250227_140027_V20.h"

#include "SENSOR_REG_H/cv5003_mipi_isp600_100fps_20250109_112708_V17_reg.h"
#include "SENSOR_REG_H/cv5003_mipi_isp600_20250227_140027_V20_reg.h"
#endif // CONFIG_SENSOR_CV5003_MIPI


#else
#include "SENSOR_H/gc2053_mipi_default_ini_v853.h"
#include "SENSOR_REG_H/gc2053_mipi_default_ini_v853_reg_day.h"
#endif
#endif //(ISP_VERSION >= 600)

unsigned int isp_cfg_log_param = ISP_LOG_CFG;

#define SIZE_OF_LSC_TBL     (12*768*2)
#define SIZE_OF_GAMMA_TBL   (5*1024*3*2)

struct isp_cfg_array cfg_arr[] = {
#if (ISP_VERSION >= 600)
#if defined CONFIG_SENSOR_GC2053_MIPI || defined CONFIG_SENSOR_GC4663_MIPI || defined CONFIG_SENSOR_SC5336_MIPI || \
	defined CONFIG_SENSOR_GC1084_MIPI || CONFIG_SENSOR_BF2257CS_MIPI || CONFIG_SENSOR_SC2355_MIPI || CONFIG_SENSOR_F37P_MIPI || \
	defined CONFIG_SENSOR_F355P_MIPI || CONFIG_SENSOR_OV02B10_MIPI || CONFIG_SENSOR_SC200AI_MIPI || CONFIG_SENSOR_CV5003_MIPI

#ifdef CONFIG_SENSOR_GC2053_MIPI
#ifdef CONFIG_SENSOR_GC2053_8BIT_MIPI
	{"gc2053_mipi", "gc2053_mipi_isp600_20240425_161007_day_8bit", 1920, 1088, 20, 0, 0, &gc2053_mipi_day_isp_cfg},
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_day_8bit", 640, 480, 120, 0, 0, &gc2053_mipi_120fps_v853_isp_cfg},
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_night_8bit", 640, 480, 120, 0, 1, &gc2053_mipi_120fps_v853_isp_cfg},
#else
	{"gc2053_mipi", "gc2053_mipi_isp600_20240425_161007_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_day_isp_cfg},
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_day", 640, 480, 120, 0, 0, &gc2053_mipi_120fps_v853_isp_cfg},
	{"gc2053_mipi", "gc2053_120fps_mipi_default_ini_v853_night", 640, 480, 120, 0, 1, &gc2053_mipi_120fps_v853_isp_cfg},
#endif
#ifdef CONFIG_ENABLE_AIISP
	{"gc2053_mipi", "gc2053_mipi_isp600_20240425_161918_aiisp", 1920, 1088, 10, 0, 2, &gc2053_mipi_aiisp_isp_cfg},
#else
	{"gc2053_mipi", "gc2053_mipi_isp600_20240425_161007_night", 1920, 1088, 20, 0, 1, &gc2053_mipi_day_isp_cfg},
#endif //CONFIG_ENABLE_AIISP
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_GC4663_MIPI
	{"gc4663_mipi", "gc4663_mipi_default_ini_day", 2560, 1440, 15, 0, 0, &gc4663_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_default_ini_night", 2560, 1440, 15, 0, 1, &gc4663_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_wdr_v853_isp_cfg_day", 2560, 1440, 15, 1, 0, &gc4663_mipi_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_mipi_wdr_v853_isp_cfg_night", 2560, 1440, 15, 1, 1, &gc4663_mipi_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_mipi_default_ini_day", 1280, 720, 120, 0, 0, &gc4663_120fps_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_mipi_default_ini_night", 1280, 720, 120, 0, 1, &gc4663_120fps_mipi_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_linear_to_wdr_day", 1280, 720, 120, 1, 0, &gc4663_120fps_mipi_linear_to_wdr_v853_isp_cfg},
	{"gc4663_mipi", "gc4663_120fps_linear_to_wdr_night", 1280, 720, 120, 1, 1, &gc4663_120fps_mipi_linear_to_wdr_v853_isp_cfg},
#endif // CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
	{"sc5336_mipi", "sc5336_130fps_mipi_default_ini_day", 1440, 400, 130, 0, 0, &sc5336_mipi_130fps_isp_cfg},
	{"sc5336_mipi", "sc5336_130fps_mipi_default_ini_night", 1440, 400, 130, 0, 1, &sc5336_mipi_130fps_isp_cfg},
	{"sc5336_mipi", "sc5336_mipi_default_ini_day", 2880, 1620, 20, 0, 0, &sc5336_mipi_isp_cfg},
	{"sc5336_mipi", "sc5336_mipi_default_ini_night", 2880, 1620, 20, 0, 1, &sc5336_mipi_isp_cfg},
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_BF2257CS_MIPI
	{"bf2257cs_mipi", "bf2257cs_mipi_2_isp600_20231219_201540_RGB_new", 1600, 1200, 30, 0, 0, &bf2257cs_mipi_rgb_isp_cfg},
	{"bf2257cs_mipi", "bf2257cs_mipi_2_isp600_20231205_154925_ir", 1600, 1200, 30, 1, 0, &bf2257cs_mipi_ir_isp_cfg},
#endif // CONFIG_SENSOR_BF2257CS_MIPI

#ifdef CONFIG_SENSOR_SC2355_MIPI
	{"sc2355_mipi", "sc2355_mipi_isp600_20220726_230636_RGB_LSC_V18", 1920, 1080, 15, 0, 0, &sc2355_mipi_rgb_isp_cfg},
	{"sc2355_mipi", "sc2355_mipi_isp600_20220720_235901_IR_V42", 1920, 1080, 15, 1, 0, &sc2355_mipi_ir_isp_cfg},
#endif // CONFIG_SENSOR_SC2355_MIPI

#ifdef CONFIG_SENSOR_F37P_MIPI
	{"f37p_mipi", "f37p_mipi_2_isp600_20231220_144145_gj", 1920, 1080, 15, 0, 0, &f37p_mipi_rgb_isp_cfg},
	{"f37p_mipi", "f37p_mipi_2_isp600_20230913_021938_gj_ir", 1920, 1080, 15, 1, 0, &f37p_mipi_ir_isp_cfg},
#endif // CONFIG_SENSOR_F37P_MIPI

#ifdef CONFIG_SENSOR_F355P_MIPI
	{"f355p_mipi", "f355p_mipi_2_isp600_20231102_231808_gj", 1920, 1080, 15, 0, 0, &f355p_mipi_rgb_isp_cfg},
	{"f355p_mipi", "f355p_mipi_2_isp600_20231103_124305_gj_ir", 1920, 1080, 15, 1, 0, &f355p_mipi_ir_isp_cfg},
#endif // CONFIG_SENSOR_F355P_MIPI

#ifdef CONFIG_SENSOR_OV02B10_MIPI
	{"ov02b10_mipi", "ov02b10_mipi_isp600_20221114b_color", 1600, 1200, 30, 0, 0, &ov02b10_mipi_isp_cfg},
	{"ov02b10_mipi", "ov02b10_mipi_isp600_20230811_104819_ir_v3", 1600, 1200, 30, 1, 0, &ov02b10_mipi_isp_ir_cfg},
#endif // CONFIG_SENSOR_OV02B10_MIPI

#ifdef CONFIG_SENSOR_GC1084_MIPI
#ifdef CONFIG_SENSOR_GC1084_8BIT_MIPI
	{"gc1084_mipi", "gc1084_mipi_v853_20230410_164555_day_8bit", 1280, 720, 15, 0, 0, &gc1084_mipi_v853_isp_cfg},
	{"gc1084_mipi", "gc1084_120fps_mipi_default_ini_v853_day_8bit", 640, 480, 120, 0, 0, &gc1084_mipi_120fps_v853_isp_cfg},
	{"gc1084_mipi", "gc1084_120fps_mipi_default_ini_v853_night_8bit", 640, 480, 120, 0, 1, &gc1084_mipi_120fps_v853_isp_cfg},
#else
	{"gc1084_mipi", "gc1084_mipi_v853_20230410_164555_day", 1280, 720, 15, 0, 0, &gc1084_mipi_v853_isp_cfg},
	{"gc1084_mipi", "gc1084_120fps_mipi_default_ini_v853_day", 640, 480, 120, 0, 0, &gc1084_mipi_120fps_v853_isp_cfg},
	{"gc1084_mipi", "gc1084_120fps_mipi_default_ini_v853_night", 640, 480, 120, 0, 1, &gc1084_mipi_120fps_v853_isp_cfg},
#endif
#ifdef CONFIG_ENABLE_AIISP
	{"gc1084_mipi", "gc1084_mipi_isp600_20230703_152809_aiisp", 1280, 720, 10, 0, 2, &gc1084_mipi_aiisp_isp_cfg},
#else
	{"gc1084_mipi", "gc1084_mipi_v853_20230410_164555_ir", 1280, 720, 15, 0, 1, &gc1084_mipi_v853_ir_isp_cfg},
#endif
#endif //CONFIG_SENSOR_GC1084_MIPI

#ifdef CONFIG_SENSOR_SC200AI_MIPI
#ifdef CONFIG_SENSOR_SC200AI_8BIT_MIPI
	{"sc200ai_mipi",  "sc200ai_mipi_isp600_20240115_182208_day_8bit", 1920, 1080, 20, 0, 0, &sc200ai_mipi_isp_cfg},
#else
	{"sc200ai_mipi",  "sc200ai_mipi_isp600_20240115_182208_day", 1920, 1080, 20, 0, 0, &sc200ai_mipi_isp_cfg},
#endif
#if CONFIG_ENABLE_AIISP
	{"sc200ai_mipi",  "sc200ai_mipi_isp600_20240125_203452_602_aiisp", 1920, 1080, 10, 0, 1, &sc200ai_mipi_aiisp_isp_cfg},
#else
	{"sc200ai_mipi",  "sc200ai_mipi_isp600_20240115_182208_ir", 1920, 1080, 20, 0, 1, &sc200ai_mipi_isp_cfg},
#endif
#endif

#ifdef CONFIG_SENSOR_CV5003_MIPI
	{"cv5003_mipi",  "cv5003_mipi_isp600_100fps_20250227_140027_V20", 1440, 810, 100, 0, 0, &cv5003_mipi_100fps_isp_cfg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_100fps_20250227_140027_V20", 1440, 810, 100, 0, 1, &cv5003_mipi_100fps_isp_cfg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_20250227_140027_V20", 2880, 1616, 15, 0, 0, &cv5003_mipi_isp_cfg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_20250227_111937_V20_IR", 2880, 1616, 15, 0, 1, &cv5003_mipi_ir_isp_cfg},
#endif

#else
	{"gc2053_mipi", "gc2053_mipi_default_ini_v853", 1920, 1088, 20, 0, 0, &gc2053_mipi_v853_isp_cfg},
#endif
#endif //(ISP_VERSION >= 600)
};

int parser_ini_info(struct isp_param_config *param, char *isp_cfg_name, char *sensor_name,
			int w, int h, int fps, int wdr, int ir, int sync_mode, int isp_id)
{
	int i;
	struct isp_cfg_pt *cfg = NULL;

	//load header parameter
	for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
		if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 6) &&
		    (w == cfg_arr[i].width) && (h == cfg_arr[i].height) &&
		    (fps == cfg_arr[i].fps) && (wdr == cfg_arr[i].wdr) &&
		    (ir == cfg_arr[i].ir)) {
				cfg = cfg_arr[i].cfg;
				ISP_PRINT("find %s_%d_%d_%d_%d [%s] isp config\n", cfg_arr[i].sensor_name,
					cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr, cfg_arr[i].isp_cfg_name);
				break;
		}
	}

	if (i == ARRAY_SIZE(cfg_arr)) {
		for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
			if (!strncmp(sensor_name, cfg_arr[i].sensor_name, 6) && (wdr == cfg_arr[i].wdr)) {
				cfg = cfg_arr[i].cfg;
				ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
				         cfg_arr[i].sensor_name, cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
				         cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
				break;
			}
		}
		if (i == ARRAY_SIZE(cfg_arr)) {
			for (i = 0; i < ARRAY_SIZE(cfg_arr); i++) {
				if (wdr == cfg_arr[i].wdr) {
					cfg = cfg_arr[i].cfg;
					ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use %s_%d_%d_%d_%d_%d -> [%s]\n", sensor_name, w, h, fps, wdr, ir,
					         cfg_arr[i].sensor_name, cfg_arr[i].width, cfg_arr[i].height, cfg_arr[i].fps, cfg_arr[i].wdr,
					         cfg_arr[i].ir, cfg_arr[i].isp_cfg_name);
					break;
				}
			}
		}
		if (i == ARRAY_SIZE(cfg_arr)) {
			ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp config, use default config [%s]\n",
				sensor_name, w, h, fps, wdr, ir, cfg_arr[i-1].isp_cfg_name);
			cfg = cfg_arr[i-1].cfg;// use the last one
		}
	}

	if (cfg != NULL) {
		strcpy(isp_cfg_name, cfg_arr[i].isp_cfg_name);
		param->isp_test_settings = *cfg->isp_test_settings;
		param->isp_3a_settings = *cfg->isp_3a_settings;
		param->isp_iso_settings = *cfg->isp_iso_settings;
		param->isp_tunning_settings = *cfg->isp_tunning_settings;
	}

	return 0;
}

struct isp_reg_array reg_arr[] = {
#if (ISP_VERSION >= 600)
#if defined CONFIG_SENSOR_GC2053_MIPI || defined CONFIG_SENSOR_GC4663_MIPI || defined CONFIG_SENSOR_SC5336_MIPI || \
	defined CONFIG_SENSOR_GC1084_MIPI || CONFIG_SENSOR_BF2257CS_MIPI || CONFIG_SENSOR_SC2355_MIPI || CONFIG_SENSOR_F37P_MIPI || \
	defined CONFIG_SENSOR_F355P_MIPI || CONFIG_SENSOR_OV02B10_MIPI || CONFIG_SENSOR_CV5003_MIPI

#ifdef CONFIG_SENSOR_GC2053_MIPI
#ifdef CONFIG_SENSOR_GC2053_8BIT_MIPI
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_day_reg_8bit", 640, 480, 120, 0, 0, &gc2053_mipi_480p_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_night_reg_8bit", 640, 480, 120, 0, 1, &gc2053_mipi_480p_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_1080p_20fps_day_reg_day_8bit", 1920, 1088, 20, 0, 0, &gc2053_mipi_isp_day_reg},
#else
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_day_reg", 640, 480, 120, 0, 0, &gc2053_mipi_480p_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_120fps_480p_night_reg", 640, 480, 120, 0, 1, &gc2053_mipi_480p_isp_day_reg},
	{"gc2053_mipi", "gc2053_mipi_1080p_20fps_day_reg_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_isp_day_reg},
#endif
#ifdef CONFIG_ENABLE_AIISP
	{"gc2053_mipi", "gc2053_mipi_1080p_aiisp_reg", 1920, 1088, 10, 0, 2, &gc2053_mipi_aiisp_isp_reg},
#else
	{"gc2053_mipi", "gc2053_mipi_1080p_20fps_day_reg_night", 1920, 1088, 20, 0, 1, &gc2053_mipi_isp_day_reg},
#endif
#endif //CONFIG_SENSOR_GC2053_MIPI

#ifdef CONFIG_SENSOR_GC4663_MIPI
	{"gc4663_mipi", "gc4663_mipi_720p_120fps_day_reg", 1280, 720, 120, 0, 0, &gc4663_mipi_720p_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_720p_120fps_night_reg", 1280, 720, 120, 0, 1, &gc4663_mipi_720p_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_15fps_day_reg_day", 2560, 1440, 15, 0, 0, &gc4663_mipi_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_15fps_day_reg_night", 2560, 1440, 15, 0, 1, &gc4663_mipi_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_wdr_15fps_day_reg_day", 2560, 1440, 15, 1, 0, &gc4663_mipi_wdr_isp_day_reg},
	{"gc4663_mipi", "gc4663_mipi_1440p_wdr_15fps_day_reg_night", 2560, 1440, 15, 1, 1, &gc4663_mipi_wdr_isp_day_reg},
#endif //CONFIG_SENSOR_GC4663_MIPI

#ifdef CONFIG_SENSOR_SC5336_MIPI
	{"sc5336_mipi", "sc5336_mipi_130fps_1440_400_day_reg", 1440, 400, 130, 0, 0, &sc5336_mipi_1440_400_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_130fps_1440_400_night_reg", 1440, 400, 130, 0, 1, &sc5336_mipi_1440_400_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_2880_1620_day_reg_day", 2880, 1620, 20, 0, 0, &sc5336_mipi_isp_day_reg},
	{"sc5336_mipi", "sc5336_mipi_2880_1620_day_reg_night", 2880, 1620, 20, 0, 1, &sc5336_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_SC5336_MIPI

#ifdef CONFIG_SENSOR_BF2257CS_MIPI
    {"bf2257cs_mipi", "bf2257cs_mipi_2_1600_1200_30fps_reg_day", 1600, 1200, 30, 0, 0, &bf2257cs_mipi_isp_day_reg},
    {"bf2257cs_mipi", "bf2257cs_mipi_2_1600_1200_30fps_reg_night", 1600, 1200, 30, 1, 0, &bf2257cs_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_BF2257CS_MIPI

#ifdef CONFIG_SENSOR_SC2355_MIPI
    {"sc2355_mipi", "sc2355_mipi_1080p_15fps_reg_day", 1920, 1080, 15, 0, 0, &sc2355_mipi_isp_day_reg},
    {"sc2355_mipi", "sc2355_mipi_1080p_15fps_reg_night", 1920, 1080, 15, 1, 0, &sc2355_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_SC2355_MIPI

#ifdef CONFIG_SENSOR_F37P_MIPI
    {"f37p_mipi", "f37p_mipi_2_1080p_15fps_reg_day", 1920, 1080, 15, 0, 0, &f37p_mipi_isp_day_reg},
    {"f37p_mipi", "f37p_mipi_2_1080p_15fps_reg_night", 1920, 1080, 15, 1, 0, &f37p_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_F37P_MIPI

#ifdef CONFIG_SENSOR_F355P_MIPI
    {"f355p_mipi", "f355p_mipi_2_1080p_15fps_reg_day", 1920, 1080, 15, 0, 0, &f355p_mipi_isp_day_reg},
    {"f355p_mipi", "f355p_mipi_2_1080p_15fps_reg_night", 1920, 1080, 15, 1, 0, &f355p_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_F355P_MIPI

#ifdef CONFIG_SENSOR_OV02B10_MIPI
    {"ov02b10_mipi", "ov02b10_mipi_1600_1200_30fps_reg_day", 1600, 1200, 30, 0, 0, &ov02b10_mipi_isp_day_reg},
    {"ov02b10_mipi", "ov02b10_mipi_1600_1200_30fps_reg_night", 1600, 1200, 30, 1, 0, &ov02b10_mipi_isp_day_reg},
#endif // CONFIG_SENSOR_OV02B10_MIPI

#ifdef CONFIG_SENSOR_GC1084_MIPI
#ifdef CONFIG_SENSOR_GC1084_8BIT_MIPI
    {"gc1084_mipi", "gc1084_mipi_120fps_360p_day_reg_8bit", 640, 480, 120, 0, 0, &gc1084_mipi_360p_isp_day_reg},
    {"gc1084_mipi", "gc1084_mipi_120fps_360p_night_reg_8bit", 640, 480, 120, 0, 1, &gc1084_mipi_360p_isp_day_reg},
    {"gc1084_mipi", "gc1084_mipi_720p_day_reg_8bit", 1280, 720, 15, 0, 0, &gc1084_mipi_isp_day_reg},
#else
    {"gc1084_mipi", "gc1084_mipi_120fps_360p_day_reg", 640, 480, 120, 0, 0, &gc1084_mipi_360p_isp_day_reg},
    {"gc1084_mipi", "gc1084_mipi_120fps_360p_night_reg", 640, 480, 120, 0, 1, &gc1084_mipi_360p_isp_day_reg},
    {"gc1084_mipi", "gc1084_mipi_720p_day_reg", 1280, 720, 15, 0, 0, &gc1084_mipi_isp_day_reg},
#endif
#ifdef CONFIG_ENABLE_AIISP
    {"gc1084_mipi", "gc1084_mipi_720p_aiisp_reg", 1280, 720, 10, 0, 2, &gc1084_mipi_isp_aiisp_reg},
#else
    {"gc1084_mipi", "gc1084_mipi_720p_night_reg", 1280, 720, 15, 0, 1, &gc1084_mipi_isp_day_reg},
#endif
#endif //CONFIG_SENSOR_GC1084_MIPI

#ifdef CONFIG_SENSOR_CV5003_MIPI
	{"cv5003_mipi",  "cv5003_mipi_isp600_100fps_20250109_112708_V17_reg", 1440, 810, 100, 0, 0, &cv5003_mipi_100fps_isp_day_reg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_100fps_20250109_112708_V17_reg", 1440, 810, 100, 0, 1, &cv5003_mipi_100fps_isp_day_reg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_20250227_140027_V20_reg", 2880, 1616, 15, 0, 0, &cv5003_mipi_isp_day_reg},
	{"cv5003_mipi",  "cv5003_mipi_isp600_20250227_140027_V20_reg", 2880, 1616, 15, 0, 1, &cv5003_mipi_isp_day_reg},
#endif

#else
    {"gc2053_mipi", "gc2053_mipi_default_ini_v853_reg_day", 1920, 1088, 20, 0, 0, &gc2053_mipi_isp_day_reg},
#endif
#endif //(ISP_VERSION >= 600)
};

int parser_ini_regs_info(struct isp_lib_context *ctx, char *sensor_name,
			int w, int h, int fps, int wdr, int ir)
{
	int i;
	struct isp_reg_pt *reg = NULL;

	for (i = 0; i < ARRAY_SIZE(reg_arr); i++) {
		if (!strncmp(sensor_name, reg_arr[i].sensor_name, 6) &&
			(w == reg_arr[i].width) && (h == reg_arr[i].height) &&// (fps == reg_arr[i].fps) &&
			(wdr == reg_arr[i].wdr)) {

			if (reg_arr[i].ir == ir) {
				reg = reg_arr[i].reg;
				ISP_PRINT("find %s_%d_%d_%d_%d ---- [%s] isp reg\n", reg_arr[i].sensor_name,
					reg_arr[i].width, reg_arr[i].height, reg_arr[i].fps, reg_arr[i].wdr, reg_arr[i].isp_cfg_name);
				break;
			}
		}
	}

	if (i == ARRAY_SIZE(reg_arr)) {
		ISP_WARN("cannot find %s_%d_%d_%d_%d_%d isp reg!!!\n", sensor_name, w, h, fps, wdr, ir);
		return -1;
	}

	if (reg != NULL) {
		if (reg->isp_save_reg)
			memcpy(ctx->load_reg_base, reg->isp_save_reg, ISP_LOAD_REG_SIZE);
		if (reg->isp_save_fe_table)
			memcpy(ctx->module_cfg.fe_table, reg->isp_save_fe_table, ISP_FE_TABLE_SIZE);
		if (reg->isp_save_bayer_table)
			memcpy(ctx->module_cfg.bayer_table, reg->isp_save_bayer_table, ISP_BAYER_TABLE_SIZE);
		if (reg->isp_save_rgb_table)
			memcpy(ctx->module_cfg.rgb_table, reg->isp_save_rgb_table, ISP_RGB_TABLE_SIZE);
		if (reg->isp_save_yuv_table)
			memcpy(ctx->module_cfg.yuv_table, reg->isp_save_yuv_table, ISP_YUV_TABLE_SIZE);
	}

	return 0;
}
