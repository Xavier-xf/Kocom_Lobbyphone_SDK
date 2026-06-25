#include <vip_lite.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <sys/time.h>
#endif

#include <hal_mem.h>
#include <hal_time.h>
#include "network_binary_softmax.h"
#include "input_conv.h"


/*
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <hal_interrupt.h>

#define get_wvalue(addr)	(*((volatile unsigned long  *)(addr)))
#define put_wvalue(addr, v)	(*((volatile unsigned long  *)(addr)) = (unsigned long)(v))

#define NPU_BASE             0x03050000
#define NPU_BASE_ADDR		 NPU_BASE
#define NPU_CLK_CTRL_REG     (NPU_BASE_ADDR + 0x000)
#define NPU_INT_ACK_REG		 (NPU_BASE_ADDR + 0x010)
#define NPU_IDLE_STS_REG	 (NPU_BASE_ADDR + 0x004)
#define NPU_INT_EN_REG		 (NPU_BASE_ADDR + 0x014)
#define NPU_PWR_CTRL_REG     (NPU_BASE_ADDR + 0x100)
#define NPU_CMD_ADDR_REG     (NPU_BASE_ADDR + 0x654)

#define NPU_DMA_HIGH         (NPU_BASE_ADDR + 0x66C)
#define NPU_DMA_LOW          (NPU_BASE_ADDR + 0x668)

#define NPU_CMD_SIZE_REG	 (NPU_BASE_ADDR + 0x3A4)
#define NPU_HOST_IF_CTRL_REG (NPU_BASE_ADDR + 0x3A8)
#define GIC_SRC_NPU          81

#define CCMU_BASE                           (0x02001000)
#define CCMU_NPU_CLK_REG					(CCMU_BASE + 0x06E0)
#define CCMU_NPU_BGR_REG					(CCMU_BASE + 0x06EC)
#define CCMU_PLL_NPU_CTRL_REG				(CCMU_BASE + 0x0080)
*/

#define CREATE_NETWORK_FROM_MEMORY  1
//#define CREATE_NETWORK_FROM_FLASH   1
//#define CREATE_NETWORK_FROM_FILE    1

#define MAX_DIMENSION_NUMBER    4
#define MAX_INPUT_OUTPUT_NUM    20
#define MATH_ABS(x)      (((x) < 0)    ? -(x) :  (x))
#define MATH_MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define MATH_MIN(a,b)    (((a) < (b)) ? (a) : (b))

/* #define MAX_SUPPORT_RUN_NETWORK   128
void *network_buffer[MAX_SUPPORT_RUN_NETWORK] = {VIP_NULL}; */

#define BITS_PER_BYTE 8

const char *usage =
    "vpm_run -s sample.txt -l loop_run_count -d device_id\n"
    "-s sample.txt:     to include one ore more network binary graph (NBG) data file resource.\n"
    "                   See sample.txt for details.\n"
    "-l loop_run_count: the number of loop run network.\n"
    "-d device_id:      specify this NBG runs device.\n"
    "-t time_out:       specify time out of network.\n"
    "-h : help\n"
    "example: ./vpm_run -s sample.txt -l 10 -d 1 specify the NBG runs 10 times on device 1.\n";

typedef struct _vpm_network_task {
    /* task information. */
    char          **base_strings;
    int             string_count;
    int             nbg_name;
    int             input_count;
    int            *input_names;
    int             output_count;
    int            *output_names;
    int             golden_count;
    int            *golden_names;
    void           **golden_data;
    vip_uint32_t   *golden_size;

    /* VIP lite buffer objects. */
    vip_network     network;
    vip_buffer     *input_buffers;
    vip_buffer     *output_buffers;

    vip_uint32_t   loop_count;
    vip_uint32_t   infer_cycle;
    vip_uint32_t   infer_time;
    vip_uint64_t   total_infer_cycle;
    vip_uint64_t   total_infer_time;
} vpm_network_task_t;

typedef enum _file_type_e
{
    NN_FILE_NONE,
    NN_FILE_TENSOR,
    NN_FILE_BINARY,
    NN_FILE_TEXT
} file_type_e;


/*
int npuirq_flag = 0;

void npu_ccm_module_enable(void)
{
	// set ahb bus reset
	put_wvalue(CCMU_NPU_BGR_REG, 0x10001);
}

void npu_ccm_module_disable(void)
{
	// clear ahb bus reset
	put_wvalue(CCMU_NPU_BGR_REG, 0x0);
}

void npu_cfg_clk(void)
{
    // set npu clk to 504MHz, Set NPU CLK Parent.
    put_wvalue(CCMU_PLL_NPU_CTRL_REG, 0xc8002003);
    put_wvalue(CCMU_NPU_CLK_REG, 0x83000000);
}

void npu_sys_open(void)
{
	int npu_clk; //M
    npu_ccm_module_disable();
    npu_ccm_module_enable();
	printf("CCMU_AIPU_CLK_REG successful\n");
	npu_cfg_clk();
}

static hal_irqreturn_t npu_handler(void *data)
{
	int val;
	val = get_wvalue(NPU_INT_ACK_REG);
	npuirq_flag = 1;
    return 0;
}

int npu_test(int argc, const char **argv)
{
	int i = 0, j = 0, ret = 0, soc_irq = 0, cmd_buffer = 0, npu_state = 0;

    // NPU Register IRQ
    ret = hal_request_irq(GIC_SRC_NPU, npu_handler, "npu", NULL);
    printf("ret = %d\n", ret);
    if (ret < 0)
        printf("vipcore, request_irq failed line=%d\n", GIC_SRC_NPU);
    else if (ret >= 0)
        printf("vipcore, request_irq success line=%d\n", GIC_SRC_NPU);
    hal_enable_irq(GIC_SRC_NPU);
    soc_irq = get_wvalue(0x30801144);
    printf("REG:****RISCV soc_irq = 0x%x******\n", soc_irq);

	// NPU SYS SOURCE
	npu_sys_open();

    npu_state = get_wvalue(CCMU_NPU_CLK_REG);
    printf("REG:****npu clk reg = 0x%x , npu_state = 0x%x******\n", CCMU_NPU_CLK_REG, npu_state);
    npu_state = get_wvalue(CCMU_NPU_BGR_REG);
    printf("REG:****npu bgr reg = 0x%x , npu_state = 0x%x******\n", CCMU_NPU_BGR_REG, npu_state);
    npu_state = get_wvalue(CCMU_PLL_NPU_CTRL_REG);
    printf("REG:****npu clk ctrl reg = 0x%x , npu_state = 0x%x******\n", CCMU_PLL_NPU_CTRL_REG, npu_state);
    npu_state = get_wvalue(NPU_BASE);
    printf("REG:****npu base reg = 0x%x , npu_state = 0x%x******\n", NPU_BASE, npu_state);
    npu_state = get_wvalue(NPU_BASE + 0x20);
    printf("REG:****npu vip reg = 0x%x , npu_state = 0x%x******\n", NPU_BASE + 0x20, npu_state);

    for (j = 0; j < 320; j++) {
        put_wvalue(0x42f40000 + j * 0x04, 0x0);
        // cmd_buffer = get_wvalue(0x44000000 + j * 0x04);
        // printf("REG:****physical = 0x%x , cmd_buffer = 0x%x******\n", 0x44000000 + (0x4 * j), cmd_buffer);
    }

    // Enable NPU IRQ
	put_wvalue(NPU_INT_EN_REG, 0xffffffff);
	put_wvalue(NPU_PWR_CTRL_REG, 0X140021);


    npu_state = get_wvalue(NPU_DMA_LOW);
    printf("REG:****NPU_DMA_LOW = 0x%x , npu_state = 0x%x******\n", NPU_DMA_LOW, npu_state);
    npu_state = get_wvalue(NPU_DMA_HIGH);
    printf("REG:****NPU_DMA_HIGH = 0x%x , npu_state = 0x%x******\n", NPU_DMA_HIGH, npu_state);
    npu_state = get_wvalue(NPU_IDLE_STS_REG);
    printf("REG:****NPU_IDLE_STS_REG = 0x%x , npu_state = 0x%x******\n", NPU_IDLE_STS_REG, npu_state);

    // Start NPU IRQ
    printf("\n");
    printf("*********Start run network!**************\n");
	put_wvalue(NPU_CMD_ADDR_REG, 0x42f00000);
	put_wvalue(NPU_CMD_SIZE_REG, 0x1ffff);
    printf("\n");

    npu_state = get_wvalue(NPU_DMA_LOW);
    printf("REG:****NPU_DMA_LOW = 0x%x , npu_state = 0x%x******\n", NPU_DMA_LOW, npu_state);
    npu_state = get_wvalue(NPU_DMA_HIGH);
    printf("REG:****NPU_DMA_HIGH = 0x%x , npu_state = 0x%x******\n", NPU_DMA_HIGH, npu_state);
    npu_state = get_wvalue(NPU_IDLE_STS_REG);
    printf("REG:****NPU_IDLE_STS_REG = 0x%x , npu_state = 0x%x******\n", NPU_IDLE_STS_REG, npu_state);

    while (!npuirq_flag) {
        hal_msleep(6000);
        printf("NPU IRQ Don`t get!\n");
        break;
    }
    npu_state = get_wvalue(NPU_DMA_LOW);
    printf("REG:****NPU_DMA_LOW = 0x%x , npu_state = 0x%x******\n", NPU_DMA_LOW, npu_state);
    npu_state = get_wvalue(NPU_DMA_HIGH);
    printf("REG:****NPU_DMA_HIGH = 0x%x , npu_state = 0x%x******\n", NPU_DMA_HIGH, npu_state);
    npu_state = get_wvalue(NPU_IDLE_STS_REG);
    printf("REG:****NPU_IDLE_STS_REG = 0x%x , npu_state = 0x%x******\n", NPU_IDLE_STS_REG, npu_state);
    printf("NPU IRQ Flag = %d\n", npuirq_flag);

    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(npu_test, npu_test, user defined cmd);
*/


