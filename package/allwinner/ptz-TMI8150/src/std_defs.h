#ifndef STD_DEFS_H
#define STD_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int U32;

/*! error code  */
typedef enum sk_status_code_e
{
	AW_SUCCESS= 0,
	AW_ERROR_BAD_PARAMETER,          					 /* Bad parameter passed       */
	AW_ERROR_NO_MEMORY,               						  /* Memory allocation failed   */
	AW_ERROR_UNKNOWN_DEVICE,          					  /* Unknown device name        */
	AW_ERROR_ALREADY_INITIALIZED,       				  /* Device already initialized */
	AW_ERROR_NO_FREE_HANDLES,          					 /* Cannot open device again   */
	AW_ERROR_INVALID_HANDLE,           					 /* Handle is not valid        */
	AW_ERROR_INVALID_ID,           						 /* ID is not valid        */
	AW_ERROR_FEATURE_NOT_SUPPORTED,     				/* Feature unavailable        */
	AW_ERROR_INTERRUPT_INSTALL,         					/* Interrupt install failed   */
	AW_ERROR_INTERRUPT_UNINSTALL,       				/* Interrupt uninstall failed */
	AW_ERROR_TIMEOUT,                  						 /* Timeout occured            */
	AW_ERROR_DEVICE_BUSY,               					 /* Device is currently busy   */
	AW_ERROR_NO_INIT,									 /* not init    */
	AW_ERROR_MAX_COUNT,								 /* that more than max count    */
	AW_ERROR_INSUFFICIENT_BUFFER,                    /* insufficient buffer    */
	AW_FAILED,              							 		/* UNKNOWN ERROR   */
	AW_STATUS_ENABLE,
	AW_STATUS_DISABLE,
	AW_ERROR_DEVICE_NOT_ONLINE,
	AW_ERROR_CLOSED_BY_REMOTE
} status_code_t;


#ifdef __cplusplus
}
#endif

#endif /*STD_DEFS_H*/

/*eof*/





