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
#ifndef _SAMPLE_CDR_RECORDER_CONF_H_
#define _SAMPLE_CDR_RECORDER_CONF_H_

#define CFG_EncRecRefBufReduceEnable                "enc_rec_ref_buf_reduce_enable"
#define CFG_VeRxInputBufmultiplexEnable             "ve_rx_input_buf_multiplex_enable"
#define CFG_PRODUCTMODE                             "product_mode"
#define CFG_ENC_RCMODE                              "enc_rcmode"
#define CFG_RegionLinkEnable                        "region_link_enable"
#define CFG_RegionLinkTexDetectEnable               "region_link_tex_detect_enable"
#define CFG_RegionLinkMotionDetectEnable            "region_link_motion_detect_enable"
#define CFG_RegionLinkMotionDetectInv               "region_link_motion_detect_inv"

#define CFG_CAPTURE_SAMPLE_RATE                     "capture_sample_rate"
#define CFG_CAPTURE_BIT_WIDTH                       "capture_bit_witdh"
#define CFG_CAPTURE_CHANNEL_CNT                     "capture_channel_cnt"
#define CFG_CAPTURE_ANS_EN                          "capture_ans_en"
#define CFG_CAPTURE_AGC_EN                          "capture_agc_en"
#define CFG_CAPTURE_AEC_EN                          "capture_aec_en"
#define CFG_AENC_TYPE                               "aenc_type"
#define CFG_AENC_BITRATE                            "aenc_bitrate"

#define CFG_RECORDER_VIPP_DEV                       "recorder_vi_dev"
#define CFG_RECORDER_VI_VIRCHN                      "recorder_vi_virchn"
#define CFG_RECORDER_ISP_DEV                        "recorder_isp_dev"
#define CFG_RECORDER_CAP_WIDTH                      "recorder_cap_width"
#define CFG_RECORDER_CAP_HEIGHT                     "recorder_cap_height"
#define CFG_RECORDER_CAP_FRAMERATE                  "recorder_cap_frmrate"
#define CFG_RECORDER_CAP_FORMAT                     "recorder_cap_format"
#define CFG_RECORDER_VI_BUFNUM                      "recorder_vi_bufnum"
#define CFG_RECORDER_ENABLEWDR                      "recorder_enable_WDR"
#define CFG_RECORDER_VI_STITCH_MODE                 "recorder_vi_stitch_mode"
#define CFG_RECORDER_ENC_CHN                        "recorder_enc_chn"
#define CFG_RECORDER_ENC_ONLINE                     "recorder_enc_online"
#define CFG_RECORDER_ENC_ONLINE_SHARE_BFUNUM        "recorder_enc_online_share_bufbum"
#define CFG_RECORDER_ENC_ISP2VE                     "recorder_enc_isp2ve"
#define CFG_RECORDER_ENC_VE2ISP                     "recorder_enc_ve2isp"
#define CFG_RECORDER_ENC_TYPE                       "recorder_enc_type"
#define CFG_RECORDER_ENC_WIDTH                      "recorder_enc_width"
#define CFG_RECORDER_ENC_HEIGHT                     "recorder_enc_height"
#define CFG_RECORDER_ENC_FRAMERATE                  "recorder_enc_frmrate"
#define CFG_RECORDER_ENC_BITRATE                    "recorder_enc_bitrate"
#define CFG_RECORDER_VEREFFRAMELBCMODE              "recorder_enc_refframelbcmode"
#define CFG_RECORDER_REC_DURATION                   "recorder_rec_duration"
#define CFG_RECORDER_REC_FILECNT                    "recorder_rec_file_cnt"
#define CFG_RECORDER_REC_FILE_FORMART               "recorder_rec_file_format"
#define CFG_RECORDER_REC_FILE                       "recorder_rec_file"

#define CFG_TAKEPIC_ENABLE                          "takepic_enable"
#define CFG_TAKEPIC_ONLINE                          "takepic_online"
#define CFG_TAKEPIC_INTERVAL                        "takepic_interval"
#define CFG_TAKEPIC_THUMB_ENABLE                    "takepic_thumb_enable"
#define CFG_TAKEPIC_VIPP_DEV                        "takepic_vi_dev"
#define CFG_TAKEPIC_VI_VIRCHN                       "takepic_vi_virchn"
#define CFG_TAKEPIC_ENC_CHN                         "takepic_enc_chn"
#define CFG_TAKEPIC_FILE_CNT                        "takepic_file_cnt"
#define CFG_TAKEPIC_FILE                            "takepic_file"

#define CFG_PREVIEW_ENABLE                          "preview_enable"
#define CFG_PREVIEW_MODE                            "preview_mode"
#define CFG_PREVIEW_RTSP_ID                         "preview_rtsp_id"
#define CFG_PREVIEW_RTSP_Net_TYPE                   "preview_rtsp_net_type"
#define CFG_PREVIEW_DISP_X                          "preview_disp_x"
#define CFG_PREVIEW_DISP_Y                          "preview_disp_y"
#define CFG_PREVIEW_DISP_WIDTH                      "preview_disp_width"
#define CFG_PREVIEW_DISP_HEIGHT                     "preview_disp_height"
#define CFG_PREVIEW_DISP_DEV                        "preview_disp_dev"
#define CFG_PREVIEW_VIPP_DEV                        "preview_vi_dev"
#define CFG_PREVIEW_VI_VIRCHN                       "preview_vi_virchn"
#define CFG_PREVIEW_ISP_DEV                         "preview_isp_dev"
#define CFG_PREVIEW_CAP_WIDTH                       "preview_cap_width"
#define CFG_PREVIEW_CAP_HEIGHT                      "preview_cap_height"
#define CFG_PREVIEW_CAP_FRAMERATE                   "preview_cap_frmrate"
#define CFG_PREVIEW_CAP_FORMAT                      "preview_cap_format"
#define CFG_PREVIEW_VI_BUFNUM                       "preview_vi_bufnum"
#define CFG_PREVIEW_ENABLEWDR                       "preview_enable_WDR"
#define CFG_PREVIEW_VI_STITCH_MODE                  "preview_vi_stitch_mode"
#define CFG_PREVIEW_ENC_CHN                         "preview_enc_chn"
#define CFG_PREVIEW_ENC_ONLINE                      "preview_enc_online"
#define CFG_PREVIEW_ENC_ONLINE_SHARE_BFUNUM         "preview_enc_online_share_bufbum"
#define CFG_PREVIEW_ENC_ISP2VE                      "preview_enc_isp2ve"
#define CFG_PREVIEW_ENC_VE2ISP                      "preview_enc_ve2isp"
#define CFG_PREVIEW_ENC_TYPE                        "preview_enc_type"
#define CFG_PREVIEW_ENC_WIDTH                       "preview_enc_width"
#define CFG_PREVIEW_ENC_HEIGHT                      "preview_enc_height"
#define CFG_PREVIEW_ENC_FRAMERATE                   "preview_enc_frmrate"
#define CFG_PREVIEW_ENC_BITRATE                     "preview_enc_bitrate"
#define CFG_PREVIEW_VEREFFRAMELBCMODE               "preview_enc_refframelbcmode"

#define CFG_TEST_DURATION                           "test_duration"

#endif
