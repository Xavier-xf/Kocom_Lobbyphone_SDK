/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
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
#include <stdint.h>
#include <stdio.h>
#include <soc.h>
#include <core_rv32.h>
#include <hal_interrupt.h>
#include <interrupt.h>
#include <excep.h>
#include "csi_rv32_gcc.h"
#include <inttypes.h>

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
#include <platform_irq_sun20iw3.h>
#include <sunxi_hal_common.h>
#include "ck_irq.h"

#define ic_readb(addr) hal_readb(addr)
#define ic_readl(addr) hal_readl(addr)

#define ic_writeb(data, addr) hal_writeb(data, addr)
#define ic_writel(data, addr) hal_writel(data, addr)

#define get_major(x)            ((x))
#define get_minor(x)              ((x))

typedef struct irq_controller
{
	unsigned long reg_base_addr;
} irq_controller_t;

static void clic_irq_priority_init(void);
#endif

extern void Default_Handler(void);
extern void SysTick_Handler(void);

void (*g_irqvector[208])(void);
void (*g_nmivector)(void);
#ifdef CONFIG_STANDBY
uint8_t g_irq_status[208] = {0};
#endif

typedef void (*irq_flow_handler_t)(int irq, void *data);

#define CLIC_PERIPH_IRQ_OFFSET (16)
#define E907_SUNXI_IRQ_MAX (192)
#define CLIC_IRQ_NUM (E907_SUNXI_IRQ_MAX + CLIC_PERIPH_IRQ_OFFSET)

static const char *irq_string[CLIC_IRQ_NUM] = {
	[0 ... 2] = NULL,
	"Machine_Software",
	"User_Timer",
	"Supervisor_Timer",
	NULL,
	"CORET",
	[8 ... 10] = NULL,
	"Machine_External",
	[12 ... 15] = NULL,
	"CPUX_MBOX_RX",     /* CPUX MSGBOX READ IRQ FOR CPUX */
	"CPUX_MBOX_TX",     /* CPUX MSGBOX READ IRQ FOR E907 */
	"UART0",
	"UART1",
	"UART2",
	"UART3",
	[22 ... 24] = NULL,
	"TWI0",
	"TWI1",
	"TWI2",
	"TWI3",
	"TWI4",
	NULL,
	"SPI0",
	"SPI1",
	"SPI2",
	"PWM",
	"SPI_FLASH",
	"WIEGAND",
	"SPI3",
	[38 ... 39] = NULL,
	"DMIC",
	"AUDIO_CODEC",
	"I2S_PCM0",
	"I2S_PCM1",
	NULL,
	"USB_OTG",
	"USB_EHCI",
	"USB_OHCI",
	[48 ... 55] = NULL,
	"SMHC0",
	"SMHC1",
	"SMHC2",
	"MSI",
	"SMC",
	NULL,
	"GMAC",
	NULL,
	"CCU_FERR",
	"AHB_HREADY_TIME_OUT",
	"DMA_CPUX_NS",
	"DMA_CPUX_S",
	"CE_NS",
	"CE_S",
	"SPINLOCK",
	"HSTIMER0",
	"HSTIMER1",
	"GPADC",
	"THS",
	"TIMER0",
	"TIMER1",
	"TIMER2",
	"TIMER3",
	"WDG",
	"IOMMU",
	"NPU",
	"VE",
	"GPIOA_NS",
	"GPIOA_S",
	[85 ... 86] = NULL,
	"GPIOC_NS",
	"GPIOC_S",
	"GPIOD_NS",
	"GPIOD_S",
	"GPIOE_NS",
	"GPIOE_S",
	"GPIOF_NS",
	"GPIOF_S",
	"GPIOG_NS",
	"GPIOG_S",
	"GPIOH_NS",
	"GPIOH_S",
	"GPIOI_NS",
	"GPIOI_S",
	"DMAC_E907_NS",
	"DMAC_E907_S",
	"DE",
	NULL,
	"G2D",
	"LCD",
	NULL,
	"DSI",
	[109 ... 110] = NULL,
	"CSI_DMA0",
	"CSI_DMA1",
	"CSI_DMA2",
	"CSI_DMA3",
	NULL,
	"CSI_PARSER0",
	"CSI_PARSER1",
	"CSI_PARSER2",
	NULL,
	"CSI_CMB",
	"CSI_TDM",
	"CSI_TOP_PKT",
	NULL,
	"CSI_ISP0",
	"CSI_ISP1",
	"CSI_ISP2",
	"CSI_ISP3",
	"VIPP0",
	"VIPP1",
	"VIPP2",
	"VIPP3",
	[132 ... 143] = NULL,
	"E907_MBOX_RX",     /* e907 msgbox read irq for e907 Interrupt */
	"E907_MBOX_TX",     /* e907 msgbox write irq for cpux Interrupt */
	"E907_WDG",
	"E907_TIMER0",
	"E907_TIMER1",
	"E907_TIMER2",
	"E907_TIMER3",
	NULL,
	"NMI",
	"PPU",
	"ALARM",
	"AHBS_HREADY_TIME_OUT",
	"PMC",
	"GIC_C0",
	"TWD",
	NULL,
};

