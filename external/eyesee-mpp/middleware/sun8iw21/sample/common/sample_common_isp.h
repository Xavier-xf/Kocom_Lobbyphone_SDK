#ifndef __SAMPLE_COMMON_ISP_H__
#define __SAMPLE_COMMON_ISP_H__

#include "media/mpi_vi.h"
#include "mm_common.h"
#include "mm_comm_rc.h"
#include "media/mpi_isp.h"

#ifdef __cplusplus
       extern "C" {
#endif

#define BUF_SIZE 1024
#define SUXI_DUMP_DUMP_PATH "/sys/class/sunxi_dump/dump"
#define SUXI_DUMP_WRITE_PATH "/sys/class/sunxi_dump/write"
#define MIPIA_PAYLOAD_REGS 0x05811118
#define MIPIB_PAYLOAD_REGS 0x05811518
#define MIPI_PAYLOAD_ERRPD_MASK 0x1f80
#define MIPIA_PHY_DESKEW_REGS 0x05810118
#define MIPIB_PHY_DESKEW_REGS 0x05810218
#define MIPI_PHY_DESKEW_CLK_DLY_MASK 0x01f
#define MIPI_PHY_DESKEW_CLK_DLY_SET 20
#define MIPI_PHY_DESKEW_CLK_DLY_MAX 0x01f
#define REGS_FLUSH 0xffffffff

typedef enum {
    ISP_AE_MDOE = 1 << 0,
    ISP_AE_EXP_BAIS = 1 << 1,
    ISP_AE_ISO_SENSITIVE = 1 << 2,
    ISP_AE_METERING = 1 << 3,
    ISP_AE_EV_IDX = 1 << 4,
    ISP_AE_LOCK = 1 << 5,
    ISP_AE_TABLE = 1 << 6,
    ISP_AE_SCENE = 1 << 7,
    ISP_AE_FLICKER = 1 << 8,
    ISP_AE_FACE_AE = 1 << 9,
    ISP_AWB_MODE = 1 << 10,
    ISP_SPEC_BRIGHTNESS = 1 << 11,
    ISP_SPEC_CONTRAST = 1 << 12,
    ISP_SPEC_SATURATION = 1 << 13,
    ISP_SPEC_SHARPNESS = 1 << 14,
    ISP_SPEC_HUE = 1 << 15,
    ISP_SPEC_PLTM = 1 << 16,
    ISP_SPEC_2DNR = 1 << 17,
    ISP_SPEC_3DNR = 1 << 18,
    ISP_SPEC_COLOR_EFFECT = 1 << 19,
    ISP_SPEC_MIRROR = 1 << 20,
    ISP_SPEC_FLIP = 1 << 21,
    ISP_SETTINGS_FREQ = 1 << 22,
    TEST_ISP_ALL = (1 << 23) - 1,
    TEST_TOTAL = 23
} TestType;

typedef enum {
    TEST_ISP_AE = ISP_AE_MDOE | ISP_AE_EXP_BAIS | ISP_AE_ISO_SENSITIVE | ISP_AE_METERING |
                    ISP_AE_METERING | ISP_AE_EV_IDX | ISP_AE_LOCK | ISP_AE_TABLE |
                    ISP_AE_SCENE | ISP_AE_FLICKER |ISP_AE_FACE_AE,
    TEST_ISP_AWB = ISP_AWB_MODE,
    TEST_ISP_SPEC = ISP_SPEC_BRIGHTNESS | ISP_SPEC_CONTRAST | ISP_SPEC_SATURATION |
                    ISP_SPEC_SHARPNESS | ISP_SPEC_HUE | ISP_SPEC_PLTM | ISP_SPEC_COLOR_EFFECT,
    TEST_ISP_NR = ISP_SPEC_2DNR | ISP_SPEC_3DNR,
    TEST_ISP_FLIP = ISP_SPEC_MIRROR | ISP_SPEC_FLIP,
} TestGroupType;

typedef struct IspAeTestCfg {
    unsigned int mAeMode;
    unsigned int mExpBais;
    unsigned int mIsoSensitive;
    int mAeMetering;
    int mAeEvIdx;
    int mAeMaxEvIdx;
    unsigned int mAeLock;
    struct ae_table_info mAeTable;
    enum ae_table_mode mAeScene;
    int mAeFlicker;
    struct isp_face_ae_attr_info mFaceAeInfo;
    RECT_S mFaceRoiRgn[AE_FACE_MAX_NUM];
    SIZE_S mRes;
    unsigned int mTiggerFaceAeForTest;
} IspAeTestCfg;

typedef struct IspAwbTestCfg {
    unsigned int mAwbMode;
    unsigned int mAwbColorTemp;
    struct isp_wb_gain mAwbGain;
} IspAwbTestCfg;

typedef struct IspSpecEffectTestCfg {
    int mBrightess;
    int mContrast;
    int mSaturation;
    int mSharpness;
    int mHue;
    int mPltmStren;
    int m2dnrStren;
    int m3dnrStren;
    enum colorfx mColorStyle;
    int mMirror;
    int mFlip;
} IspSpecEffectTestCfg;

typedef struct IspSettingsTestCfg {
    unsigned short mIspAlgoFreq;
} IspSettingsTestCfg;

typedef struct IspApiTaskConfig {
    void (*tasks[TEST_TOTAL])();
    int completed[TEST_TOTAL];
    int mTasksNum;
    int mCurrentTask;
    int mtaskSchedule;
    unsigned int mTaskList[TEST_TOTAL];
    unsigned int mActuralTaskNum;
} IspApiTaskConfig;

typedef struct IspApiTestCtrlConfig {
    unsigned int mTestIntervalMs;
    unsigned int mTetstIspChannelId;
    pthread_t ispTestThreadId;
    IspApiTaskConfig mIspApiTask;
    IspAeTestCfg mTestAeCfg;
    IspAwbTestCfg mTetstAwbCfg;
    IspSpecEffectTestCfg mTestSpecEffectCfg;
    IspSettingsTestCfg mTetstSettingsCfg;
} IspApiTestCtrlConfig;

int ispApiTestInit(IspApiTestCtrlConfig *pIspTestContext);
int ispApiTestExit(IspApiTestCtrlConfig *pIspTestContext);


typedef struct MipiDeskConfig {
    unsigned int mPayLoadStatus;
    unsigned int mPayLoadAddr;
    unsigned int mPhyDeskRegsAddr;
    unsigned int mPhyDeskValue;
    unsigned int mPhyDeskValueMax;
    unsigned int mCalcDeskStart;
    unsigned int mCalcDeskPass;
    unsigned int mCalcDeskEnd;
    unsigned int mCalcDeskBest;
} MipiDeskConfig;

typedef struct DetMipiDeskCtrlConfig{
    pthread_t mdetMipiDeskThreadId;
    bool mdetMipiDeskEnable;
    unsigned int mDetectIntervalMs;
    MipiDeskConfig mMipiDeskCfg;
    bool mdetMipiDeskDone;
} DetMipiDeskCtrlConfig;

int detectMipiDeskTestInit(DetMipiDeskCtrlConfig *pDetMipiDeskContext);
int detectMipiDeskTestExit(DetMipiDeskCtrlConfig *pDetMipiDeskContext);

#ifdef __cplusplus
       }
#endif

#endif