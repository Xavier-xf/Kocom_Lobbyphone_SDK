#ifndef SUNXI_RPROC_H_
#define SUNXI_RPROC_H_

#include <metal/compiler.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_AMP_TRACE_SUPPORT
METAL_PACKED_BEGIN
struct fw_rsc_aw_trace
{
	uint32_t type;
	uint32_t da;
	uint32_t len;
	uint32_t reserved;
	uint8_t name[32];
} METAL_PACKED_END;
#endif

struct rproc_global_impls {
	void *ops;
	void *priv;
};

extern struct rproc_global_impls sunxi_rproc_impls[];
extern const size_t sunxi_rproc_impls_size;

#define GET_RPROC_GLOBAL_IMPLS_ITEMS(impls_array) \
        sizeof(impls_array) / sizeof(impls_array[0]);

#ifdef __cplusplus
}
#endif


#endif /* SUNXI_RPROC_H_ */
