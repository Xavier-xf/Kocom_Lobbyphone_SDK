#ifndef __CK_IRQ_H__
#define __CK_IRQ_H__

#include <stdint.h>

typedef enum irq_trigger_type
{
	IRQ_TRIGGER_TYPE_LEVEL = 0,
	IRQ_TRIGGER_TYPE_EDGE_RISING,
	IRQ_TRIGGER_TYPE_EDGE_FALLING,
	IRQ_TRIGGER_TYPE_EDGE_BOTH
} irq_trigger_type_t;

int irq_core_set_irq_trigger_type(uint32_t irq_id, irq_trigger_type_t type);

#endif

