#ifndef SUNXI_HAL_INTERRUPT_H
#define SUNXI_HAL_INTERRUPT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef enum {
	HAL_IRQ_PRIO2PREEMPT = 0,
	HAL_IRQ_PREEMPT2PRIO,
} hal_irqprio_map_t;

typedef enum {
	HAL_IRQ_PRIO_LOW = 0,
	HAL_IRQ_PRIO_MIDDLE,
	HAL_IRQ_PRIO_HIGH,
	HAL_IRQ_PRIO_TOP,

	HAL_IRQ_PRIO_MAX,
	HAL_IRQ_PRIO_BASE = HAL_IRQ_PRIO_LOW,
} hal_irq_prio_t;

#define hal_irq_prio_t_valid(_t) \
	(((_t) >= HAL_IRQ_PRIO_BASE) && ((_t) < HAL_IRQ_PRIO_MAX))

typedef enum hal_irqreturn {
	HAL_IRQ_OK		= (0 << 0),
	HAL_IRQ_ERR		= (1 << 0),
} hal_irqreturn_t;

typedef hal_irqreturn_t (*hal_irq_handler_t)(void *);

#define in_interrupt(...)       hal_interrupt_get_nest()
#define in_nmi(...)             (0)

void hal_interrupt_enable(void);
void hal_interrupt_disable(void);
unsigned long hal_interrupt_is_disable(void);
unsigned long hal_interrupt_save(void);
void hal_interrupt_restore(unsigned long flag);
uint32_t hal_interrupt_get_nest(void);

int32_t hal_request_irq(int32_t irq, hal_irq_handler_t handler, const char *name, void *data);
void hal_free_irq(int32_t irq);
int hal_enable_irq(int32_t irq);
void hal_disable_irq(int32_t irq);

void hal_irq_set_prioritygrouping(uint32_t group);
uint32_t hal_irq_get_prioritygrouping(void);
int hal_irq_set_specific_priority(int32_t irq, uint32_t preemptpriority, uint32_t subpriority);
int hal_irq_get_specific_priority(int32_t irq, uint32_t *p_preemptpriority, uint32_t *p_subpriority);
int hal_irq_set_priority(int32_t irq, hal_irq_prio_t priority);
int hal_irq_get_priority(int32_t irq, hal_irq_prio_t *priority);

void hal_nvic_irq_set_priority(int32_t irq, uint32_t priority);
uint32_t hal_nvic_irq_get_priority(int32_t irq);

void hal_interrupt_clear_pending(int32_t irq);
int32_t hal_interrupt_is_pending(int32_t irq);
void hal_interrupt_set_pending(int32_t irq);

void hal_interrupt_enter(void);
void hal_interrupt_leave(void);

#ifdef __cplusplus
}
#endif

#endif