struct arch_irq_desc
{
    hal_irq_handler_t handle_irq;
    void *data;
};

static struct arch_irq_desc arch_irqs_desc[E907_SUNXI_IRQ_MAX];

void show_irqs(void)
{
	int i;
	int enable;
	const char *status;
	const char *irq_name;
	printf("IRQ    Status    Name\r\n");
	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (csi_vic_get_enabled_irq(i)) {
			irq_name = irq_string[i];
			if (irq_name)
				printf("%3d    Enabled   %s\r\n", i, irq_name);
			else
				printf("%3d    Enabled\r\n", i);
		}
	}
}

static hal_irqreturn_t clic_null_handler(void *data)
{
    return IRQ_NONE;
}

void enable_irq(unsigned int irq_num)
{
    if (NMI_EXPn != irq_num)
    {
        csi_vic_enable_irq(irq_num);
#ifdef CONFIG_STANDBY
        g_irq_status[irq_num] = 1;
#endif
    }
}

void disable_irq(unsigned int irq_num)
{
    if (NMI_EXPn != irq_num)
    {
        csi_vic_disable_irq(irq_num);
#ifdef CONFIG_STANDBY
        g_irq_status[irq_num] = 0;
#endif
    }
}

#ifdef CONFIG_STANDBY
void irq_suspend(void)
{
	int i = 0;

	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (g_irq_status[i])
			csi_vic_disable_irq(i);
	}
}

void irq_resume(void)
{
	int i = 0;

	for (i = 0; i < CLIC_IRQ_NUM; i++) {
		if (g_irq_status[i])
			csi_vic_enable_irq(i);
	}
}
#endif

int32_t arch_request_irq(int32_t irq, hal_irq_handler_t handler, void *data)
{
    if (irq < CLIC_PERIPH_IRQ_OFFSET)
    {
        g_irqvector[irq] = (void *)handler;
        return irq;
    }

    if (irq + CLIC_PERIPH_IRQ_OFFSET < CLIC_IRQ_NUM)
    {
        if (handler && arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq == (void *)clic_null_handler)
        {
            arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)handler;
            arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].data = data;
        }
        return irq;
    }
    printf("Wrong irq NO.(%"PRIu32") to request !!\n", irq);
    return -1;
}

void arch_free_irq(uint32_t irq)
{
    if (irq < CLIC_PERIPH_IRQ_OFFSET)
    {
        g_irqvector[irq] = (void *)Default_Handler;
        return;
    }
    if (irq + CLIC_PERIPH_IRQ_OFFSET < CLIC_IRQ_NUM)
    {
        arch_irqs_desc[irq - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)clic_null_handler;
    }
    return;
}