#if defined(__linux__)
#define TIME_SLOTS   10
vip_uint64_t time_begin[TIME_SLOTS];
vip_uint64_t time_end[TIME_SLOTS];
static vip_uint64_t GetTime(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (vip_uint64_t)(time.tv_usec + time.tv_sec * 1000000);
}

static void TimeBegin(int id)
{
    time_begin[id] = GetTime();
}

static void TimeEnd(int id)
{
    time_end[id] = GetTime();
}

static vip_uint64_t TimeGet(int id)
{
    return time_end[id] - time_begin[id];
}
#endif

static vip_status_e vip_memset(vip_uint8_t *dst, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
#if 0
    vip_uint32_t i = 0;
    for (i = 0; i < size; i++) {
        dst[i] = 0;
    }
#else
    memset(dst, 0, size);
#endif
    return status;
}

static vip_status_e vip_memcpy(vip_uint8_t *dst, vip_uint8_t *src, vip_uint32_t size)
{
    vip_status_e status = VIP_SUCCESS;
#if 0
    vip_uint32_t i = 0;
    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
#else
    memcpy(dst, src, size);
#endif
    return status;
}

typedef struct
{
    vip_uint8_t* raw_addr;
} aligned_header;

static vip_uint8_t * vsi_nn_MallocAlignedBuffer
    (
    vip_uint32_t mem_size,
    vip_uint32_t align_start_size,
    vip_uint32_t align_block_size
    )
{
    vip_uint32_t sz;
    long temp;
    vip_uint8_t* raw_addr;
    vip_uint8_t* p;
    vip_uint8_t* align_addr;
    aligned_header* header;

    sz = sizeof(aligned_header) + mem_size + align_start_size + align_block_size;
    /* raw_addr = (vip_uint8_t *)malloc(sz * sizeof(vip_uint8_t ) ); */
    raw_addr = (vip_uint8_t *)hal_malloc(sz * sizeof(vip_uint8_t ) );
    memset(raw_addr, 0, sizeof(vip_uint8_t ) * sz);
    p = raw_addr + sizeof(aligned_header);

    temp = (long)(p) % align_start_size;
    if (temp == 0)
    {
        align_addr = p;
    }
    else
    {
        align_addr = p + align_start_size - temp;
    }
    header = (aligned_header*)(align_addr - sizeof(aligned_header));
    header->raw_addr = raw_addr;

    return align_addr;
}/* vsi_nn_MallocAlignedBuffer() */

static void vsi_nn_FreeAlignedBuffer
    (
    vip_uint8_t* handle
    )
{
    aligned_header* header;
    header = (aligned_header*)(handle - sizeof(aligned_header));
    /* free(header->raw_addr); */
    hal_free(header->raw_addr);
}

static unsigned int load_file(const char *name, void *dst)
{
    FILE *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fseek(fp, 0, SEEK_SET);
        size = fread(dst, size, 1, fp);

        fclose(fp);
    }

    return size;
}

static unsigned int save_file(const char *name, void *data, unsigned int size)
{
    FILE *fp = fopen(name, "wb+");
    unsigned int saved = 0;

    if (fp != NULL) {
        saved = fwrite(data, size, 1, fp);

        fclose(fp);
    }
    else {
        printf("Saving file %s failed.\n", name);
    }

    return saved;
}

static unsigned int get_file_size(const char *name)
{
    FILE    *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fclose(fp);
    }
    else {
        printf("Checking file %s failed.\n", name);
    }

    return size;
}

static int get_file_type(const char *file_name)
{
    int type = 0;
    const char *ptr;
    char sep = '.';
    unsigned int pos,n;
    char buff[32] = {0};

    ptr = strrchr(file_name, sep);
    pos = ptr - file_name;
    n = strlen(file_name) - (pos + 1);
    strncpy(buff, file_name+(pos+1), n);

    if (strcmp(buff, "tensor") == 0) {
        type = NN_FILE_TENSOR;
    }
    else if(strcmp(buff, "dat") == 0 || !strcmp(buff, "bin"))
    {
        type = NN_FILE_BINARY;
    }
    else if(strcmp(buff, "txt") == 0)
    {
        type = NN_FILE_TEXT;
    }
    else {
        printf("unsupported input file type=%s.\n", buff);
    }

    return type;
}

static vip_uint32_t type_get_bytes(const vip_enum type)
{
    switch(type)
    {
        case VIP_BUFFER_FORMAT_INT8:
        case VIP_BUFFER_FORMAT_UINT8:
            return 1;
        case VIP_BUFFER_FORMAT_INT16:
        case VIP_BUFFER_FORMAT_UINT16:
        case VIP_BUFFER_FORMAT_FP16:
        case VIP_BUFFER_FORMAT_BFP16:
            return 2;
        case VIP_BUFFER_FORMAT_FP32:
        case VIP_BUFFER_FORMAT_INT32:
        case VIP_BUFFER_FORMAT_UINT32:
            return 4;
        case VIP_BUFFER_FORMAT_FP64:
        case VIP_BUFFER_FORMAT_INT64:
        case VIP_BUFFER_FORMAT_UINT64:
            return 8;
        case VIP_BUFFER_FORMAT_INT4:
        case VIP_BUFFER_FORMAT_UINT4:
            return 1;

        default:
            return 0;
    }
}

static vip_uint32_t type_get_bits(const vip_enum type)
{
    switch(type)
    {
        case VIP_BUFFER_FORMAT_INT8:
        case VIP_BUFFER_FORMAT_UINT8:
            return 1 * BITS_PER_BYTE;
        case VIP_BUFFER_FORMAT_INT16:
        case VIP_BUFFER_FORMAT_UINT16:
        case VIP_BUFFER_FORMAT_FP16:
        case VIP_BUFFER_FORMAT_BFP16:
            return 2 * BITS_PER_BYTE;
        case VIP_BUFFER_FORMAT_FP32:
        case VIP_BUFFER_FORMAT_INT32:
        case VIP_BUFFER_FORMAT_UINT32:
            return 4 * BITS_PER_BYTE;
        case VIP_BUFFER_FORMAT_FP64:
        case VIP_BUFFER_FORMAT_INT64:
        case VIP_BUFFER_FORMAT_UINT64:
            return 8 * BITS_PER_BYTE;
        case VIP_BUFFER_FORMAT_INT4:
        case VIP_BUFFER_FORMAT_UINT4:
            return BITS_PER_BYTE / 2;

        default:
            return 0;
    }
}

