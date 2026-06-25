#ifndef __AW_MSG_H__
#define __AW_MSG_H__

#include <sys/time.h>
#include <unistd.h>

#include "aw_dev.h"
#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYNC_WORD_H 0xEF
#define SYNC_WORD_L 0xAA

#define USER_NAME_SIZE 32
#define PARITY_CHECK_OFFSET 2
#define PARITY_CHECK_LENGTH(msg_len) ((msg_len) - 3)

enum MSG_ID {
    // Module to Host (m->h)
    MID_REPLY = 0x00,  // request(command) reply message, success with data or fail with reason
    MID_NOTE = 0x01,   // note to host e.g. the position or angle of the face
    MID_IMAGE = 0x02,  // send image to host, Host to Module (h->m)
    MID_RESET = 0x10,      // stop and clear all in-processing messages. enter standby mode
    MID_GETSTATUS = 0x11,  // to ping the module and get the status
    MID_VERIFY = 0x12,     // to verify the person in front of the camera
    MID_ENROLL = 0x13,     // to enroll and register the persion in front of the camera
    MID_ENROLL_SINGLE = 0x1D,  // to enroll and register the persion in front of the camera, with one frame image
    MID_ENROLL_ITG = 0x26,       // Integrated enroll message, support all existing enroll type
    MID_PALM_ENROLL_ITG = 0x27,  // to enroll and register via palmar vein
    MID_SNAPIMAGE = 0x16,        // to snap a picture and save it
    MID_GETSAVEDIMAGE = 0x17,    // to get size of saved image
    MID_UPLOADIMAGE = 0x18,      // upload images
    MID_DELUSER = 0x20,          // Delete the specified user with user id
    MID_DELALL = 0x21,           // Delete all registerred users
    MID_GETUSERINFO = 0x22,      // Get user info
    MID_FACERESET = 0x23,        // Reset face status
    MID_GET_ALL_USERID = 0x24,   // get all users ID
    MID_GET_VERSION = 0x30,      // get version information
    MID_START_OTA = 0x40,        // ask the module to enter OTA mode
    MID_STOP_OTA = 0x41,         // ask the module to exit OTA mode
    MID_GET_OTA_STATUS = 0x42,   // query the current ota status
    MID_OTA_HEADER = 0x43,       // the ota header data
    MID_OTA_PACKET = 0x44,       // the data packet, carries real firmware data
    MID_INIT_ENCRYPTION = 0x50,  // initialize encrypted communication
    MID_CONFIG_BAUDRATE = 0x51,  // config uart baudrate
    MID_SET_RELEASE_ENC_KEY = 0x52,  // set release encrypted key
    MID_SET_DEBUG_ENC_KEY = 0x53,    // set debug encrypted key
    MID_GET_LOGFILE = 0x60,          // get log file
    MID_UPLOAD_LOGFILE = 0x61,       // upload log file
    MID_SET_THRESHOLD_LEVEL = 0xD4,  // Set threshold level
    MID_POWERDOWN = 0xED,            // be prepared to power off
    MID_DEBUG_MODE = 0xF0,           // enter debug mode
    MID_GET_DEBUG_INFO = 0xF1,       // get size of debug information
    MID_UPLOAD_DEBUG_INFO = 0xF2,    // upload debug information
    MID_DEMOMODE = 0xFE,  // enter demo mode, verify flow will skip feature comparation step.
    MID_MAX = 0xFF        // reserved
};

enum MMI_RET {
    MR_SUCCESS = 0,                 // success
    MR_REJECTED = 1,                // module rejected this command
    MR_ABORTED = 2,                 // algo aborted
    MR_FAILED4_CAMERA = 4,          // camera open failed
    MR_FAILED4_UNKNOWNREASON = 5,   // UNKNOWN_ERROR
    MR_FAILED4_INVALIDPARAM = 6,    // invalid param
    MR_FAILED4_NOMEMORY = 7,        // no enough memory
    MR_FAILED4_UNKNOWNUSER = 8,     // no user enrolled
    MR_FAILED4_MAXUSER = 9,         // exceed maximum user number
    MR_FAILED4_FACEENROLLED = 10,   // this face has been enrolled
    MR_FAILED4_LIVENESSCHECK = 12,  // liveness check failed
    MR_FAILED4_TIMEOUT = 13,        // exceed the time limit
    MR_FAILED4_AUTHORIZATION = 14,  // authorization failed
    MR_FAILED4_READ_FILE = 19,      // read file failed
    MR_FAILED4_WRITE_FILE = 20,     // write file failed
    MR_FAILED4_NO_ENCRYPT = 21,     // encrypt must be set
};

enum NOTE_ID {
    NID_READY = 0,
    NID_FACE_STATE = 1,
    NID_UNKNOWNERROR = 2,
    NID_OTA_DONE = 3,
    NID_AUTHORIZATION = 8,
};

typedef uint8_t s_face_dir;
enum FACE_DIR {
    FACE_UP = 0x10,
    FACE_DOWN = 0x08,
    FACE_LEFT = 0x04,
    FACE_RIGHT = 0x02,
    FACE_MIDDLE = 0x01,
};

