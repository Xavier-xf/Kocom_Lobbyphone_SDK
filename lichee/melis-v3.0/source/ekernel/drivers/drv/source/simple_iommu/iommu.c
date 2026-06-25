#include <hal_osal.h>
#include <sunxi_hal_common.h>
#include <stdint.h>
#include <errno.h>

#define SUNXI_IOMMU_BASE			(0x02010000)

#define IOMMU_VERSION_REG               0x0000
#define IOMMU_RESET_REG                 0x0010
#define IOMMU_ENABLE_REG                0x0020
#define IOMMU_BYPASS_REG                0x0030
#define IOMMU_AUTO_GATING_REG           0x0040
#define IOMMU_WBUF_CTRL_REG             0x0044
#define IOMMU_OOO_CTRL_REG              0x0048
#define IOMMU_4KB_BDY_PRT_CTRL_REG      0x004C
#define IOMMU_TTB_REG                   0x0050
#define IOMMU_TLB_ENABLE_REG            0x0060
#define IOMMU_TLB_PREFETCH_REG          0x0070
#define IOMMU_TLB_FLUSH_ENABLE_REG      0x0080
#define IOMMU_TLB_IVLD_MODE_SEL_REG     0x0084
#define IOMMU_TLB_IVLD_START_ADDR_REG   0x0088
#define IOMMU_TLB_IVLD_END_ADDR_REG     0x008C
#define IOMMU_TLB_IVLD_ADDR_REG         0x0090
#define IOMMU_TLB_IVLD_ADDR_MASK_REG    0x0094
#define IOMMU_TLB_IVLD_ENABLE_REG       0x0098
#define IOMMU_PC_IVLD_MODE_SEL_REG      0x009C
#define IOMMU_PC_IVLD_ADDR_REG          0x00A0
#define IOMMU_PC_IVLD_START_ADDR_REG    0x00A4
#define IOMMU_PC_IVLD_ENABLE_REG        0x00A8
#define IOMMU_PC_IVLD_END_ADDR_REG      0x00AC
#define IOMMU_DM_AUT_CTRL_REG0          0x00B0
#define IOMMU_DM_AUT_CTRL_REG1          0x00B4
#define IOMMU_DM_AUT_CTRL_REG2          0x00B8
#define IOMMU_DM_AUT_CTRL_REG3          0x00BC
#define IOMMU_DM_AUT_CTRL_REG4          0x00C0
#define IOMMU_DM_AUT_CTRL_REG5          0x00C4
#define IOMMU_DM_AUT_CTRL_REG6          0x00C8
#define IOMMU_DM_AUT_CTRL_REG7          0x00CC
#define IOMMU_DM_AUT_OVWT_REG           0x00D0
#define IOMMU_INT_ENABLE_REG            0x0100
#define IOMMU_INT_CLR_REG               0x0104
#define IOMMU_INT_STA_REG               0x0108

#define NUM_ENTRIES_PDE                 4096
#define NUM_ENTRIES_PTE                 256
#define PD_SIZE                         (NUM_ENTRIES_PDE * sizeof(u32))
#define PT_SIZE                         (NUM_ENTRIES_PTE * sizeof(u32))

#define IOMMU_PD_SHIFT                  20
#define IOMMU_PD_MASK                   (~((1UL << IOMMU_PD_SHIFT) - 1))

#define IOMMU_PT_SHIFT                  12
#define IOMMU_PT_MASK                   (~((1UL << IOMMU_PT_SHIFT) - 1))

#define PAGE_OFFSET_MASK                ((1UL << IOMMU_PT_SHIFT) - 1)
#define IOPTE_BASE_MASK                 (~(PT_SIZE - 1))

#define IOPDE_INDEX(va)                 (((va) >> IOMMU_PD_SHIFT) & (NUM_ENTRIES_PDE - 1))
#define IOPTE_INDEX(va)                 (((va) >> IOMMU_PT_SHIFT) & (NUM_ENTRIES_PTE - 1))

#define IOPTE_BASE(ent)                 ((ent) & IOPTE_BASE_MASK)