static vip_uint32_t get_tensor_size(
    vip_int32_t *shape,
    vip_uint32_t dim_num,
    vip_enum type
    )
{
    vip_uint32_t sz;
    vip_uint32_t i;
    sz = 0;
    if(NULL == shape || 0 == dim_num)
    {
        return sz;
    }
    sz = type_get_bits(type);
    for(i = 0; i < dim_num; i ++)
    {
        sz *= shape[i];
        if (0 == i) {
            /* round up */
            sz = (sz + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
        }
    }

    return sz;
}

static vip_uint32_t get_element_num(
    vip_int32_t *sizes,
    vip_uint32_t num_of_dims,
    vip_enum data_format
    )
{
    vip_uint32_t num = 1;
    vip_uint32_t i = 0;

    for (i = 0; i < num_of_dims; i++)
    {
        num *= sizes[i];
    }

    return num;
}

static vip_int32_t type_is_integer(const vip_enum type)
{
    vip_int32_t ret;
    ret = 0;
    switch(type)
    {
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_UINT8:
    case VIP_BUFFER_FORMAT_UINT16:
    case VIP_BUFFER_FORMAT_UINT32:
    case VIP_BUFFER_FORMAT_UINT4:
    case VIP_BUFFER_FORMAT_INT4:
        ret = 1;
        break;
    default:
        break;
    }

    return ret;
}

static vip_int32_t type_is_signed(const vip_enum type)
{
    vip_int32_t ret;
    ret = 0;
    switch(type)
    {
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_BFP16:
    case VIP_BUFFER_FORMAT_FP16:
    case VIP_BUFFER_FORMAT_FP32:
        ret = 1;
        break;
    default:
        break;
    }

    return ret;
}

static void type_get_range(vip_enum type, double *max_range, double * min_range)
{
    vip_int32_t bits;
    double from, to;
    from = 0.0;
    to = 0.0;
    bits = type_get_bits(type);
    if(type_is_integer(type)) {
        if(type_is_signed(type)) {
            from = (double)(-(1L << (bits - 1)));
            to = (double)((1UL << (bits - 1)) - 1);
        }
        else {
            from = 0.0;
            to = (double)((1UL << bits) - 1);
        }
    }
    else {
        //  TODO: Add float
    }
    if(NULL != max_range) {
        *max_range = to;
    }
    if(NULL != min_range) {
        *min_range = from;
    }
}

static double copy_sign(double number, double sign)
{
    double value = MATH_ABS(number);
    return (sign > 0) ? value : (-value);
}

static int math_floorf(double x)
{
    if (x >= 0)
    {
        return (int)x;
    }
    else
    {
        return (int)x - 1;
    }
}

static double rint(double x)
{
#define _EPSILON 1e-8
    double decimal;
    double inter;
    int intpart;

    intpart = (int)x;
    decimal = x - intpart;
    inter = (double)intpart;

    if(MATH_ABS((MATH_ABS(decimal) - 0.5f)) < _EPSILON )
    {
        inter += (vip_int32_t)(inter) % 2;
    }
    else
    {
        return copy_sign(math_floorf(MATH_ABS(x) + 0.5f), x);
    }

    return inter;
}

static vip_int32_t fp32_to_dfp(const float in,  const signed char fl, const vip_enum type)
{
    vip_int32_t data;
    double max_range;
    double min_range;
    type_get_range(type, &max_range, &min_range);
    if(fl > 0 )
    {
        data = (vip_int32_t)rint(in * (float)(1 << fl ));
    }
    else
    {
        data = (vip_int32_t)rint(in * (1.0f / (float)(1 << -fl )));
    }
    data = MATH_MIN(data, (vip_int32_t)max_range);
    data = MATH_MAX(data, (vip_int32_t)min_range);

    return data;
}

static vip_int32_t fp32_to_affine(
    const float in,
    const float scale,
    const  int zero_point,
    const vip_enum type
    )
{
    vip_int32_t data;
    double max_range;
    double min_range;
    type_get_range(type, &max_range, &min_range);
    data = (vip_int32_t)(rint(in / scale ) + zero_point);
    data = MATH_MAX((vip_int32_t)min_range, MATH_MIN((vip_int32_t)max_range , data ));
    return data;
}

static vip_status_e integer_convert(
    const void * src,
    void *dest,
    vip_enum src_dtype,
    vip_enum dst_dtype
    )
{
    vip_status_e status = VIP_SUCCESS;

        unsigned char all_zeros[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
        unsigned char all_ones[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
        vip_uint32_t src_sz = type_get_bytes(src_dtype);
        vip_uint32_t dest_sz = type_get_bytes(dst_dtype);
        unsigned char* buffer = all_zeros;

        if (VIP_BUFFER_FORMAT_INT4 == src_dtype || VIP_BUFFER_FORMAT_INT8 == src_dtype ||
            VIP_BUFFER_FORMAT_INT16 == src_dtype || VIP_BUFFER_FORMAT_INT32 == src_dtype ||
            VIP_BUFFER_FORMAT_INT64 == src_dtype)
        {
            vip_int8_t mask = 0x80;
            if (VIP_BUFFER_FORMAT_INT4 == src_dtype) {
                mask = 0x8;
            }
            if (((vip_int8_t*)src)[src_sz - 1] & mask)
            {
                buffer = all_ones;
            }
        }

        memcpy(buffer, src, src_sz);
        if (VIP_BUFFER_FORMAT_INT4 == dst_dtype || VIP_BUFFER_FORMAT_UINT4 == dst_dtype) {
            buffer[0] &= 0x0f;
        }
        memcpy(dest, buffer, dest_sz);

    return status;
}

static unsigned short  fp32_to_bfp16_rtne(float in)
{
    /*
    Convert a float point to bfloat16, with round-nearest-to-even as rounding method.
    */
    vip_uint32_t fp32 = *((unsigned int *) &in);
    unsigned short  out;

    vip_uint32_t lsb = (fp32 >> 16) & 1;    /* Least significant bit of resulting bfloat. */
    vip_uint32_t rounding_bias = 0x7fff + lsb;

    if (0x7FC00000 == in ) {
        out = 0x7fc0;
    }
    else {
        fp32 += rounding_bias;
        out = (unsigned short ) (fp32 >> 16);
    }

    return out;
}

static unsigned short fp32_to_fp16(float in)
{
    vip_uint32_t fp32 = 0;
    vip_uint32_t t1 = 0;
    vip_uint32_t t2 = 0;
    vip_uint32_t t3 = 0;
    vip_uint32_t fp16 = 0u;

    vip_memcpy((vip_uint8_t*)&fp32, (vip_uint8_t*)&in, sizeof(vip_uint32_t));

    t1 = (fp32 & 0x80000000u) >> 16;  /* sign bit. */
    t2 = (fp32 & 0x7F800000u) >> 13;  /* Exponent bits */
    t3 = (fp32 & 0x007FE000u) >> 13;  /* Mantissa bits, no rounding */

    if(t2 >= 0x023c00u )
    {
        fp16 = t1 | 0x7BFF;     /* Don't round to infinity. */
    }
    else if(t2 <= 0x01c000u )
    {
        fp16 = t1;
    }
    else
    {
        t2 -= 0x01c000u;
        fp16 = t1 | t2 | t3;
    }

    return (unsigned short) fp16;
}

static vip_status_e float32_to_dtype(
    float src,
    unsigned char *dst,
    const vip_enum data_type,
    const vip_enum quant_format,
    signed char fixed_point_pos,
    float tf_scale,
    vip_int32_t tf_zerop
    )
{
    vip_status_e status = VIP_SUCCESS;

    switch(data_type )
    {
    case VIP_BUFFER_FORMAT_FP32:
        *(float *)dst = src;
        break;
    case VIP_BUFFER_FORMAT_FP16:
        *(vip_int16_t *)dst = fp32_to_fp16(src);
        break;
    case VIP_BUFFER_FORMAT_BFP16:
        *(vip_int16_t *)dst = fp32_to_bfp16_rtne(src);
        break;
    case VIP_BUFFER_FORMAT_INT8:
    case VIP_BUFFER_FORMAT_UINT8:
    case VIP_BUFFER_FORMAT_INT16:
    case VIP_BUFFER_FORMAT_INT32:
    case VIP_BUFFER_FORMAT_INT4:
    case VIP_BUFFER_FORMAT_UINT4:
        {
            vip_int32_t dst_value = 0;
            switch(quant_format)
            {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                dst_value = fp32_to_dfp(src, fixed_point_pos, data_type);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                dst_value = fp32_to_affine(src, tf_scale, tf_zerop, data_type);
                break;
            case VIP_BUFFER_QUANTIZE_NONE:
                dst_value = (vip_int32_t)src;
                break;
            default:
                break;
            }
            integer_convert(&dst_value, dst, VIP_BUFFER_FORMAT_INT32, data_type);
        }
        break;
    default:
        printf("unsupported tensor type\n");;
    }

    return status;
}

unsigned char *get_binary_data(
    char *file_name,
    vip_uint32_t *file_size
    )
{
    unsigned char *tensorData;

    *file_size = get_file_size((const char *)file_name);
    /* tensorData = (unsigned char *)malloc(*file_size * sizeof(unsigned char)); */
    tensorData = (unsigned char *)hal_malloc(*file_size * sizeof(unsigned char));
    load_file(file_name, (void *)tensorData);

    return tensorData;
}

static vip_bool_e compare_low_4bits(
    vip_uint8_t* src0,
    vip_uint8_t* src1
    )
{
    vip_bool_e is_equal = vip_false_e;
    vip_uint8_t src0_low = 0, src1_low = 0;

    src0_low = src0[0] & 0x0F;
    src1_low = src1[0] & 0x0F;

    if (src0_low == src1_low) {
        is_equal = vip_true_e;
    }

    return is_equal;
}

vip_status_e pack_4bit_data(
    vip_uint8_t* src,
    vip_uint8_t* dest,
    vip_uint32_t size,
    vip_uint32_t x_dim
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t i = 0, j = 0;
    vip_uint8_t high = 0, low = 0;
    for (i = 0; i < size; i++)
    {
        if ((i + 1) % x_dim == 0)
        {
            high = 0;
            low = src[i];
        }
        else
        {
            high = src[i + 1];
            low = src[i];
            i++;
        }
        dest[j] = (high << 4) | (low & 0xF);
        j++;
    }

    return status;
}

vip_status_e unpack_4bit_data(
    vip_uint8_t* src,
    vip_uint8_t* dest,
    vip_uint32_t size,
    vip_uint32_t x_dim,
    vip_bool_e sign
    )
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint32_t i = 0, j = 0;
    vip_uint8_t high = 0, low = 0;
    for (i = 0; i < size; i++)
    {
        high = src[i] >> 4;
        low = src[i] & 0x0F;
        if (vip_true_e == sign) {
            if (high > 7)
            {
                high = high | 0xF0;
            }
            if (low > 7)
            {
                low = low | 0xF0;
            }
        }

        if ((j + 1) % x_dim == 0)
        {
            dest[j] = low;
            j++;
        }
        else
        {
            dest[j] = low;
            dest[j + 1] = high;
            j += 2;
        }
    }

    return status;
}

unsigned char *get_tensor_data(
    vpm_network_task_t *task,
    char *file_name,
    vip_uint32_t *file_size,
    vip_uint32_t index
    )
{
    vip_uint32_t sz = 1;
    vip_uint32_t stride = 1;
    vip_int32_t sizes[4];
    vip_uint32_t num_of_dims;
    vip_uint32_t i = 0;
    vip_enum data_format;
    vip_enum quant_format;
    vip_int32_t fixed_point_pos;
    float tf_scale;
    vip_int32_t tf_zerop;
    unsigned char *tensorData = NULL;
    FILE *tensorFile;
    float fval = 0.0;

    tensorFile = fopen(file_name, "rb");

    vip_query_input(task->network, index, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &num_of_dims);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_DATA_FORMAT, &data_format);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_QUANT_FORMAT, &quant_format);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_FIXED_POINT_POS, &fixed_point_pos);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_TF_SCALE, &tf_scale);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, sizes);
    vip_query_input(task->network, index, VIP_BUFFER_PROP_TF_ZERO_POINT, &tf_zerop);

    sz = get_element_num(sizes, num_of_dims, data_format);
    stride = type_get_bytes(data_format);
    /* tensorData = (unsigned char *)malloc(stride * sz * sizeof(unsigned char)); */
    tensorData = (unsigned char *)hal_malloc(stride * sz * sizeof(unsigned char));
    memset(tensorData, 0, stride * sz * sizeof(unsigned char));
    *file_size = stride * sz * sizeof(unsigned char);

    for (i = 0; i < sz; i++)
    {
        fscanf(tensorFile, "%f ", &fval);
        float32_to_dtype(fval, &tensorData[stride * i], data_format, quant_format,
                         fixed_point_pos, tf_scale, tf_zerop);
    }

    fclose(tensorFile);

    if (VIP_BUFFER_FORMAT_INT4 == data_format ||
        VIP_BUFFER_FORMAT_UINT4 == data_format) {
        vip_uint32_t output_size = type_get_bits(data_format);
        vip_uint32_t output_element = 1;
        void* pack_data = VIP_NULL;
        for (i = 0; i < num_of_dims; i++) {
            output_element *= sizes[i];
            output_size *= sizes[i];
            if (0 == i) {
                /* round up */
                output_size = (output_size + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
            }
        }
        /* pack_data = malloc(output_size); */
        pack_data = hal_malloc(output_size);
        memset(pack_data, 0, output_size);
        pack_4bit_data(tensorData, pack_data, output_element, sizes[0]);
        /* free(tensorData); */
        hal_free(tensorData);
        tensorData = pack_data;
    }

    return tensorData;
}

