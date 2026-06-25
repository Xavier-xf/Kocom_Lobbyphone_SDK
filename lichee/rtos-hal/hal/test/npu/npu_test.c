#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <hal_mem.h>
#include <hal_time.h>
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
#define NPU_CMD_SIZE_REG	 (NPU_BASE_ADDR + 0x3A4)
#define NPU_HOST_IF_CTRL_REG (NPU_BASE_ADDR + 0x3A8)
#define NPU_DMA_HIGH         (NPU_BASE_ADDR + 0x66C)
#define NPU_DMA_LOW          (NPU_BASE_ADDR + 0x668)
#define GIC_SRC_NPU          81

#define CCMU_BASE                           (0x02001000)
#define CCMU_NPU_CLK_REG					(CCMU_BASE + 0x06E0)
#define CCMU_NPU_BGR_REG					(CCMU_BASE + 0x06EC)
#define CCMU_PLL_NPU_CTRL_REG				(CCMU_BASE + 0x0080)

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