enum VERIFY_TYPE {
    VERIFY_TYPE_FACE = 1,
    VERIFY_TYPE_PALM = 2,
};

enum ENROLL_STATE {
    ENROLL_FACE_UNFINISH = 0,
    ENROLL_FACE_FINISH = 1,
    ENROLL_PALM_FINISH = 2,
};

typedef struct {
    uint8_t mid;     // the command(message) id to reply (the request message ID)
    uint8_t result;  // command handling result: success or failed -> s_msg_result
    uint8_t data[0];
} msg_reply_data_t;

// note msg
typedef struct {
    uint8_t nid;  // note id
    uint8_t data[0];
} msg_note_data_t;

// verify
typedef struct {
    uint8_t pd_rightaway;  // power down right away after verifying
    uint8_t timeout;       // timeout, unit second, default 10s, max is 255s
} msg_verify_data_t;

/* message reply verify data definitions */
typedef struct {
    uint8_t user_id_heb;
    uint8_t user_id_leb;
    uint8_t user_name[USER_NAME_SIZE];
    uint8_t admin;
    uint8_t id_type;  // 01:face, 02:palm
} msg_reply_verify_data_t;

// enroll user by face
typedef struct {
    uint8_t admin;  // the user will be set to admin
    uint8_t user_name[USER_NAME_SIZE];
    s_face_dir face_direction;
    uint8_t timeout;
} msg_enroll_data_t;

// enroll user by palm
typedef struct {
    uint8_t admin;  // the user will be set to admin
    uint8_t user_name[USER_NAME_SIZE];
    uint8_t direction;    // Invalid parameter
    uint8_t enroll_type;  // Invalid parameter
    uint8_t duplicate;    // Invalid parameter
    uint8_t timeout;      // timeout unit second default 10s
} msg_palm_enroll_data_t;

// reply for enroll
typedef struct {
    uint8_t user_id_heb;
    uint8_t user_id_leb;
    uint8_t face_direction;
    uint8_t enroll_state;  // 00: unfinished, 01:finish face enroll, 02:finish palm enroll
} msg_reply_enroll_data_t;

typedef struct {
    int16_t state;  // corresponding to FACE_STATE_*
    // position
    int16_t left;  // in pixel
    int16_t top;
    int16_t right;
    int16_t bottom;
    // pose
    int16_t yaw;    // up and down in vertical orientation
    int16_t pitch;  // right or left turned in horizontal orientation
    int16_t roll;   // slope
} msg_note_face_data_t;

typedef struct {
    uint8_t user_id_heb;
    uint8_t user_id_leb;
} msg_getuserinfo_data_t;

typedef struct {
    uint8_t user_id_heb;
    uint8_t user_id_leb;
    uint8_t user_name[USER_NAME_SIZE];
    uint8_t admin;
} msg_reply_getuserinfo_data_t;

// delete user
typedef struct {
    uint8_t user_id_heb;  // high eight bits of user_id to be deleted
    uint8_t user_id_leb;  // low eight bits pf user_id to be deleted
} msg_deluser_data_t;

/* |SyncWord(2byte) | MsgID(1byte) | Size(2byte) | Data(Size byte) | ParityCheck(1byte) |*/
// PacketHeader + Data + ParityCheck

typedef struct aw_packet_header_s {
    uint8_t sync_word_heb;  // sync word high eight bits
    uint8_t sync_word_leb;  // sync word low eight bits
    uint8_t msg_id;
    uint8_t data_len_heb;
    uint8_t data_len_leb;
} __attribute__((packed)) aw_packet_header_t;

typedef struct aw_packet_crc_s {
    uint8_t packet_crc;
} __attribute__((packed)) aw_packet_crc_t;

// general message
typedef struct {
    uint8_t sync_word_heb;  // sync word high eight bits
    uint8_t sync_word_leb;  // sync word low eight bits
    uint8_t mid;            // the message id
    uint8_t size_heb;       // high eight bits
    uint8_t size_leb;       // low eight bits
    uint8_t data[0];        // data
    uint8_t parity_check;   // parity check
} msg_t;

int msg_write_packet(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len);
int msg_read_packet(aw_dev_t *aw_dev_info, uint8_t *data, uint32_t len);
uint8_t get_parity_checksum(uint8_t *pack, uint16_t pack_len);
void msg_prepare_verify_reply_data(uint16_t usr_id, char *name, uint8_t admin, uint8_t id_type);
void msg_prepare_enroll_reply_data(uint16_t usr_id, uint8_t face_direction, uint8_t enroll_state);
void msg_prepare_note_face_data(int16_t state, int16_t left, int16_t top, int16_t right,
                                int16_t bottom, int16_t yaw, int16_t pitch, int16_t roll);
int msg_send_reply(aw_dev_t *aw_dev_info, uint8_t mid, uint8_t reply_result);
int msg_send_note(aw_dev_t *aw_dev_info, uint8_t nid, uint8_t nid_result);

#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* __AW_MSG_H__ */