void destroy_network(vpm_network_task_t *task)
{
    int i = 0;

    if (task == VIP_NULL) {
        printf("failed task is NULL\n");
        return;
    }

    vip_destroy_network(task->network);

    for (i = 0; i < task->input_count; i++) {
        vip_destroy_buffer(task->input_buffers[i]);
    }
    /* free(task->input_buffers); */
    hal_free(task->input_buffers);

    for (i = 0; i < task->output_count; i++) {
        vip_destroy_buffer(task->output_buffers[i]);
    }
    /* free(task->output_buffers); */
    hal_free(task->output_buffers);
    task->output_buffers = VIP_NULL;

}

void destroy_test_resources(vpm_network_task_t *taskes, vip_int32_t task_count)
{
    vip_int32_t i = 0, j = 0;
    vpm_network_task_t *task = VIP_NULL;

    if (taskes == VIP_NULL) {
        printf("failed task is NULL\n");
        return;
    }

    printf("destroy teset resource task_count=%d\n", task_count);

    for (j = 0; j < task_count; j++) {
        task = &taskes[j];

        if (task != VIP_NULL) {
            if (task->input_names != VIP_NULL) {
                /* free(task->input_names); */
                hal_free(task->input_names);
                task->input_names = VIP_NULL;
            }
            if (task->output_names != VIP_NULL) {
                /* free(task->output_names); */
                hal_free(task->output_names);
                task->output_names = VIP_NULL;
            }
            if (task->golden_names != VIP_NULL) {
                /* free(task->golden_names); */
                hal_free(task->golden_names);
                task->golden_names = VIP_NULL;
            }
        }
        else {
            printf("failed to destroy task=%d\n", j);
        }
    }

    for (i = 0; i < taskes->string_count; i++) {
        if (taskes->base_strings[i] != VIP_NULL) {
            /* free(taskes->base_strings[i]); */
            hal_free(taskes->base_strings[i]);
            taskes->base_strings[i] = VIP_NULL;
        }
    }

    if (taskes->base_strings != VIP_NULL) {
        /* free(taskes->base_strings); */
        hal_free(taskes->base_strings);
        taskes->base_strings = VIP_NULL;
    }

    /* free(taskes); */
    hal_free(taskes);
    //taskes = VIP_NULL;

}

vpm_network_task_t *parse_sample_txt_file(const char *file_name, int *Count)
{
    static const char *tokens[] = {
        "[network]",
        "[input]",
        "[golden]",
        "[output]"
    };
    vpm_network_task_t *task = VIP_NULL, *cur_task = VIP_NULL;
    char line_buffer[255] = {0};
    char *line_string = VIP_NULL;
    int  line_count = 0;
    int  line_len = 0;
    int  network_count = 0;
    int  i;
    int  current_data = 0;
    int  first_task = 1;

    /* Load the task file as a string buffer. */
    FILE *fp = fopen(file_name, "r");
    if (fp == VIP_NULL) {
        printf("failed to open file=%s\n", file_name);
        return VIP_NULL;
    }

    /* Count the networks. */
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        line_count++;
#if defined(_WIN32)
        if ((line_buffer[strlen(line_buffer) - 2] == '\r') ||
            (line_buffer[strlen(line_buffer) - 2] == '\n')) {
            line_buffer[strlen(line_buffer) - 2] = '\0';
        }
#else
        if ((line_buffer[strlen(line_buffer) - 1] == '\r') ||
            (line_buffer[strlen(line_buffer) - 1] == '\n')) {
            line_buffer[strlen(line_buffer) - 1] = '\0';
        }
#endif
        else {
            line_buffer[strlen(line_buffer) - 1] = '\0';
        }
        if(strcmp(line_buffer, tokens[0]) == 0) {
            network_count++;
        }
    }
    *Count = network_count;

    /* Allocate taskes. */
    /* task = (vpm_network_task_t *)malloc(sizeof(vpm_network_task_t) * network_count); */
    task = (vpm_network_task_t *)hal_malloc(sizeof(vpm_network_task_t) * network_count);
    vip_memset((void*)task, sizeof(vpm_network_task_t)  * network_count);
    if (task == VIP_NULL) {
        fclose(fp);
        return VIP_NULL;
    }

    /* Setup task: set up the strings. */
    /*task->base_strings = (char **)malloc(sizeof(char *) * line_count);*/
    task->base_strings = (char **)hal_malloc(sizeof(char *) * line_count);
    task->string_count = line_count;
    fseek(fp, 0, SEEK_SET);
    line_count = 0;
    cur_task = task;

    /* Setup base string. */
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        line_len = strlen(line_buffer);
        /* cur_task->base_strings[line_count] = (char *)malloc(line_len + 1); */
        cur_task->base_strings[line_count] = (char *)hal_malloc(line_len + 1);
        memset(cur_task->base_strings[line_count], 0, line_len + 1);
        strcpy(cur_task->base_strings[line_count], line_buffer);
#if defined(_WIN32)
        if (cur_task->base_strings[line_count][line_len - 2] == '\n' ||
            cur_task->base_strings[line_count][line_len - 2] == '\r') {
            cur_task->base_strings[line_count][line_len - 2] = '\0';
        }
#else
        if (cur_task->base_strings[line_count][line_len - 1] == '\n' ||
            cur_task->base_strings[line_count][line_len - 1] == '\r') {
            cur_task->base_strings[line_count][line_len - 1] = '\0';
        }
#endif
        else {
            cur_task->base_strings[line_count][line_len - 1] = '\0';
        }

        line_count++;
        memset(line_buffer, 0, sizeof(line_buffer));
    }
    fclose(fp);

    /* Locate the nbg strings. */
    cur_task = task;
    cur_task->output_names = NULL;   /* Output name is optional. */
    cur_task->output_count = 0;
    cur_task->input_count  = 0;
    cur_task->golden_count = 0;
    cur_task->output_buffers = NULL;
    cur_task->input_buffers = NULL;
    cur_task->input_names = NULL;
    cur_task->golden_names = NULL;

    for (i = 0; i < line_count; i++) {
        line_string = task->base_strings[i];
        /* Parse the string data. */
        if (line_string[0] == '#') {
            continue;
        }
        else if (line_string[0] == '[') {
            if (strcmp(line_string, tokens[0]) == 0) {
                current_data = 1;
                if (first_task == 0) {
                    cur_task++;
                    cur_task->base_strings = task->base_strings;
                    cur_task->output_count = 0;
                    cur_task->input_count  = 0;
                    cur_task->golden_count = 0;
                    cur_task->output_buffers = NULL;
                    cur_task->input_buffers = NULL;
                    cur_task->output_names = NULL;
                    cur_task->input_names = NULL;
                    cur_task->golden_names = NULL;
                }
                else {
                    first_task = 0;
                }
            }
            else if (strcmp(line_string, tokens[1]) == 0) {
                current_data = 2;
            }
            else if (strcmp(line_string, tokens[2]) == 0) {
                current_data = 3;
            }
            else if (strcmp(line_string, tokens[3]) == 0) {
                current_data = 4;
            }
            else {
                printf("Bad task file. Wrong line @ %d.\n", i);
                /* free(task->base_strings);
                free(task); */
                hal_free(task->base_strings);
                hal_free(task);
                task = VIP_NULL;
                break;
            }
        }
        else{
            switch (current_data) {
            case 1: /* Network */
                cur_task->nbg_name = i;
                break;

            case 2: /* Input */
                /* Count how many inputs and assign it accordingly. */
                {
                    int iCount = 0;
                    int j;
                    for (j = i; ; j++) {
                        if (j >= line_count)
                            break;

                        if ((task->base_strings[j][0] == '#') ||
                            (task->base_strings[j][0] == '\0'))
                            continue;

                        if (task->base_strings[j][0] != '[') {
                            iCount++;
                        }
                        else {
                            break;
                        }
                    }

                    cur_task->input_count = iCount;
                    /* cur_task->input_names = (int *)malloc(sizeof(int) * iCount); */
                    cur_task->input_names = (int *)hal_malloc(sizeof(int) * iCount);

                    iCount = 0;
                    for (; ; i++) {
                        if (i >= line_count)
                            break;

                        if ((task->base_strings[i][0] == '#') ||
                            (task->base_strings[i][0] == '\0'))
                            continue;

                        if (task->base_strings[i][0] != '[') {
                            cur_task->input_names[iCount++] = i;
                        }
                        else {
                            i--;
                            break;
                        }
                    }
                }
                break;

            case 3: /* Golden */
                {
                    int iCount = 0;
                    int j;
                    for (j = i; ; j++) {
                        if (j >= line_count)
                            break;

                        if ((task->base_strings[j][0] == '#') ||
                            (task->base_strings[j][0] == '\0'))
                            continue;

                        if (task->base_strings[j][0] != '[') {
                            iCount++;
                        }
                        else {
                            break;
                        }
                    }

                    cur_task->golden_count = iCount;
                    /* cur_task->golden_names = (int *)malloc(sizeof(int) * iCount); */
                    cur_task->golden_names = (int *)hal_malloc(sizeof(int) * iCount);

                    iCount = 0;
                    for (; ; i++) {
                        if (i >= line_count)
                            break;

                        if ((task->base_strings[i][0] == '#') ||
                            (task->base_strings[i][0] == '\0'))
                            continue;

                        if (task->base_strings[i][0] != '[') {
                            cur_task->golden_names[iCount++] = i;
                        }
                        else {
                            i--;
                            break;
                        }
                    }
                }
                break;

            case 4: /* Output */
                {
                    int iCount = 0;
                    int j;
                    for (j = i; ; j++) {
                        if (j >= line_count)
                            break;

                        if ((task->base_strings[j][0] == '#') ||
                            (task->base_strings[j][0] == '\0'))
                            continue;

                        if (task->base_strings[j][0] != '[') {
                            iCount++;
                        }
                        else {
                            break;
                        }
                    }

                    cur_task->output_count = iCount;
                    /* cur_task->output_names = (int *)malloc(sizeof(int) * iCount); */
                    cur_task->output_names = (int *)hal_malloc(sizeof(int) * iCount);

                    iCount = 0;
                    for (; ; i++) {
                        if (i >= line_count)
                            break;

                        if ((task->base_strings[i][0] == '#') ||
                            (task->base_strings[i][0] == '\0'))
                            continue;

                        if (task->base_strings[i][0] != '[') {
                            cur_task->output_names[iCount++] = i;
                        }
                        else {
                            i--;
                            break;
                        }
                    }
                }
                break;

            default:
                break;
            }
        }
    }

    return task;
}