int request_threaded_irq(unsigned int irq, hal_irq_handler_t handler,
                         hal_irq_handler_t thread_fn, unsigned long irqflags,
                         const char *devname, void *dev_id)
{
    return arch_request_irq(irq, (void *)handler, dev_id);
}

const void *free_irq(int32_t irq)
{
    arch_free_irq(irq);
	return NULL;
}

unsigned long riscv_cpu_handle_interrupt(unsigned long scause, unsigned long sepc, unsigned long stval, irq_regs_t *regs)
{
    printf("E907 will not support the interrupt mode!\n");
	printf("cause:0x%08lx mepc:0x%08lx mtval:0x%08lx\r\n", scause, sepc, stval);
	return 0;
}

void clic_common_handler(void)
{
#ifndef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    hal_interrupt_enter();
#endif
    int id = (__get_MCAUSE() & 0xfff) - CLIC_PERIPH_IRQ_OFFSET;
    if (arch_irqs_desc[id].handle_irq &&
        arch_irqs_desc[id].handle_irq != (void *)clic_null_handler)
    {
        arch_irqs_desc[id].handle_irq(arch_irqs_desc[id].data);
    }
    else
    {
        printf("no handler for irq %d\n", id);
    }
#ifndef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    hal_interrupt_leave();
#endif
}

void irq_vectors_init(void)
{
    int i;

    for (i = CLIC_PERIPH_IRQ_OFFSET; i < CLIC_IRQ_NUM; i++)
    {
#ifdef CONFIG_STANDBY
		g_irq_status[i] = 0;
#endif
        disable_irq(i - CLIC_PERIPH_IRQ_OFFSET);
        g_irqvector[i] = (void *)clic_common_handler;
        arch_irqs_desc[i - CLIC_PERIPH_IRQ_OFFSET].handle_irq = (void *)clic_null_handler;
        arch_irqs_desc[i - CLIC_PERIPH_IRQ_OFFSET].data = NULL;
    }

#ifdef CONFIG_STANDBY
	for (i = 0; i < CLIC_PERIPH_IRQ_OFFSET; i++) {
		if (csi_vic_get_enabled_irq(i))
			g_irq_status[i] = 1;
		else
			g_irq_status[i] = 0;
	}
#endif

    g_irqvector[CORET_IRQn] = SysTick_Handler;
#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    clic_irq_priority_init();
#endif
}

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
/*
 * | 7    8 - CLICINTCTL_NLBITS |   ......     | 7 - PLAT_CLIC_CLICINTCTLBITS   0 |
 * |   preempt priority         | sub priority | Invalid bit, value 1             |
 */
#define PRIORITY_REG_MAX		(0xff)
#define CLICINTCTL_NLBITS_MASK		(PRIORITY_REG_MAX >> CLICINTCTL_NLBITS)
#define PRIORITY_INVALID_BIT_MASK	(PRIORITY_REG_MAX >> PLAT_CLIC_CLICINTCTLBITS)
#define PREEMPTPRIORITY_VALID_SHIFT	(8 - CLICINTCTL_NLBITS)
#define SUBPRIORITY_VALID_SHIFT		(8 - PLAT_CLIC_CLICINTCTLBITS)

#define PREEMPTPRIORITY_REG_MAX		(PRIORITY_REG_MAX)
#define PREEMPTPRIORITY_REG_MIN		(CLICINTCTL_NLBITS_MASK)
#define SUBPRIORITY_REG_MAX		(PRIORITY_REG_MAX)
#define SUBPRIORITY_REG_MIN		(PRIORITY_INVALID_BIT_MASK)

#define PREEMPTPRIORITY_MAX		((~CLICINTCTL_NLBITS_MASK & PRIORITY_REG_MAX) >> PREEMPTPRIORITY_VALID_SHIFT)
#define PREEMPTPRIORITY_MIN		(0)
#define SUBPRIORITY_MAX			(((~PRIORITY_INVALID_BIT_MASK & CLICINTCTL_NLBITS_MASK) & PRIORITY_REG_MAX) >> SUBPRIORITY_VALID_SHIFT)
#define	SUBPRIORITY_MIN			(0)