#define SPAGE_SIZE                      (1 << IOMMU_PT_SHIFT)
#define SPD_SIZE                        (1 << IOMMU_PD_SHIFT)

/*
 * Page Directory Entry Control Bits
 */
#define DENT_VALID                      0x01
#define DENT_PTE_SHFIT                  10
#define DENT_WRITABLE                   (1UL << (3))
#define DENT_READABLE                   (1UL << (2))

/*
 * Page Table Entry Control Bits
 */
#define SUNXI_PTE_PAGE_WRITABLE       (1UL << (3))
#define SUNXI_PTE_PAGE_READABLE       (1UL << (2))
#define SUNXI_PTE_PAGE_VALID          (1UL << (1))

#define IS_VALID(x) (((x) & 0x03) == DENT_VALID)

#define IOVA_AREA_SIZE                (128 * 1024 * 1024)
#define IOVA_BASE                     (0x48000000)
#define IOVA_PGTABLE_NENT             (IOVA_AREA_SIZE / SPAGE_SIZE)

/*
 * IOMMU enable register field
 */
#define IOMMU_ENABLE                  0x1

struct iommu_static_map {
	unsigned long iova;
	unsigned long pa;
	unsigned long size;
	unsigned long pgtable;
};

static u32 iommu_pg_l1table = 0;
static u32 vmap_pgtable[IOVA_PGTABLE_NENT] __attribute__((aligned(1024))) = { 0 };

static inline void sunxi_iommu_write(unsigned long offset, unsigned long val)
{
	hal_writel(val, SUNXI_IOMMU_BASE + offset);
}

static inline uint32_t sunxi_iommu_read(unsigned long offset)
{
	return hal_readl(SUNXI_IOMMU_BASE + offset);
}

static int sunxi_tlb_flush(void)
{
	int us = 0;

	sunxi_iommu_write(IOMMU_TLB_FLUSH_ENABLE_REG, 0x0003007f);
	while (us < 2 * 1000) {
		if (!sunxi_iommu_read(IOMMU_TLB_FLUSH_ENABLE_REG))
			break;
		hal_udelay(2);
		us += 2;
	}

	if (sunxi_iommu_read(IOMMU_TLB_FLUSH_ENABLE_REG)) {
		printf("Enable flush all request timed out\n");
		return -1;
	}

	return 0;
}

static inline uint32_t *iopde_offset(unsigned long iova)
{
	uint32_t *iopd = (uint32_t *)iommu_pg_l1table;
	return iopd + IOPDE_INDEX(iova);
}

static inline uint32_t *iopte_offset(uint32_t *ent, unsigned long iova)
{
	unsigned long iopte_base = 0;

	iopte_base = (*ent) & IOPTE_BASE_MASK;

	return (uint32_t *)(iopte_base) + IOPTE_INDEX(iova);
}

static inline uint32_t sunxi_mk_pte(uint32_t page)
{
	uint32_t flags = 0;

	flags |= SUNXI_PTE_PAGE_READABLE;
	flags |= SUNXI_PTE_PAGE_WRITABLE;
	page &= IOMMU_PT_MASK;
	return page | flags | SUNXI_PTE_PAGE_VALID;
}

static int sunxi_tlb_invalid(unsigned long start, unsigned long end)
{
	uint32_t us = 0;

	sunxi_iommu_write(IOMMU_TLB_IVLD_START_ADDR_REG, start);
	sunxi_iommu_write(IOMMU_TLB_IVLD_END_ADDR_REG, end);
	sunxi_iommu_write(IOMMU_TLB_IVLD_ENABLE_REG, 0x1);

	while (us < 2 * 1000) {
		if (!sunxi_iommu_read(IOMMU_TLB_IVLD_ENABLE_REG))
			break;
		hal_udelay(2);
		us += 2;
	}

	if (sunxi_iommu_read(IOMMU_TLB_FLUSH_ENABLE_REG)) {
		printf("TLB cache invalid timed out\n");
		return -1;
	}
	return 0;
}