void init_test_resources(vpm_network_task_t **taskes, const char *taskFileName, int *Count)
{
    vpm_network_task_t *items = VIP_NULL;

    items = parse_sample_txt_file(taskFileName, Count);

    *taskes = items;
}

vip_status_e query_hardware_info(void)
{
    vip_uint32_t version = vip_get_version();
    vip_uint32_t device_count = 0;
    vip_uint32_t cid = 0;
    vip_uint32_t *core_count = VIP_NULL;
    vip_uint32_t i = 0;

    if (version >= 0x00010601) {
        vip_query_hardware(VIP_QUERY_HW_PROP_CID, sizeof(vip_uint32_t), &cid);
        vip_query_hardware(VIP_QUERY_HW_PROP_DEVICE_COUNT, sizeof(vip_uint32_t), &device_count);
        /* core_count = (vip_uint32_t*)malloc(sizeof(vip_uint32_t) * device_count); */
        core_count = (vip_uint32_t*)hal_malloc(sizeof(vip_uint32_t) * device_count);
        vip_query_hardware(VIP_QUERY_HW_PROP_CORE_COUNT_EACH_DEVICE,
                          sizeof(vip_uint32_t) * device_count, core_count);
        printf("cid=0x%x, device_count=%d\n", cid, device_count);
        for (i = 0; i < device_count; i++) {
            printf("  device[%d] core_count=%d\n", i, core_count[i]);
        }
        /* free(core_count); */
        hal_free(core_count);
    }
    return VIP_SUCCESS;
}