#ifdef CONFIG_INTERRUPT_NEST_DEBUG_LOG
#define nest_dbg(fmt, arg...)		do { printf(fmt, ##arg); } while(0)
#else
#define nest_dbg(fmt, arg...)		do { } while(0)
#endif

/* Type definitions. */
#if __riscv_xlen == 64
	#define portSTACK_TYPE	uint64_t
#elif __riscv_xlen == 32
	#define portSTACK_TYPE	uint32_t
#else
	#error Assembler did not define __riscv_xlen
#endif
typedef portSTACK_TYPE StackType_t;
/* defined in vectors.S */
extern uint32_t g_irq_stack_base[];
/* Arrange from low to high */
static uint8_t hal_priority_set[HAL_IRQ_PRIO_MAX] = {0};

void clic_show_hal_priority_set(void)
{
	int i;

	nest_dbg("PRIORITY_INVALID_BIT_MASK: 0x%x\n", PRIORITY_INVALID_BIT_MASK);
	nest_dbg("CLICINTCTL_NLBITS_MASK: 0x%x\n", CLICINTCTL_NLBITS_MASK);
	nest_dbg("PREEMPTPRIORITY_VALID_SHIFT: %d\n", PREEMPTPRIORITY_VALID_SHIFT);
	nest_dbg("SUBPRIORITY_VALID_SHIFT: %d\n", SUBPRIORITY_VALID_SHIFT);
	nest_dbg("\n");
	nest_dbg("PREEMPTPRIORITY_REG_MAX: %d\n", PREEMPTPRIORITY_REG_MAX);
	nest_dbg("PREEMPTPRIORITY_REG_MIN: %d\n", PREEMPTPRIORITY_REG_MIN);
	nest_dbg("SUBPRIORITY_REG_MAX: %d\n", SUBPRIORITY_REG_MAX);
	nest_dbg("SUBPRIORITY_REG_MIN: %d\n", SUBPRIORITY_REG_MIN);
	nest_dbg("\n");
	printf("sum of preempt priority: %d\n", (0x1 << CLICINTCTL_NLBITS));
	printf("PREEMPTPRIORITY_MAX: %d\n", PREEMPTPRIORITY_MAX);
	printf("PREEMPTPRIORITY_MIN: %d\n", PREEMPTPRIORITY_MIN);
	printf("SUBPRIORITY_MAX: %d\n", SUBPRIORITY_MAX);
	printf("SUBPRIORITY_MIN: %d\n", SUBPRIORITY_MIN);

	for (i = 0; i < HAL_IRQ_PRIO_MAX; i++)
		printf("hal_priority(%d) = PREEMPTPRIORITY(%d)\n", i, hal_priority_set[i]);
}

static void clic_irqstack_overflow_check_init(StackType_t *addr, StackType_t size)
{
	StackType_t i;

	for (i = 0; i < (size / sizeof(StackType_t)); i++) {
		*(addr + i) = IRQSTACK_BOTTOM_MAGIC;
	}
}

void clic_irq_priority_init_early(void)
{
	/* set CLICCFG.nlbits */
	hal_writel((hal_readl(NEST_PLAT_ROOT_IC_REG_BASE_ADDR + 0x0) & ~(0xf << 1)) | (CLICINTCTL_NLBITS << 1), NEST_PLAT_ROOT_IC_REG_BASE_ADDR + 0x0);
}

