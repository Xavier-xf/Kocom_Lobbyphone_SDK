#include <include.h>
#include "internal.h"

#if CONFIG_UART_PORT == 0
#define UART_IOBASE SUNXI_UART0_BASE
#endif
#if CONFIG_UART_PORT == 1
#define UART_IOBASE SUNXI_UART1_BASE
#endif
#if CONFIG_UART_PORT == 2
#define UART_IOBASE  SUNXI_UART2_BASE
#define UART_TX_PIN  SUNXI_GPH(5)
#define UART_TX_FUNC 5
#define UART_RX_PIN  SUNXI_GPH(6)
#define UART_RX_FUNC 5
#endif
#if CONFIG_UART_PORT == 3
#define UART_IOBASE SUNXI_UART3_BASE
#define UART_TX_PIN  SUNXI_GPE(0)
#define UART_TX_FUNC 7
#define UART_RX_PIN  SUNXI_GPE(1)
#define UART_RX_FUNC 7
#endif

static void boart_uart_init(void)
{
	sunxi_clock_init_uart(CONFIG_UART_PORT);

	sunxi_gpio_set_cfgpin(UART_TX_PIN, UART_TX_FUNC);
	sunxi_gpio_set_cfgpin(UART_RX_PIN, UART_RX_FUNC);

	uart_init(UART_IOBASE);
}

static void __jump(void)
{
#define RISCV_START_ADDR			(0x06010204)
	u32 addr = CONFIG_RAM_START_ADDRESS;

	while (addr == CONFIG_RAM_START_ADDRESS || addr == 0)
		addr = *(volatile u32 *)(RISCV_START_ADDR);

	printf("jump to second firmware: 0x%08x\n", addr);

	asm volatile("mv s1, %0" : : "r"(addr) : "memory");
	asm volatile("jr s1");
}

int main(void)
{
#if defined(CONFIG_IRQCHIP_USED)
	interrupt_init();
#endif

	boart_uart_init();

	printf("Hello, RISCV %d\n", get_time_us());
	sensor_init();

#if defined(CONFIG_IRQCHIP_USED)
	interrupt_exit();
#endif
	__jump();
	return 0;
}