/* Create the network in the task. */
vip_status_e create_network(
    vpm_network_task_t *task,
    vip_uint32_t device_id,
    vip_uint32_t network_id
    )
{
    vip_status_e status = VIP_SUCCESS;
    char *file_name = VIP_NULL;
    void *output_map = VIP_NULL;
    int file_size = 0;
    int i = 0;
    int input_count = 0;
    vip_buffer_create_params_t param;

    /* Load nbg data. */
    /* file_name = task->base_strings[task->nbg_name];
    file_size = get_file_size((const char *) file_name);
    if (file_size <= 0) {
        printf("Network binary file %s can't be found.\n", file_name);
        status = VIP_ERROR_INVALID_ARGUMENTS;
        return status;
    } */

#ifdef CREATE_NETWORK_FROM_MEMORY
    /* network_buffer[network_id] = malloc(file_size); */
    /* load_file(file_name, network_buffer[network_id]); */

    #if defined (__linux__)
    TimeBegin(1);
    #endif

    /*printf("*****vip_create_network network_buffer=0x%p*****\n", (void *)network_buffer);
    for(int j = 0; j < 80; j++) {
        printf("***** network_buffer[%d] =0x%x *****\n", j, network_buffer[j]);
    }
    printf("*****network_buffer 0=0x%x, 1=0x%x, 2=0x%x, 3=0x%x *****\n", network_buffer[0], network_buffer[1], network_buffer[2], network_buffer[3]);*/
    network_buffer[0] = 0x56;
    status = vip_create_network(network_buffer, network_file_size, VIP_CREATE_NETWORK_FROM_MEMORY,
                                &task->network);
    /* free(network_buffer[network_id]);
    network_buffer[network_id] = VIP_NULL; */

#elif CREATE_NETWORK_FROM_FILE
    #if defined (__linux__)
    TimeBegin(1);
    #endif

    status = vip_create_network(file_name, 0, VIP_CREATE_NETWORK_FROM_FILE, &task->network);

#elif CREATE_NETWORK_FROM_FLASH
    /* This is a demo code for DDR-less project.
       You don't need to allocate this memory if you are in DDR-less products.
       You can use vip_create_network() function to create a network.
       network_buffer is the staring address of flash */
    network_buffer[network_id] = vsi_nn_MallocAlignedBuffer(file_size, 4096, 4096);
    load_file(file_name, network_buffer[network_id]);

#if defined (__linux__)
    TimeBegin(1);
#endif

    status = vip_create_network(network_buffer[network_id], file_size, VIP_CREATE_NETWORK_FROM_FLASH,
                                &task->network);
#endif
    if (status != VIP_SUCCESS) {
        printf("Network creating failed. Please validate the content of network.\n");
        return status;
    }

    /* Create input buffers. */
    vip_query_network(task->network, VIP_NETWORK_PROP_INPUT_COUNT, &task->input_count);
    /* if (input_count != task->input_count) {
        printf("Error: input count mismatch. Required inputs by network: %d, actually provided: %d.\n",
                input_count, task->input_count);
        status = VIP_ERROR_MISSING_INPUT_OUTPUT;
        return status;
    } */

    /* task->input_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * task->input_count); */
    task->input_buffers = (vip_buffer *)hal_malloc(sizeof(vip_buffer) * task->input_count);
    for (i = 0; i < task->input_count; i++) {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_input(task->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_input(task->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_input(task->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_input(task->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_input(task->network, i, VIP_BUFFER_PROP_NAME, name);
        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                vip_query_input(task->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                &param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                vip_query_input(task->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                &param.quant_data.affine.scale);
                vip_query_input(task->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                &param.quant_data.affine.zeroPoint);
            default:
            break;
        }

        printf("input %d dim %d %d %d %d, data_format=%d, quant_format=%d, name=%s\n",
               i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
               param.data_format, param.quant_format, name);

        /* switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                       param.quant_data.affine.zeroPoint);
                break;
            default:
                printf(", none-quant\n");
        } */

        status = vip_create_buffer(&param, sizeof(param), &task->input_buffers[i]);
        if (status != VIP_SUCCESS) {
            printf("fail to create input %d buffer, status=%d\n", i, status);
            return status;
        }
    }

    /* Create output buffer. */
    vip_query_network(task->network, VIP_NETWORK_PROP_OUTPUT_COUNT, &task->output_count);
    /* task->output_buffers = (vip_buffer *)malloc(sizeof(vip_buffer) * task->output_count); */
    task->output_buffers = (vip_buffer *)hal_malloc(sizeof(vip_buffer) * task->output_count);
    for (i = 0; i < task->output_count; i++) {
        vip_char_t name[256];
        memset(&param, 0, sizeof(param));
        param.memory_type = VIP_BUFFER_MEMORY_TYPE_DEFAULT;
        vip_query_output(task->network, i, VIP_BUFFER_PROP_DATA_FORMAT, &param.data_format);
        vip_query_output(task->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION, &param.num_of_dims);
        vip_query_output(task->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);
        vip_query_output(task->network, i, VIP_BUFFER_PROP_QUANT_FORMAT, &param.quant_format);
        vip_query_output(task->network, i, VIP_BUFFER_PROP_NAME, name);
        switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                vip_query_output(task->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                 &param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                vip_query_output(task->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                 &param.quant_data.affine.scale);
                vip_query_output(task->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                 &param.quant_data.affine.zeroPoint);
                break;
            default:
            break;
        }

        printf("ouput %d dim %d %d %d %d, data_format=%d, name=%s\n",
               i, param.sizes[0], param.sizes[1], param.sizes[2], param.sizes[3],
               param.data_format, name);

        /* switch(param.quant_format) {
            case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
                printf(", dfp=%d\n", param.quant_data.dfp.fixed_point_pos);
                break;
            case VIP_BUFFER_QUANTIZE_TF_ASYMM:
                printf(", scale=%f, zero_point=%d\n", param.quant_data.affine.scale,
                       param.quant_data.affine.zeroPoint);
                break;
            default:
                printf(", none-quant\n");
        } */

        status = vip_create_buffer(&param, sizeof(param), &task->output_buffers[i]);
         if (status != VIP_SUCCESS) {
             printf("fail to create output %d buffer, status=%d\n", i, status);
            return status;
         }
        /* memset output_buffer to zero */
        /* {
            vip_uint32_t k = 0;
            vip_uint32_t buf_size = type_get_bits(param.data_format);
            for (k = 0; k < param.num_of_dims; k++) {
                buf_size *= param.sizes[k];
                if (0 == k) { */
                    /* round up */
        /*            buf_size = (buf_size + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
                }
            }
            output_map = vip_map_buffer(task->output_buffers[i]);
            vip_memset(output_map, buf_size);
            vip_flush_buffer(task->output_buffers[i], VIP_BUFFER_OPER_TYPE_INVALIDATE);
            vip_unmap_buffer(task->output_buffers[i]);
            output_map = VIP_NULL;
        }*/
    }

#if defined (__linux__)
    TimeEnd(1);
    //printf("nbg name=%s\n", file_name);
    printf("create network %d: %lu us.\n", network_id, (unsigned long)TimeGet(1));
#endif

    /* {
        vip_uint32_t mem_pool_size = 0;
        vip_uint8_t core_count = 0;
        vip_query_network(task->network, VIP_NETWORK_PROP_MEMORY_POOL_SIZE, &mem_pool_size);
        printf("memory pool size=%dbyte\n", mem_pool_size);
        vip_query_network(task->network, VIP_NETWORK_PROP_CORE_COUNT, &core_count);
        printf("network core count=%d\n", core_count);
    } */ 


    /* the defalt device id is 0. we need chang it when not use device 0.*/
    /* if (device_id > 0) {
        printf("vpm run start set device id=%d.\n", device_id);
        status = vip_set_network(task->network, VIP_NETWORK_PROP_SET_DEVICE_ID, &device_id);
        if(status != VIP_SUCCESS) {
            printf("vpm run set device id fail, id = %d.\n", device_id);
        }
        printf("vpm run set device id success, id = %d.\n", device_id);
    } */

    return status;
}

vip_status_e load_golden_data(vpm_network_task_t *task)
{
    vip_status_e status = VIP_SUCCESS;
    vip_int32_t i = 0 ;
    char *golden_name = VIP_NULL;

    if (task->golden_count > 0) {
        if (VIP_NULL == task->golden_data ) {
            /* task->golden_data = malloc(sizeof(void*) * task->golden_count); */
            task->golden_data = hal_malloc(sizeof(void*) * task->golden_count);
            memset(task->golden_data, 0, (sizeof(void*) * task->golden_count));
        }
        if (VIP_NULL == task->golden_size) {
            /* task->golden_size = malloc(sizeof(vip_uint32_t*) * task->golden_count); */
            task->golden_size = hal_malloc(sizeof(vip_uint32_t*) * task->golden_count);
        }
    }

    printf("golden file count=%d\n", task->golden_count);
    for (i = 0; i < task->golden_count; i++) {
        if (task->base_strings[task->golden_names[i]] != VIP_NULL) {
            golden_name = task->base_strings[task->golden_names[i]];
            printf("%d read golden file %s\n", i, golden_name);
            task->golden_size[i] = get_file_size(golden_name);
            if (0 == task->golden_size[i]) {
                printf("    fail to read golden_%d name=%s\n", i, golden_name);
                continue;
            }

            /* task->golden_data[i] = malloc(task->golden_size[i]); */
            task->golden_data[i] = hal_malloc(task->golden_size[i]);
            load_file(golden_name, (void *)task->golden_data[i]);
        }
    }

    return status;
}

vip_status_e free_golden_data(vpm_network_task_t *task)
{
    int i = 0;

    if (task->golden_data != VIP_NULL) {
        for (i = 0; i < task->golden_count; i++) {
            if (task->golden_data[i] != VIP_NULL) {
                /* free(task->golden_data[i]); */
                hal_free(task->golden_data[i]);
                task->golden_data[i] = VIP_NULL;
            }
        }

        /* free(task->golden_data); */
        hal_free(task->golden_data);
        task->golden_data = VIP_NULL;
    }
    if (task->golden_size != VIP_NULL) {
        /* free(task->golden_size); */
        hal_free(task->golden_size);
        task->golden_size = VIP_NULL;
    }

    return VIP_SUCCESS;
}

vip_status_e load_input_data(vpm_network_task_t *task)
{
    vip_status_e status = VIP_SUCCESS;
    void *data;
    void *file_data = VIP_NULL;
    char *file_name;
    vip_uint32_t file_size;
    vip_uint32_t buff_size;
    int i;

    /* Load input buffer data. */
    for (i = 0; i < task->input_count; i++) {
        /* file_type_e file_type;
        file_name = task->base_strings[task->input_names[i]];
        printf("input %d name: %s\n", i , file_name);
        file_type = get_file_type(file_name); */

        /*switch(file_type)
        {
            case NN_FILE_TENSOR:
                file_data = (void *)get_tensor_data(task, file_name, &file_size, i);
                break;
            case NN_FILE_BINARY:
                file_data = (void *)get_binary_data(file_name, &file_size);
                break;
            case NN_FILE_TEXT:
                file_data = (void *)get_tensor_data(task, file_name, &file_size, i);
                break;
            default:
                printf("error input file type\n");
                break;
        }*/

        data = vip_map_buffer(task->input_buffers[i]);
        buff_size = vip_get_buffer_size(task->input_buffers[i]);
		file_size = input_file_size;
        vip_memcpy(data, input_buffers, buff_size > file_size ? file_size : buff_size);
        vip_unmap_buffer(task->input_buffers[i]);

        /*if (file_data != VIP_NULL) {
            free(file_data);
            file_data = VIP_NULL;
        }*/
    }

    return status;
}

/* Create buffers, and configure the netowrk in the task. */
vip_status_e set_network_input_output(vpm_network_task_t *task)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;

    /* Load input buffer data. */
    for (i = 0; i < task->input_count; i++) {
        /* Set input. */
        status = vip_set_input(task->network, i, task->input_buffers[i]);
        if (status != VIP_SUCCESS) {
            printf("fail to set input %d\n", i);
            goto ExitFunc;
        }
    }

    for (i = 0; i < task->output_count; i++) {
        if (task->output_buffers[i] != VIP_NULL) {
            status = vip_set_output(task->network, i, task->output_buffers[i]);
            if (status != VIP_SUCCESS) {
                printf("fail to set output\n");
                goto ExitFunc;
            }
        }
        else {
            printf("fail output %d is null. output_counts=%d\n", i, task->output_count);
            status = VIP_ERROR_FAILURE;
            goto ExitFunc;
        }
    }

ExitFunc:
    return status;
}

static float int8_to_fp32(signed char val, signed char fixedPointPos)
{
    float result = 0.0f;

    if (fixedPointPos > 0) {
        result = (float)val * (1.0f / ((float) (1 << fixedPointPos)));
    }
    else {
        result = (float)val * ((float) (1 << -fixedPointPos));
    }

    return result;
}

static float int16_to_fp32(vip_int16_t val, signed char fixedPointPos)
{
    float result = 0.0f;

    if (fixedPointPos > 0) {
        result = (float)val * (1.0f / ((float) (1 << fixedPointPos)));
    }
    else {
        result = (float)val * ((float) (1 << -fixedPointPos));
    }

    return result;
}
static vip_float_t affine_to_fp32(vip_int32_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    result = ((vip_float_t)val - zeroPoint) * scale;
    return result;
}

static vip_float_t uint8_to_fp32(vip_uint8_t val, vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    result = (val - (vip_uint8_t)zeroPoint) * scale;
    return result;
}

typedef union
{
    unsigned int u;
    float f;
} _fp32_t;

static float fp16_to_fp32(const short in)
{
    const _fp32_t magic = { (254 - 15) << 23 };
    const _fp32_t infnan = { (127 + 16) << 23 };
    _fp32_t o;
    // Non-sign bits
    o.u = (in & 0x7fff ) << 13;
    o.f *= magic.f;
    if(o.f  >= infnan.f)
    {
        o.u |= 255 << 23;
    }
    //Sign bit
    o.u |= (in & 0x8000 ) << 16;
    return o.f;
}

static vip_bool_e get_top(
    float *pf_prob,
    float *pf_max_prob,
    unsigned int *max_class,
    unsigned int out_put_count,
    unsigned int top_num
    )
{
    unsigned int i, j;

    if (top_num > 10) return vip_false_e;

    memset(pf_max_prob, 0xfe, sizeof(float) * top_num);
    memset(max_class, 0xff, sizeof(float) * top_num);
    for (j = 0; j < top_num; j++) {
        for (i=0; i<out_put_count; i++) {
            if ((i == *(max_class+0)) || (i == *(max_class+1)) || (i == *(max_class+2)) ||
                (i == *(max_class+3)) || (i == *(max_class+4)))
                continue;
            if (pf_prob[i] > *(pf_max_prob+j)) {
                *(pf_max_prob+j) = pf_prob[i];
                *(max_class+j) = i;
            }
        }
    }

    return vip_true_e;
}

void show_result(
    void* buffer,
    unsigned int count,
    signed int data_type,
    vip_int32_t quant_format,
    unsigned char fix_pos,
    vip_int32_t zeroPoint,
    vip_float_t scale
    )
{
    unsigned int i;
    unsigned int max_class[5];
    float fMaxProb[5];
    float *outBuf = VIP_NULL;
    short *ptr_fp16 = VIP_NULL;
    vip_int8_t *ptr_int8 = VIP_NULL;
    vip_uint8_t *ptr_uint8 = VIP_NULL;
    vip_int16_t *ptr_int16 = VIP_NULL;
    vip_int32_t *ptr_int32 = VIP_NULL;

    /* outBuf = (float *) malloc(count* sizeof(float)); */
    outBuf = (float *) hal_malloc(count* sizeof(float));
    memset(outBuf, 0, count* sizeof(float));
    if(outBuf == NULL) {
        printf("Can't malloc space \n");
    }
    if(data_type == VIP_BUFFER_FORMAT_INT8 ||
       data_type == VIP_BUFFER_FORMAT_INT4) {
        ptr_int8 = (vip_int8_t *)buffer;
        for(i = 0; i < count; i++) {
            if (quant_format == VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT) {
                outBuf[i] = int8_to_fp32(ptr_int8[i], fix_pos);
            }
            else if (quant_format == VIP_BUFFER_QUANTIZE_TF_ASYMM) {
                vip_int32_t src_value = 0;
                integer_convert(&ptr_int8[i], &src_value, VIP_BUFFER_FORMAT_INT8, VIP_BUFFER_FORMAT_INT32);
                outBuf[i] = affine_to_fp32(src_value, zeroPoint, scale);
            }
            else {
                outBuf[i] = *((float*)ptr_int8);
            }
        }
    }
    else if(data_type == VIP_BUFFER_FORMAT_FP16) {
        ptr_fp16 = (short *)buffer;
        for(i = 0; i < count; i++) {
            outBuf[i] = fp16_to_fp32(ptr_fp16[i]);
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_UINT8 ||
        data_type == VIP_BUFFER_FORMAT_UINT4) {
        ptr_uint8 = (vip_uint8_t *)buffer;
        for(i = 0; i < count; i++) {
            outBuf[i] = affine_to_fp32(ptr_uint8[i], zeroPoint, scale);
        }
    }
    else if(data_type == VIP_BUFFER_FORMAT_INT16) {
        ptr_int16 = (vip_int16_t *)buffer;
        for(i = 0; i < count; i++) {
            outBuf[i] = int16_to_fp32(ptr_int16[i], fix_pos);
        }
    }
    else if(data_type == VIP_BUFFER_FORMAT_INT32) {
        ptr_int32 = (vip_int32_t *)buffer;
        if (quant_format == VIP_BUFFER_QUANTIZE_NONE) {
            for(i = 0; i < count; i++) {
            outBuf[i] = (float)ptr_int32[i];
            }
        }
    }
    else if (data_type == VIP_BUFFER_FORMAT_FP32) {
        memcpy(outBuf,buffer,count* sizeof(float));
    }
    else {
        printf("not support this format TOP5\n");
        return;
    }

    if (!get_top((float*)outBuf, fMaxProb, max_class, count, 5)) {
        printf("Fail to show result.\n");
    }

    printf(" --- Top5 ---\n");

    for (i=0; i<5; i++) {
        printf("%3d: %8.6f\n", max_class[i], (float)fMaxProb[i]);
    }

    /* free(outBuf); */
    hal_free(outBuf);
}

int save_txt_file(
    void* buffer,
    unsigned int ele_size,
    signed int data_type,
    vip_int32_t quant_format,
    unsigned char fix_pos,
    vip_int32_t zeroPoint,
    vip_float_t scale,
    vip_uint32_t index
    )
{
    #define TMPBUF_SZ  (512)
    char filename[255] = {'\0'};
    vip_uint32_t i = 0;
    FILE        *fp;
    float fp_data = 0.0;
    vip_uint8_t *data = (vip_uint8_t*)buffer;
    vip_uint32_t type_size = type_get_bytes(data_type);
    vip_uint8_t buf[TMPBUF_SZ];
    vip_uint32_t count = 0;

    sprintf(filename, "output_%d.txt", index);
    fp = fopen(filename, "w");

    for (i = 0; i < ele_size; i++) {
        if (data_type == VIP_BUFFER_FORMAT_INT8) {
            if (quant_format == VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT) {
                fp_data = int8_to_fp32(*data, fix_pos);
            }
            else if (quant_format == VIP_BUFFER_QUANTIZE_TF_ASYMM) {
                vip_int32_t src_value = 0;
                integer_convert(data, &src_value, VIP_BUFFER_FORMAT_INT8, VIP_BUFFER_FORMAT_INT32);
                fp_data = affine_to_fp32(src_value, zeroPoint, scale);
            }
            else {
                fp_data = *((float*)data);
            }
        }
        else if (data_type == VIP_BUFFER_FORMAT_FP16) {
            fp_data = fp16_to_fp32(*((short *)data));
        }
        else if (data_type == VIP_BUFFER_FORMAT_UINT8) {
            fp_data = uint8_to_fp32(*data, zeroPoint, scale);
        }
        else if (data_type == VIP_BUFFER_FORMAT_INT16) {
            fp_data = int16_to_fp32(*((short *)data), fix_pos);
        }
        else if (data_type == VIP_BUFFER_FORMAT_FP32) {
            fp_data = *((float*)data);
        }
        else if(data_type == VIP_BUFFER_FORMAT_INT32) {
            if (quant_format == VIP_BUFFER_QUANTIZE_NONE) {
                fp_data = (float)(*((vip_int32_t*)data));
            }
        }
        else {
            printf("not support this format into output.txt file\n");
            goto error;
        }

        data += type_size;

        count += sprintf((char *)&buf[count], "%f%s", fp_data, "\n");

        if ((count + 50) > TMPBUF_SZ)
        {
            fwrite(buf, count, 1, fp );
            count = 0;
        }

    }

    fwrite(buf, count, 1, fp );
    fflush(fp);
error:
    fclose(fp );

    return 0;
}

vip_int32_t inference_profile(
    vpm_network_task_t *task,
    vip_uint32_t count
    )
{
    vip_inference_profile_t profile;
    vip_int32_t ret = 0;
    vip_uint32_t tolerance = 1000; /* 1000us */
    vip_uint32_t time_diff = 0;

    vip_query_network(task->network, VIP_NETWORK_PROP_PROFILING, &profile);
    printf("profile inference time=%dus, cycle=%d\n", profile.inference_time,
           profile.total_cycle);
    if (1 == count) {
        task->infer_cycle = profile.total_cycle;
        task->infer_time = profile.inference_time;
    }
    else {
        vip_float_t rate = (vip_float_t)task->infer_cycle / (vip_float_t)profile.total_cycle;
        time_diff = (task->infer_time > profile.inference_time) ? (task->infer_time - profile.inference_time) : \
                     (profile.inference_time - task->infer_time);
        if (((rate > 1.05) || (rate < 0.95)) && (time_diff > tolerance)) {
            ret = -1;
        }
    }

    task->total_infer_cycle += (vip_uint64_t)task->infer_cycle;
    task->total_infer_time += (vip_uint64_t)task->infer_time;

    return ret;
}

vip_int32_t check_result(vpm_network_task_t *task)
{
    char *out_name = VIP_NULL;
    void *out_data = VIP_NULL;
    void* out_data_4bit = VIP_NULL;
    vip_int32_t j = 0;
    vip_int32_t ret = 0;
    vip_uint32_t k = 0;
    vip_int32_t data_format = 0;
    vip_int32_t output_fp  = 0;
    vip_int32_t quant_format = 0;
    vip_int32_t output_counts = 0;
    vip_uint32_t output_size = 0;
    vip_int32_t zeroPoint = 0;
    vip_float_t scale;
    vip_buffer_create_params_t param;
    vip_bool_e is_odd_bit4s = vip_false_e;

    vip_query_network(task->network, VIP_NETWORK_PROP_OUTPUT_COUNT, &output_counts);

    for (j = 0; j < output_counts; j++) {
        unsigned int output_element = 1;
        memset(&param, 0, sizeof(param));
        vip_query_output(task->network, j, VIP_BUFFER_PROP_QUANT_FORMAT, &quant_format);
        vip_query_output(task->network, j, VIP_BUFFER_PROP_TF_SCALE,
                         &param.quant_data.affine.scale);
        scale = param.quant_data.affine.scale;
        vip_query_output(task->network, j, VIP_BUFFER_PROP_TF_ZERO_POINT,
                           &param.quant_data.affine.zeroPoint);
        zeroPoint = param.quant_data.affine.zeroPoint;
        vip_query_output(task->network, j, VIP_BUFFER_PROP_DATA_FORMAT,
                         &param.data_format);
        data_format = param.data_format;
        vip_query_output(task->network, j, VIP_BUFFER_PROP_NUM_OF_DIMENSION,
                         &param.num_of_dims);
        vip_query_output(task->network, j, VIP_BUFFER_PROP_FIXED_POINT_POS,
                         &param.quant_data.dfp.fixed_point_pos);
        output_fp = param.quant_data.dfp.fixed_point_pos;
        vip_query_output(task->network, j, VIP_BUFFER_PROP_SIZES_OF_DIMENSION, param.sizes);

        output_size = type_get_bits(data_format);
        for (k = 0; k < param.num_of_dims; k++) {
            output_element *= param.sizes[k];
            output_size *= param.sizes[k];
            if (0 == k) {
                /* round up */
                output_size = (output_size + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
            }
        }

        out_data = vip_map_buffer(task->output_buffers[j]);
        if (VIP_BUFFER_FORMAT_INT4 == data_format || VIP_BUFFER_FORMAT_UINT4 == data_format) {
            is_odd_bit4s = (output_size % 2 == 0) ? vip_false_e : vip_true_e;
            /*out_data_4bit = malloc(output_element * sizeof(vip_uint8_t)); */
            out_data_4bit = hal_malloc(output_element * sizeof(vip_uint8_t));
            memset(out_data_4bit, 0, output_element * sizeof(vip_uint8_t));
            unpack_4bit_data(out_data, out_data_4bit, output_size, param.sizes[0],
                VIP_BUFFER_FORMAT_INT4 == data_format ? vip_true_e : vip_false_e);
            pack_4bit_data(out_data_4bit, out_data, output_element, param.sizes[0]);
        #ifdef SAVE_OUTPUT_TXT_FILE
            /* save output to .txt file */
            save_txt_file(out_data_4bit, output_element, VIP_BUFFER_FORMAT_UINT8,
                quant_format, output_fp, zeroPoint, scale, j);
        #endif
        }
        else {
        #ifdef SAVE_OUTPUT_TXT_FILE
            /* save output to .txt file */
            save_txt_file(out_data, output_element, data_format, quant_format, output_fp, zeroPoint, scale, j);
        #endif
        }

        /* save output to binary file */
        if ((task->output_names != NULL) && (task->base_strings[task->output_names[j]] != VIP_NULL)) {
            out_name = task->base_strings[task->output_names[j]];
            save_file(out_name, out_data, output_size);
        }

        if ((j < task->golden_count) && (task->golden_data != VIP_NULL) && (task->golden_data[j] != VIP_NULL)) {
            if (output_size <= task->golden_size[j]) {
                printf("******* golden TOP5 ********\n");
                if (VIP_BUFFER_FORMAT_INT4 == data_format || VIP_BUFFER_FORMAT_UINT4 == data_format) {
                    /* void* buffer_tmp = malloc(output_element * sizeof(vip_uint8_t)); */
                    void* buffer_tmp = hal_malloc(output_element * sizeof(vip_uint8_t));
                    memset(buffer_tmp, 0, output_element * sizeof(vip_uint8_t));
                    unpack_4bit_data(task->golden_data[j], buffer_tmp, output_size, param.sizes[0],
                        VIP_BUFFER_FORMAT_INT4 == data_format ? vip_true_e : vip_false_e);
                    show_result(buffer_tmp, output_element, data_format,
                        quant_format, output_fp, zeroPoint, scale);
                    /* free(buffer_tmp); */
                    hal_free(buffer_tmp);
                }
                else {
                    show_result(task->golden_data[j], output_element, data_format,
                        quant_format, output_fp, zeroPoint, scale);
                }
            }
        }

        printf("******* nb TOP5 ********\n");
        if (VIP_BUFFER_FORMAT_INT4 == data_format || VIP_BUFFER_FORMAT_UINT4 == data_format) {
            show_result(out_data_4bit, output_element, data_format, quant_format, output_fp, zeroPoint, scale);
        }
        else {
            show_result(out_data, output_element, data_format, quant_format, output_fp, zeroPoint, scale);
        }

        if ((j < task->golden_count) && (task->golden_data != VIP_NULL) && (task->golden_data[j] != VIP_NULL)) {
            /* Check result. */
            vip_bool_e is_equal = vip_false_e;
            /* check int4 or uint4 output num is odd */
            if (is_odd_bit4s) {
                is_equal = (memcmp(out_data, task->golden_data[j], task->golden_size[j] - 1) == 0) ?
                    vip_true_e : vip_false_e;
                is_equal &= compare_low_4bits((vip_uint8_t*)out_data + output_size -1,
                        (vip_uint8_t*)task->golden_data[j] + output_size -1);
            }
            else {
                is_equal = (memcmp(out_data, task->golden_data[j], task->golden_size[j]) == 0) ?
                    vip_true_e : vip_false_e;
            }

            if (!is_equal) {
                char name[255] = {'\0'};
                if (task->output_names != NULL &&
                    task->output_names[j] > 0) {
                    out_name = task->base_strings[task->output_names[j]];
                }
                else {
                    sprintf(name, "failed_output_%d.bin", j);
                    out_name = name;
                }
                save_file(out_name, out_data, output_size);
                printf("    Test output %d failed: data mismatch. Output saved in file %s "
                       "for further analysis.\n", j, out_name);
                ret = -1;
                vip_memset(out_data, output_size);
            }
            else {
                vip_memset(out_data, task->golden_size[j]);
                printf("    Test output %d passed.\n\n", j);
                if ((vip_flush_buffer(task->output_buffers[j], VIP_BUFFER_OPER_TYPE_FLUSH)) != VIP_SUCCESS) {
                    printf("flush output%d cache failed.\n", j);
                }
            }
        }

        vip_unmap_buffer(task->output_buffers[j]);
        if (VIP_BUFFER_FORMAT_INT4 == data_format || VIP_BUFFER_FORMAT_UINT4 == data_format) {
            /* free(out_data_4bit); */
            hal_free(out_data_4bit);
        }
    }

    return ret;
}

int vpm_run(int argc, const char **argv)
{
    vip_status_e status = VIP_SUCCESS;
    //vip_char_t *file_name = VIP_NULL;
    vip_int32_t task_count = 1;
    vip_int32_t i = 0, k = 0;
    vip_uint32_t loop_count = 1;
    vip_uint32_t count = 0;
    vip_int32_t ret = 0;
    vip_uint32_t version = 0;
    vip_uint32_t device_id = 0;
    vpm_network_task_t *tasks = VIP_NULL;
    vip_uint32_t time_out = 0;
    vip_uint32_t hardware_bypass = 0;

    /*if (argc < 2) {
        printf("%s\n", usage);
        return -1;
    }

    for (i = 0; i< argc; i++) {
        if (!strcmp(argv[i], "-l")) {
            loop_count = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-d")) {
            device_id = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-s")) {
            file_name = argv[++i];
        }
        else if (!strcmp(argv[i], "-t")) {
            time_out = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-b")) {
            hardware_bypass = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "-h")) {
            printf("%s\n", usage);
            return 0;
        }
    }
    printf("loop_count=%d, device_id=%d, file_name=%s\n",loop_count, device_id, file_name);

    if (VIP_NULL == file_name) {
        printf("%s\n", usage);
        return -1;
    } */
    printf("test started.\n\n");

    version = vip_get_version();
    printf("init vip lite, driver version=0x%08x...\n", version);

    status = vip_init();
    if (status != VIP_SUCCESS) {
        printf("failed to init vip\n");
        ret = -1;
        goto exit;
    }
    printf("vip lite init OK.\n\n");

    query_hardware_info();

    /* init_test_resources(&tasks, file_name, &task_count);
    printf("init test resources, task_count: %d ...\n", task_count); */
    tasks = (vpm_network_task_t *)hal_malloc(sizeof(vpm_network_task_t));
    vip_memset((void*)tasks, sizeof(vpm_network_task_t) );
    if (tasks == VIP_NULL) {
        return VIP_NULL;
    }

    for (i = 0; i < task_count; i++) {
        tasks[i].loop_count = loop_count;
    }

    printf("create/prepare networks ...\n");
    if (tasks != VIP_NULL) {
        for (i = 0; i < task_count; i++) {
            /* printf("task i=%d, binary name: %s\n", i, tasks[i].base_strings[tasks[i].nbg_name]); */
            printf("task i=%d, binary load!\n", i);
            status = create_network(&tasks[i], device_id, i);
            if (status != VIP_SUCCESS) {
                printf("create network %d failed.\n", i);
                ret = -1;
                break;
            }

            if (0 != time_out) {
                status = vip_set_network(tasks[i].network, VIP_NETWORK_PROP_SET_TIME_OUT, &time_out);
                if (status != VIP_SUCCESS) {
                    printf("fail to set time out of network\n");
                    ret = -1;
                    break;
                }
            }

            #if defined (__linux__)
            TimeBegin(2);
            #endif
            /* Prepare network. */
            status = vip_prepare_network(tasks[i].network);
            if (status != VIP_SUCCESS) {
                printf("fail prpare network, status=%d\n", status);
                ret = -1;
                break;
            }

            /* load_golden_data(&tasks[i]); */
            load_input_data(&tasks[i]);

            #if defined (__linux__)
            TimeEnd(2);
            printf("prepare network %d: %lu us.\n", i, (unsigned long) TimeGet(2));
            #endif
        }

        /* run network */
        while(count < loop_count) {
            count++;
            for (i = 0; i < task_count; i++) {
                printf("task: %d, loop count: %d\n", i, count);
                status = set_network_input_output(&tasks[i]);
                if (status != VIP_SUCCESS) {
                    printf("set network input/output %d failed.\n", i);
                    ret = -1;
                    goto exit;
                }

                /* printf("start to run network=%s\n", tasks[i].base_strings[tasks[i].nbg_name]); */
                printf("start to run network!\n");
                #if defined (__linux__)
                TimeBegin(0);
                #endif
                /* it is only necessary to call vip_flush_buffer() after set vpmdENABLE_FLUSH_CPU_CACHE to 2 */
                /* for (k = 0; k < tasks[i].input_count; k++) {
                    if ((vip_flush_buffer(tasks[i].input_buffers[k], VIP_BUFFER_OPER_TYPE_FLUSH)) != VIP_SUCCESS) {
                        printf("flush input%d cache failed.\n", k);
                    }
                } */

                status = vip_run_network(tasks[i].network);
                if (status != VIP_SUCCESS) {
                    if (status == VIP_ERROR_CANCELED) {
                        printf("network is canceled.\n");
                        ret = VIP_ERROR_CANCELED;
                        goto exit;
                    }
                    printf("fail to run network, status=%d, taskCount=%d\n", status, i);
                    ret = -2;
                    goto exit;
                }

                for (k = 0; k < tasks[i].output_count; k++) {
                  if ((vip_flush_buffer(tasks[i].output_buffers[k], VIP_BUFFER_OPER_TYPE_INVALIDATE)) != VIP_SUCCESS){
                      printf("flush output%d cache failed.\n", k);
                    }
                }

                #if defined (__linux__)
                TimeEnd(0);
                printf("run time for this network %d: %lu us.\n", i, (unsigned long) TimeGet(0));
                #endif
                printf("run network done...\n");

                inference_profile(&tasks[i], count);

                if (hardware_bypass == 0) {
                    ret = check_result(&tasks[i]);
                    if (ret != 0) {
                        goto exit;
                    }
                }
            }
        };

        if (loop_count > 1) {
            for (i = 0; i < task_count; i++) {
                printf("task %d, profile avg inference time=%dus, cycle=%d\n", i,
                    (vip_uint32_t)(tasks[i].total_infer_time / tasks[i].loop_count),
                    (vip_uint32_t)(tasks[i].total_infer_cycle / tasks[i].loop_count));
            }
        }
    }
    /*else {
        printf("failed to read %s\n", file_name);
    }*/

exit:
    if (tasks != VIP_NULL) {
        for (i = 0; i < task_count; i++) {
            free_golden_data(&tasks[i]);

            vip_finish_network(tasks[i].network);

            destroy_network(&tasks[i]);

            if (network_buffer[i] != VIP_NULL) {
                #ifdef CREATE_NETWORK_FROM_FLASH
                vsi_nn_FreeAlignedBuffer((vip_uint8_t*)network_buffer[i]);
                #else
                /* free(network_buffer[i]); */
                #endif
                network_buffer[i] = VIP_NULL;
            }
        }
    }

    for (i = 0; i < task_count; i++) {
        if (network_buffer[i] != VIP_NULL) {
            /* free(network_buffer[i]); */
            network_buffer[i] = VIP_NULL;
        }
    }

    destroy_test_resources(tasks, task_count);

    status = vip_destroy();
    if (status != VIP_SUCCESS) {
        printf("fail to destory vip\n");
    }

    /*printf("*****end network_buffer 0=0x%x, 1=0x%x, 2=0x%x, 3=0x%x *****\n", network_buffer[0], network_buffer[1], network_buffer[2], network_buffer[3]);
    for(int j = 0; j < 80; j++) {
        printf("***** network_buffer[%d] =0x%x *****\n", j, network_buffer[j]);
    }*/
    return ret;
}
FINSH_FUNCTION_EXPORT_ALIAS(vpm_run, vpm_run, user defined cmd);

int hello_world(int argc, const char **argv)
{
    printf("hello world.\n");
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(hello_world, hello_world, user defined cmd);