static void clic_irq_priority_init(void)
{
	int i;
	StackType_t *overflow_check = g_irq_stack_base;

	clic_irqstack_overflow_check_init(overflow_check, 1 * sizeof(StackType_t));

	if (HAL_IRQ_PRIO_MAX < 2) {
		nest_dbg("irq priority init error, there is only one priority. HAL_IRQ_PRIO_MAX: %d\n", HAL_IRQ_PRIO_MAX);
		return;
	}

	hal_writel(0x0, NEST_PLAT_ROOT_IC_REG_BASE_ADDR + 0x8);

	hal_priority_set[0] = PREEMPTPRIORITY_MIN;
	hal_priority_set[HAL_IRQ_PRIO_MAX - 1] = PREEMPTPRIORITY_MAX;

	/* different value of hal_irq_priority_t may lead to the same priority */
	for (i = 1; i < (HAL_IRQ_PRIO_MAX - 1); i++) {
		hal_priority_set[i] = PREEMPTPRIORITY_MIN + i;
		if (hal_priority_set[i] == PREEMPTPRIORITY_MAX)
			/* make sure there is only one TOP priority */
			hal_priority_set[i] = hal_priority_set[i - 1];
	}

	dsb();
}

static uint8_t clic_halprio_to_preemptprio(hal_irq_prio_t priority)
{
	return hal_priority_set[priority];
}

int clic_irq_set_priority(const struct irq_controller *ic, uint32_t irq_id, uint32_t preemptpriority, uint32_t subpriority)
{
	uint32_t reg_addr;
	uint8_t priority_val;
	uint32_t tmpv;

	tmpv = PREEMPTPRIORITY_MIN;
	if ((preemptpriority < tmpv) || (preemptpriority > PREEMPTPRIORITY_MAX)) {
		nest_dbg("preemptpriority %d invalid\n", preemptpriority);
		return -1;
	}

	tmpv = SUBPRIORITY_MIN;
	if ((subpriority < tmpv) || (subpriority > SUBPRIORITY_MAX)) {
		nest_dbg("subpriority %d invalid\n", subpriority);
		return -2;
	}

	reg_addr = ic->reg_base_addr + CLIC_INT_X_CTRL_REG_OFF(irq_id);
	priority_val = ic_readb(reg_addr) & PRIORITY_INVALID_BIT_MASK;
	priority_val |= (uint8_t)(preemptpriority << PREEMPTPRIORITY_VALID_SHIFT);
	priority_val |= (uint8_t)(subpriority << SUBPRIORITY_VALID_SHIFT);
	ic_writeb(priority_val, reg_addr);
	nest_dbg("set priority reg(0x%x) value(0x%x), preemptpriority(%d), subpriority(%d)\n", reg_addr, priority_val, preemptpriority, subpriority);

	return 0;
}

int clic_irq_get_priority(const struct irq_controller *ic, uint32_t irq_id, uint32_t *preemptpriority, uint32_t *subpriority)
{
	uint32_t reg_addr;
	uint8_t priority_val;

	if (!preemptpriority || !subpriority) {
		nest_dbg("param invalid, preemptpriority or subpriority is NULL\n");
		return -1;
	}

	reg_addr = ic->reg_base_addr + CLIC_INT_X_CTRL_REG_OFF(irq_id);
	priority_val = ic_readb(reg_addr);

	*subpriority =(uint32_t)(((priority_val | PRIORITY_INVALID_BIT_MASK) & CLICINTCTL_NLBITS_MASK) >> SUBPRIORITY_VALID_SHIFT);
	*preemptpriority =(uint32_t)((priority_val | CLICINTCTL_NLBITS_MASK) >> PREEMPTPRIORITY_VALID_SHIFT);
	nest_dbg("get priority reg(0x%x) value(0x%x), preemptpriority(%d), subpriority(%d)\n", reg_addr, priority_val, *preemptpriority, *subpriority);

	return 0;
}