static int sunxi_ptw_cache_invalid(unsigned long start, unsigned long end)
{
	uint32_t us = 0;

	sunxi_iommu_write(IOMMU_PC_IVLD_START_ADDR_REG, start);
	sunxi_iommu_write(IOMMU_PC_IVLD_END_ADDR_REG, end);
	sunxi_iommu_write(IOMMU_PC_IVLD_ENABLE_REG, 0x1);

	while (us < 2 * 1000) {
		if (!sunxi_iommu_read(IOMMU_PC_IVLD_ENABLE_REG))
			break;
		hal_udelay(2);
		us += 2;
	}

	if (sunxi_iommu_read(IOMMU_PC_IVLD_ENABLE_REG)) {
		printf("PTW cache invalid timed out\n");
		return -1;
	}
	return 0;
}

static void sunxi_flush_tlb(unsigned long iova, unsigned long size)
{
	sunxi_tlb_invalid(iova, iova + size);
	sunxi_ptw_cache_invalid(iova, iova + size);
}

int iommu_map_page(unsigned long iova, unsigned long pa)
{
	unsigned long iova_start, paddr_start;
	uint32_t *dent, *pent;
	int i, j;

	iova_start = iova & IOMMU_PT_MASK;
	paddr_start = pa & IOMMU_PT_MASK;

	dent = iopde_offset(iova_start);
	if (!IS_VALID(*dent)) {
		printf("%s:%d: BUG: L1TABLE EMPTY\n", __func__, __LINE__);
		return -EFAULT;
	}

	pent = ((uint32_t *)IOPTE_BASE(*dent)) + IOPTE_INDEX(iova_start);
	*pent = sunxi_mk_pte(paddr_start);
	hal_dcache_clean((unsigned long)(dent), PT_SIZE);

	return 0;
}

static int _iommu_map_region(unsigned long iova, unsigned long pa, unsigned long size)
{
	unsigned long iova_start, iova_end, paddr_start;
	uint32_t *dent, *pent;
	int i, j;
	int l2table_offset = 0;

	iova_start = iova & IOMMU_PT_MASK;
	iova_end = (iova + size) & IOMMU_PT_MASK;

	paddr_start = pa & IOMMU_PT_MASK;

	while (iova_start < iova_end) {
		dent = iopde_offset(iova_start);
		if (!IS_VALID(*dent)) {
			printf("%s:%d: BUG: L1TABLE EMPTY\n", __func__, __LINE__);
			return -EFAULT;
		}

		if (IOPDE_INDEX(iova_start) == IOPDE_INDEX(iova_end))
			j = IOPTE_INDEX(iova_end);
		else
			j = NUM_ENTRIES_PTE;

		for (i = IOPTE_INDEX(iova_start); i < j; i++) {
			pent = ((uint32_t *)IOPTE_BASE(*dent)) + i;
			*pent = sunxi_mk_pte(paddr_start);
			iova_start += SPAGE_SIZE;
			paddr_start += SPAGE_SIZE;
		}
		hal_dcache_clean((unsigned long)(dent), PT_SIZE);
	}

	return 0;
}

static int iommu_unmap_page(unsigned long iova, unsigned long pa)
{
	unsigned long iova_start, paddr_start;
	uint32_t *dent, *pent;
	int i, j;

	iova_start = iova & IOMMU_PT_MASK;
	paddr_start = pa & IOMMU_PT_MASK;

	dent = iopde_offset(iova_start);
	if (!IS_VALID(*dent)) {
		printf("%s:%d: BUG: L1TABLE EMPTY\n", __func__, __LINE__);
		return -EFAULT;
	}

	pent = ((uint32_t *)IOPTE_BASE(*dent)) + IOPTE_INDEX(iova_start);
	*pent = 0;
	hal_dcache_clean((unsigned long)(dent), PT_SIZE);

	return 0;
}

