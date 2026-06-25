#include "sensor_register.h"

struct sensor_cfg_array sensor_array[] = {
#ifdef CONFIG_SENSOR_GC4663_MIPI
        {"gc4663_mipi", &gc4663_core},
#endif
#ifdef CONFIG_SENSOR_GC2053_MIPI
        {"gc2053_mipi", &gc2053_core},
#endif
#ifdef CONFIG_SENSOR_SC2355_MIPI
        {"sc2355_mipi", &sc2355_core},
#endif
#ifdef CONFIG_SENSOR_SC202CS_VC_MIPI
        {"sc202cs_vc_mipi", &sc202cs_vc_core},
#endif
#ifdef CONFIG_SENSOR_SC5336_MIPI
        {"sc5336_mipi", &sc5336_core},
#endif
#ifdef CONFIG_SENSOR_SC5336P_MIPI
        {"sc5336p_mipi", &sc5336p_core},
#endif
#ifdef CONFIG_SENSOR_GC1054_MIPI
        {"gc1054_mipi", &gc1054_core},
#endif
#ifdef CONFIG_SENSOR_GC1084_MIPI
        {"gc1084_mipi", &gc1084_core},
#endif
#ifdef CONFIG_SENSOR_IMX319_MIPI
        {"imx319_mipi", &imx319_core},
#endif
#ifdef CONFIG_SENSOR_IMX335_MIPI
        {"imx335_mipi", &imx335_core},
#endif
#ifdef CONFIG_SENSOR_SC035HGS_MIPI
        {"sc035hgs_mipi", &sc035hgs_core},
#endif
#ifdef CONFIG_SENSOR_TP9950_MIPI
        {"tp9950_mipi", &tp9950_mipi_core},
#endif
#ifdef CONFIG_SENSOR_TP9953_DVP
        {"tp9953_dvp",  &tp9953_dvp_core},
#endif
#ifdef CONFIG_SENSOR_TP2815_MIPI
        {"tp2815_mipi",  &tp2815_core},
#endif
#ifdef CONFIG_SENSOR_BF2253L_MIPI
        {"bf2253l_mipi", &bf2253l_core},
#endif
#ifdef CONFIG_SENSOR_N5_DVP
        {"n5_dvp", &n5_core},
#endif
#ifdef CONFIG_SENSOR_BF2257CS_MIPI
        {"bf2257cs_mipi", &bf2257cs_core},
#endif
#ifdef CONFIG_SENSOR_F355P_MIPI
        {"f355p_mipi", &f355p_core},
#endif
#ifdef CONFIG_SENSOR_F355P_DVP
        {"f355p_dvp", &f355p_dvp_core},
#endif
#ifdef CONFIG_SENSOR_F37P_DVP
        {"f37p_dvp", &f37p_dvp_core},
#endif
#ifdef CONFIG_SENSOR_F37P_MIPI
        {"f37p_mipi", &f37p_core},
#endif
#ifdef CONFIG_SENSOR_OV02B10_MIPI
        {"ov02b10_mipi", &ov02b10_core},
#endif
#ifdef CONFIG_SENSOR_CV5003_MIPI
        {"cv5003_mipi", &cv5003_core},
#endif
};

struct sensor_fuc_core *find_sensor_func(char *sensor_name)
{
        int i;

        if (!ARRAY_SIZE(sensor_array)) {
                vin_err("%s:sensor_array is NULL\n", __func__);
                return NULL;
        }

	if (!sensor_name) {
		vin_err("sensor_name is NULL!\n");
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(sensor_array); i++) {
                if (!strncmp(sensor_name, sensor_array[i].sensor_name, strlen(sensor_array[i].sensor_name))) {
                        vin_print("%s: find %s sensor core function from %s sensor_core!\n", __func__, sensor_name, sensor_array[i].sensor_name);
                        return sensor_array[i].sensor_core;
                }
        }

        return NULL;
}