int clic_irq_priority_map(hal_irqprio_map_t map, hal_irq_prio_t *priority, uint32_t *preemptpriority, uint32_t *subpriority)
{
	int i;
	uint32_t tmpv;

	if (!preemptpriority || !priority) {
		nest_dbg("invalid param NULL\n");
		return -1;
	}

	if (map == HAL_IRQ_PRIO2PREEMPT) {
		if (!hal_irq_prio_t_valid(*priority)) {
			nest_dbg("priority %d invalid\n", *priority);
			return -2;
		}

		*preemptpriority = (uint32_t)clic_halprio_to_preemptprio(*priority);
		/* subpriority of hal_prio fix to SUBPRIORITY_MIN */
		*subpriority = SUBPRIORITY_MIN;
		nest_dbg("priority_map: halprio(%d) to preemtprio(%d), subpriority(%d)\n", *priority, *preemptpriority, *subpriority);
	} else if (map == HAL_IRQ_PREEMPT2PRIO) {
		tmpv = PREEMPTPRIORITY_MIN;
		if ((*preemptpriority < tmpv) || (*preemptpriority > PREEMPTPRIORITY_MAX)) {
			nest_dbg("preemptpriority %d invalid\n", *preemptpriority);
			return -3;
		}

	       for (i = 0; i < HAL_IRQ_PRIO_MAX; i++) {
	               if (hal_priority_set[i] == *preemptpriority) {
	                       *priority = i;
				nest_dbg("priority_map: preemtprio(%d) to halprio(%d)\n", *preemptpriority, *priority);
				/* no subpriority map */
				return 0;
	               }
	       }

	       nest_dbg("hal_priority fail not find, preemtprio: %d\n", *preemptpriority);
	       return -4;
	} else {
		nest_dbg("invalid irqprio map param\n");
		return -5;
	}

	return 0;
}

int arch_irq_priority_map(int32_t irq_num, hal_irqprio_map_t map, hal_irq_prio_t *priority, uint32_t *preemptpriority, uint32_t *subpriority)
{
	return clic_irq_priority_map(map, priority, preemptpriority, subpriority);
}

int arch_irq_set_priority(int32_t irq_num, uint32_t preemptpriority, uint32_t subpriority)
{
	irq_controller_t ic;
	uint32_t irq_id = get_major(irq_num);

	ic.reg_base_addr = NEST_PLAT_ROOT_IC_REG_BASE_ADDR;

	return clic_irq_set_priority(&ic, irq_id, preemptpriority, subpriority);;
}

int arch_irq_get_priority(int32_t irq_num, uint32_t *preemptpriority, uint32_t *subpriority)
{
	irq_controller_t ic;
	uint32_t irq_id = get_major(irq_num);

	ic.reg_base_addr = NEST_PLAT_ROOT_IC_REG_BASE_ADDR;
	return clic_irq_get_priority(&ic, irq_id, preemptpriority, subpriority);
}

static inline void clic_set_trigger_type(uint32_t reg_addr, irq_trigger_type_t type)
{
	uint8_t reg_data, field_value;

	if (type == IRQ_TRIGGER_TYPE_LEVEL)
	{
		field_value = 0;
	}
	else if (type == IRQ_TRIGGER_TYPE_EDGE_RISING)
	{
		field_value = 1;
	}
	else if (type == IRQ_TRIGGER_TYPE_EDGE_FALLING)
	{
		field_value = 3;
	}
	else
	{
		return;
	}

	reg_data = ic_readb(reg_addr);

	reg_data &= ~TRIGGER_TYPE_BIT_MASK;
	reg_data |= field_value << TRIGGER_TYPE_SHIFT;

	ic_writeb(reg_data, reg_addr);
}

int irq_core_set_irq_trigger_type(uint32_t irq_num, irq_trigger_type_t type)
{
	uint32_t reg_addr;
	uint32_t irq_id = get_major(irq_num);

	if (type == IRQ_TRIGGER_TYPE_EDGE_BOTH)
		return -1;

	reg_addr = (uint32_t)NEST_PLAT_ROOT_IC_REG_BASE_ADDR + CLIC_INT_X_ATTR_REG_OFF(irq_id);
	clic_set_trigger_type(reg_addr, type);
	return 0;
}
#endif /* CONFIG_ARCH_RISCV_INTERRUPT_NEST */