static int _iommu_unmap_region(unsigned long iova, unsigned long size)
{
	unsigned long iova_start, iova_end;
	uint32_t *dent, *pent;
	int i, j;

	iova_start = iova & IOMMU_PT_MASK;
	iova_end = (iova + size) & IOMMU_PT_MASK;

	while (iova_start < iova_end) {
		dent = iopde_offset(iova_start);
		if (!IS_VALID(*dent)) {
			printf("%s:%d: BUG: L1TABLE EMPTY\n", __func__, __LINE__);
			return -EFAULT;
		}

		if (IOPDE_INDEX(iova_start) == IOPDE_INDEX(iova_end))
			j = IOPTE_INDEX(iova_end);
		else
			j = NUM_ENTRIES_PTE;

		for (i = IOPTE_INDEX(iova_start); i < j; i++) {
			pent = ((uint32_t *)IOPTE_BASE(*dent)) + i;
			*pent = 0;
			iova_start += SPAGE_SIZE;
		}
		hal_dcache_clean((unsigned long)(dent), PT_SIZE);
	}

	return 0;
}


int simple_iommu_map_init(void)
{
	unsigned long iova_start, iova_end;
	uint32_t *dent;
	int l2table_offset = 0;

	iommu_pg_l1table = sunxi_iommu_read(IOMMU_TTB_REG);

	iova_start = IOVA_BASE & IOMMU_PT_MASK;
	iova_end = (IOVA_BASE + IOVA_AREA_SIZE) & IOMMU_PT_MASK;

	while (iova_start < iova_end) {
		dent = iopde_offset(iova_start);
		*dent = ((uint32_t)vmap_pgtable + l2table_offset) | DENT_VALID;
		l2table_offset += PT_SIZE;
		iova_start += SPD_SIZE;
	}

	hal_dcache_clean(iommu_pg_l1table, PD_SIZE);

	return 0;
}

int simple_iommu_map_region(unsigned long iova, unsigned long pa, unsigned long size)
{
	int ret;

	if (iova < IOVA_BASE || (iova + size) > (IOVA_BASE + IOVA_AREA_SIZE)) {
		printf("iova range should in [0x%08x, 0x%08x)\n", IOVA_BASE, IOVA_BASE + IOVA_AREA_SIZE);
		return -ENODEV;
	}

	hal_enter_critical();
	ret = _iommu_map_region(iova, pa, size);
	sunxi_flush_tlb(iova, size);
	hal_exit_critical();

	return ret;
}

int simple_iommu_unmap_region(unsigned long iova, unsigned long size)
{
	int ret;

	if (iova < IOVA_BASE || (iova + size) > (IOVA_BASE + IOVA_AREA_SIZE)) {
		printf("iova range should in [0x%08x, 0x%08x)\n", IOVA_BASE, IOVA_BASE + IOVA_AREA_SIZE);
		return -ENODEV;
	}

	hal_enter_critical();
	ret = _iommu_unmap_region(iova, size);
	sunxi_flush_tlb(iova, size);
	hal_exit_critical();

	return ret;
}

/*
 * only for sun8iw21 npu driver
 * splicing rule:
 * 		input:
 * 			buf1: 1 2 3 4 5
 * 			buf2: a b c d e
 * 		out
 * 			buf3: 1 a 2 b 3 c 4 d 5 e
 */
