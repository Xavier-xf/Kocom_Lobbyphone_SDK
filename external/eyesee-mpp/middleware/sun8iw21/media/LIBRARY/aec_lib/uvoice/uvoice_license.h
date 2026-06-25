
#ifndef UVOICE_LICENSE_PARAM_DEF_H

#define UVOICE_LICENSE_PARAM_DEF_H

/*
 * 用户需要实现的回调函数，用来把https_post_body通过HTTPS协议，POST到UVoice的授权服务器
 * 服务器的返回数据，需要通过此函数return回来。
 * 当下的服务器地址为：
 * https://srv01.51asr.com:8007/asrsn_active2"
 * 因此，这个回调函数的行为应该和以下curl命令等效：
 * curl -k -s --data-binary $https_post_body https://srv01.51asr.com:8007/asrsn_active2"
 */
typedef const char* (*uvoice_auth_callback_t)(const char* https_post_body);

/*--------------------------------------------------------------------------------------*/
/*
 * 结构体说明如下：
 * license: UVoice发布的授权码
 * license_path:此参数在activate_type=0的时候表示服务器授权设备成功后，授权信息写入的文件。
 *              如果activate_type=1,用flash信息进行授权，此时，这个参数表示flash对应的设备文件。
 * uuid：设备的唯一码
 * activate_type:记录授权信息的类型，0：表示以文件方式记录，1：flash,2：efuse
 * auth_cb:用户需要实现的回调函数，参考上边函数指针的说明
 */
typedef struct uv_activate_param {
        char *license;
        char *license_path;
        char *uuid;
        int  activate_type;
        uvoice_auth_callback_t auth_cb;
} uv_activate_param;

#endif

