#ifndef _SUNXI_SIMPLE_IOMMU_H
#define _SUNXI_SIMPLE_IOMMU_H

#include <stdint.h>

int simple_iommu_map_region(unsigned long iova, unsigned long pa, unsigned long size);
int simple_iommu_unmap_region(unsigned long iova, unsigned long size);
int simple_iommu_splice_npu_buffer(unsigned long iova, void *buf1, void *buf2,
				unsigned long size, unsigned long batch);

#endif