int simple_iommu_splice_npu_buffer(unsigned long iova, void *buf1, void *buf2,
				unsigned long size, unsigned long batch)
{
	unsigned long iova_end;
	uint32_t pa1, pa2;
	uint32_t *dent, *pent;
	int i, j;
	int mapped = 0;
	int idx = 0;

	pa1 = (uint32_t)buf1;
	pa2 = (uint32_t)buf2;

	if (iova < IOVA_BASE || (iova + size) > (IOVA_BASE + IOVA_AREA_SIZE)) {
		printf("iova range should in [0x%08x, 0x%08x)\n", IOVA_BASE, IOVA_BASE + IOVA_AREA_SIZE);
		return -EINVAL;
	}

	if ((pa1 & (SPAGE_SIZE - 1)) || (pa2 & (SPAGE_SIZE - 1))) {
		printf("buf should align to 0x%08x, buf1=%p, buf2=%p\n", SPAGE_SIZE, buf1, buf2);
		return -EINVAL;
	}

	if ((iova & (SPAGE_SIZE - 1)) || (size & (SPAGE_SIZE - 1))) {
		printf("iova should align to 0x%08x, iova=%08lx, size=%08lx\n", SPAGE_SIZE, iova, size);
		return -EINVAL;
	}

	if (batch & (SPAGE_SIZE - 1)) {
		printf("batch should align to 0x%08x, batch=%08lx\n", SPAGE_SIZE, batch);
		return -EINVAL;
	}

	iova_end = iova + size;

	hal_enter_critical();

	while (iova < iova_end) {
		dent = iopde_offset(iova);
		if (!IS_VALID(*dent)) {
			printf("%s:%d: BUG: L1TABLE EMPTY\n", __func__, __LINE__);
			hal_exit_critical();
			return -EFAULT;
		}

		if (IOPDE_INDEX(iova) == IOPDE_INDEX(iova_end))
			j = IOPTE_INDEX(iova_end);
		else
			j = NUM_ENTRIES_PTE;

		for (i = IOPTE_INDEX(iova); i < j; i++) {
			pent = ((uint32_t *)IOPTE_BASE(*dent)) + i;
			if (!(idx & 0x1)) {
				*pent = sunxi_mk_pte(pa1);
				pa1 += SPAGE_SIZE;
			} else {
				*pent = sunxi_mk_pte(pa2);
				pa2 += SPAGE_SIZE;
			}
			mapped += SPAGE_SIZE;
			if (mapped == batch) {
				mapped = 0;
				idx++;
			}
			iova += SPAGE_SIZE;
		}
		hal_dcache_clean((unsigned long)(dent), PT_SIZE);
	}

	sunxi_flush_tlb(iova, size);

	hal_exit_critical();

	return 0;
}


#include <hal_cmd.h>
#include <string.h>
static int cmd_test_splice_buf(int argc, const char **argv)
{
#define DUMP_SIZE				32
#define MAP_SIZE				(16 * 1024 * 4)
#define BATCH_SIZE				(16 * 1024)

	uint8_t *buf1, *buf2, *buf3;
	int i;

	buf1 = (uint8_t *)(CONFIG_DRAM_PHYBASE);
	buf2 = (uint8_t *)(CONFIG_DRAM_PHYBASE + MAP_SIZE);
	buf3 = (uint8_t *)0x40000000;

	printf("buf1: %p\n", buf1);
	printf("buf2: %p\n", buf2);
	printf("buf3: %p\n", buf3);

	simple_iommu_splice_npu_buffer((unsigned long)buf3, buf1, buf2, MAP_SIZE * 2, BATCH_SIZE);
	hal_dcache_invalidate((unsigned long)buf3, MAP_SIZE);

	for (i = 0; i < MAP_SIZE; i+=BATCH_SIZE) {
		printf("Check buf3 <-> buf1 %p <-> %p\n", &buf3[2 * i], &buf1[i]);
		if (memcmp(&buf3[2 * i], &buf1[i], BATCH_SIZE)) {
			printf("Check buf1 data failed\n");
			printf("buf1 %p:0x%08lx\n", &buf1[i], *(uint32_t *)(&buf1[i]));
			printf("buf2 %p:0x%08lx\n", &buf2[i], *(uint32_t *)(&buf2[i]));
			printf("buf3 %p:0x%08lx\n", &buf3[2 * i], *(uint32_t *)(&buf3[2 * i]));
			return 0;
		}

		printf("Check buf3 <-> buf2 %p <-> %p\n", &buf3[2 * i + BATCH_SIZE], &buf2[i]);
		if (memcmp(&buf3[2 * i + BATCH_SIZE], &buf2[i], BATCH_SIZE)) {
			printf("Check buf2 data failed\n");
			printf("buf1 %p:0x%08lx\n", &buf1[i], *(uint32_t *)(&buf1[i]));
			printf("buf2 %p:0x%08lx\n", &buf2[i], *(uint32_t *)(&buf2[i]));
			printf("buf3 %p:0x%08lx\n", &buf3[2 * i + BATCH_SIZE], *(uint32_t *)(&buf3[2 * i + BATCH_SIZE]));
			return 0;
		}
	}

	printf("Map Success\r\n");

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(cmd_test_splice_buf, test_splice_buf, test func simple_iommu_splice_npu_buffer);
