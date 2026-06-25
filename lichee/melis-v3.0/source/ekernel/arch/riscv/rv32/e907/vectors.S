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
#include <cpuport.h>
#include <platform_irq_sun20iw3.h>

/* Enable interrupts when returning from the handler */
#define MSTATUS_PRV1 0x1880

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
#if __riscv_xlen == 64
	#define portWORD_SIZE 8
	#define store_x sd
	#define load_x ld
#elif __riscv_xlen == 32
	#define store_x sw
	#define load_x lw
	#define portWORD_SIZE 4
#else
	#error The toolchain did not define __riscv_xlen
#endif

#define IRQSTACK_BOTTOM_MAGIC	(0x55555555)
#define WORD_SUM (36)
#define portCONTEXT_SIZE (WORD_SUM * portWORD_SIZE)

#define CLIC_MINTTHRESH_REG (NEST_PLAT_ROOT_IC_REG_BASE_ADDR + CLIC_MINTTHRESH_REG_OFF)

#ifdef CONFIG_ARCH_RISCV_FPU
#include <csr.h>

#if __riscv_flen == 64
	#define FPU_REG_SIZE 8
#elif __riscv_flen == 32
	#define FPU_REG_SIZE 4
#else
	#error toolchain did not define __riscv_flen
#endif

#define RISCV_FPU_GENERAL_REG_CNT (32)
#define RISCV_FPU_CSR_REG_CNT (1)
#define FPU_CONTEXT_SIZE ((RISCV_FPU_GENERAL_REG_CNT * FPU_REG_SIZE) + (RISCV_FPU_CSR_REG_CNT * portWORD_SIZE))

#define INTERRUPT_LEVEL_SUM	(0x1 << CLICINTCTL_NLBITS)
/* The value of fpu_use_rec_flag is placed at the end */
#define FPU_SAVE_PROTSIZE	(FPU_CONTEXT_SIZE + portWORD_SIZE)
#define FPU_SAVE_NEST_SIZE 	(INTERRUPT_LEVEL_SUM * FPU_SAVE_PROTSIZE)

.section .bss
	.balign 16
	.global fpu_save_nest
fpu_save_nest:
	.space FPU_SAVE_NEST_SIZE

/* The maxmum priority number of e90x is 32.
 * Dirty value of MSTATUS.FS will not be overwritten before and after nesting.
 * The bit corresponding to the nest_cnt is 1 indicates this nest layer uses FPU.
 * if (fpu_use_rec_flag EXIT > fpu_use_rec_flag ENTRY), means FPU has been used after interrupt ENTRY.
 */
.section .bss
	.balign 16
	.global fpu_use_rec_flag
fpu_use_rec_flag:
	.space 4

.section .bss
	.balign 16
	.global nest_fpu_dirty_rec
nest_fpu_dirty_rec:
	.space 4

.extern awos_arch_save_fpu_status
.extern awos_arch_restore_fpu_status
#endif /* CONFIG_ARCH_RISCV_FPU */

.section .bss
	.balign 16
	.global interrupt_nest_cnt
interrupt_nest_cnt:
	.space 4

.section .bss
	.balign 16
	.global clic_thresh_save
clic_thresh_save:
	.space 4

.section .bss
	.balign 16
	.global mcause_save
mcause_save:
	.space portWORD_SIZE

#ifdef CONFIG_IRQNEST_OSTEST_GENERAL_CTX_CMP
.extern g_irqnest_store_addr
.extern irqnest_ostest_ctx_cmp_entry
.extern irqnest_ostest_ctx_cmp_exit
#endif

.extern rt_interrupt_enter_noirq
.extern rt_interrupt_leave_noirq
#endif /* CONFIG_ARCH_RISCV_INTERRUPT_NEST */

.section .bss
    .align  2
    .globl  g_trapstackalloc
    .global g_trapstackbase
    .global g_top_trapstack
g_trapstackalloc:
g_trapstackbase:
    .space 768
g_top_trapstack:

    .align 2
    .globl g_trap_sp
    .type  g_trap_sp, object
g_trap_sp:
    .long 0
    .size g_trap_sp, .-g_trap_sp

    .globl  g_irq_stack_base
    .global g_irq_stack_top
g_irq_stack_base:
#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    .space 16 * 1024 + CONFIG_INTERRUPT_NEST_STACK
#else
    .space 16 * 1024
#endif
g_irq_stack_top:
    .type  irq_ori_sp, object
irq_ori_sp:
.long 0
    .size irq_ori_sp, .-irq_ori_sp

.text

    .align  2
    .global Default_IRQHandler
    .weak   Default_IRQHandler
    .type   Default_IRQHandler, %function
Default_IRQHandler:
#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    addi sp, sp, -portCONTEXT_SIZE
    store_x x1,  1  * portWORD_SIZE( sp )
    store_x x5,  2  * portWORD_SIZE( sp )
    store_x x6,  3  * portWORD_SIZE( sp )
    store_x x7,  4  * portWORD_SIZE( sp )
    store_x x8,  5  * portWORD_SIZE( sp )
    store_x x9,  6  * portWORD_SIZE( sp )
    store_x x10, 7  * portWORD_SIZE( sp )
    store_x x11, 8  * portWORD_SIZE( sp )
    store_x x12, 9  * portWORD_SIZE( sp )
    store_x x13, 10 * portWORD_SIZE( sp )
    store_x x14, 11 * portWORD_SIZE( sp )
    store_x x15, 12 * portWORD_SIZE( sp )
    store_x x16, 13 * portWORD_SIZE( sp )
    store_x x17, 14 * portWORD_SIZE( sp )
    store_x x18, 15 * portWORD_SIZE( sp )
    store_x x19, 16 * portWORD_SIZE( sp )
    store_x x20, 17 * portWORD_SIZE( sp )
    store_x x21, 18 * portWORD_SIZE( sp )
    store_x x22, 19 * portWORD_SIZE( sp )
    store_x x23, 20 * portWORD_SIZE( sp )
    store_x x24, 21 * portWORD_SIZE( sp )
    store_x x25, 22 * portWORD_SIZE( sp )
    store_x x26, 23 * portWORD_SIZE( sp )
    store_x x27, 24 * portWORD_SIZE( sp )
    store_x x28, 25 * portWORD_SIZE( sp )
    store_x x29, 26 * portWORD_SIZE( sp )
    store_x x30, 27 * portWORD_SIZE( sp )
    store_x x31, 28 * portWORD_SIZE( sp )
    store_x x3,  31 * portWORD_SIZE( sp )
    store_x x4,  32 * portWORD_SIZE( sp )

    addi a0, sp, portCONTEXT_SIZE
    store_x a0,  30 * portWORD_SIZE( sp )

    csrr a0, mepc
    store_x a0,  0 * portWORD_SIZE( sp )
    csrr t0, mscratch
    store_x t0, 33 * portWORD_SIZE( sp )

    csrr t0, mstatus
    store_x t0, 29 * portWORD_SIZE( sp )

    load_x a0, interrupt_nest_cnt
    bnez a0, nest_save_to_sp
    /* store thresh and mcause to label if nest_cnt is 0 */
    la t0, mcause_save
    csrr t1, mcause
    store_x t1, 0(t0)

    la t0, clic_thresh_save
    li t1, CLIC_MINTTHRESH_REG
    lw t2, 0(t1)
    sw t2, 0(t0)
    j nest_save_end

nest_save_to_sp:
    csrr t0, mcause
    store_x t0, 34 * portWORD_SIZE( sp )

    li t0, CLIC_MINTTHRESH_REG
    lw t1, 0(t0)
    sw t1, 35 * portWORD_SIZE( sp )
#ifdef CONFIG_IRQNEST_OSTEST_GENERAL_CTX_CMP
    /* use irqstack if nest_cnt > 0 */
    store_x sp, g_irqnest_store_addr, t0
    mv a1, a0	/* a0 is nest_cnt here */
    csrr a0, mcause
    andi a0, a0, 0x7FF
    call irqnest_ostest_ctx_cmp_entry
#endif
nest_save_end:
#else
    ipush

    li    a0, 0x8
    csrc  mstatus, a0
#endif

#ifdef CONFIG_ARCH_RISCV_FPU
#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    load_x t2, interrupt_nest_cnt
    beqz t2, nest_save_fpu_end

    li t0, FPU_SAVE_PROTSIZE
    mul t0, t0, t2
    la t1, fpu_save_nest
    add a0, t1, t0
    /* store the fpu_use flag */
    li t0, 31
    sub t0, t0, t2
    li t1, 0x7fffffff
    srl t1, t1, t0
    lw t0, fpu_use_rec_flag
    and t1, t1, t0
    sw t1, FPU_CONTEXT_SIZE(a0)

    csrr t2, mstatus
    li t1, SR_FS
    and t0, t2, t1
    li t2, SR_FS_DIRTY
    bne t0, t2, save_fpu_context_end

    li t0, 0x1
    sw t0, nest_fpu_dirty_rec, t1
    la t0, awos_arch_save_fpu_status
    jalr t0
    j save_fpu_context_end
nest_save_fpu_end:
#endif
    csrr  a0, mstatus
    call  fpu_save_inirq
save_fpu_context_end:
#endif

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    /* nest_cnt++ */
    load_x a0, interrupt_nest_cnt
    addi a0, a0, 1
    store_x a0, interrupt_nest_cnt, t0
    addi a0, a0, -1
    bnez a0, enter_sp_switch_end
#endif

    /* switch to IRQ stack */
    la      t1, irq_ori_sp
    sw      sp, 0(t1)
    la      sp, g_irq_stack_top
enter_sp_switch_end:

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    la t0, rt_interrupt_enter_noirq
    jalr t0
    /* Change interrupt thresh to current interrupt level */
    li t1, CLIC_MINTTHRESH_REG
    csrr t0, mintstatus
    sw t0, 0(t1)
    /* Enable global interrupt */
    li t0, 8
    csrrs t0, mstatus, t0
#endif

    csrr    t1, mcause
    andi    t1, t1, 0x3FF
    slli    t1, t1, 2
    la      t0, g_irqvector
    add     t0, t0, t1
    lw      t2, (t0)
    jalr    t2

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    /* Disable global interrupt */
    li t0, 8
    csrrc t0, mstatus, t0

    la t0, rt_interrupt_leave_noirq
    jalr t0

    /* nest_cnt-- */
    load_x a0, interrupt_nest_cnt
    addi a0, a0, -1
    store_x a0, interrupt_nest_cnt, t0
    bnez a0, exit_sp_switch_end
#else
    li      t0, MSTATUS_PRV1
    csrs    mstatus, t0

#    li   a0, 0x1880
#    csrw mstatus, a0
#endif

    /* switch to origin stack */
    la      t1, irq_ori_sp
    lw      sp, 0(t1)
exit_sp_switch_end:

#ifdef CONFIG_ARCH_RISCV_FPU
#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    load_x s7, interrupt_nest_cnt
    beqz s7, nest_restore_fpu_context_end
    /* return to interrupt */
    li t0, FPU_SAVE_PROTSIZE
    mul t0, t0, s7
    la t1, fpu_save_nest
    add s6, t1, t0
    /* record FPU has changed if FS Dirty */
    csrr t2, mstatus
    li a0, SR_FS
    and t0, t2, a0
    li a1, SR_FS_DIRTY
    bne t0, a1, fpu_restore_record_use_end
    li t0, 1
    sll t0, t0, s7
    lw t1, fpu_use_rec_flag
    or t0, t0, t1
    sw t0, fpu_use_rec_flag, t1
    li t0, 0x1
    sw t0, nest_fpu_dirty_rec, t1
fpu_restore_record_use_end:
    load_x t0, 29 * portWORD_SIZE(sp)
    and t0, t0, a0 /* a0 is SR_FS here */
    bne t0, a1, restore_fpu_context_end /* a1 is SR_FS_DIRTY here */
    /* if (FS in stack is Dirty) && (FPU has changed), restore FPU and change FS to Dirty */
    lw t0, fpu_use_rec_flag
    lw t1, FPU_CONTEXT_SIZE(s6)
    bgeu t1, t0, restore_fpu_context_end
    not t1, a0 /* a0 is SR_FS here */
    and t2, t2, t1 /* t2 is MSTATUS here */
    or s7, t2, a1 /* a1 is SR_FS_DIRTY here */
    /* restore FPU */
    mv a0, s6
    la t0, awos_arch_restore_fpu_status
    jalr t0
    csrw mstatus, s7
    j restore_fpu_context_end

nest_restore_fpu_context_end:
    /* return to thread */
    lw t0, nest_fpu_dirty_rec
    beqz t0, restore_fpu_context_end
    sw x0, nest_fpu_dirty_rec, t0
    /* if (FS in stack is Dirty), restore FPU */
    load_x t2, 29 * portWORD_SIZE(sp)
    li a0, SR_FS
    and t0, t2, a0
    li a1, SR_FS_DIRTY
    bne t0, a1, restore_fpu_context_end
    not t1, a0 /* a0 is SR_FS here */
    and t2, t2, t1 /* t2 is MSTATUS here */
    or t0, t2, a1 /* a1 is SR_FS_DIRTY here */
    csrw mstatus, t0
#endif
    csrr a0, mstatus
    call fpu_restore_inirq
restore_fpu_context_end:
#endif

#ifdef CONFIG_ARCH_RISCV_INTERRUPT_NEST
    load_x a0, interrupt_nest_cnt
    beqz a0, nest_restore_clic

    /* restore mcause and clic regs when nested */
    li t0, CLIC_MINTTHRESH_REG
    lw t1, 35 * portWORD_SIZE( sp )
    sw t1, 0(t0)

    load_x  t0, 34 * portWORD_SIZE( sp )
    csrw mcause, t0
    j nest_restore_end

nest_restore_clic:
    li t1, CLIC_MINTTHRESH_REG
    la t0, clic_thresh_save
    lw t2, 0(t0)
    sw t2, 0(t1)
    la t0, mcause_save
    load_x t1, 0(t0)
    csrw mcause, t1
nest_restore_end:
    load_x  t0, 0  * portWORD_SIZE( sp )
    csrw mepc, t0
    /* Load mstatus with the interrupt enable bits used by the task. */
    load_x  t0, 29 * portWORD_SIZE( sp )
    csrw mstatus, t0
    load_x  t0, 33 * portWORD_SIZE( sp )
    csrw mscratch, t0

#ifdef CONFIG_IRQNEST_OSTEST_GENERAL_CTX_CMP
    load_x a1, interrupt_nest_cnt
    beqz a1, ostest_exit_cmp_end
    /* use irqstack if nest_cnt > 0 */
    la t0, g_irqnest_store_addr
    store_x sp, 0(t0)
    csrr a0, mcause
    andi a0, a0, 0x7FF
    call irqnest_ostest_ctx_cmp_exit
ostest_exit_cmp_end:
#endif

    load_x t0, g_irq_stack_base
    li t1, IRQSTACK_BOTTOM_MAGIC
    beq t0, t1, irqstack_overflow_check_end
    ebreak
irqstack_overflow_check_end:

    load_x t0, interrupt_nest_cnt
    bnez t0, exit_skip_schedule
    jal  schedule_preempt_inirq
exit_skip_schedule:
    /* schedule_preempt_inirq() may execute mret for thread switch.
     * restore csr before mret, and restore mepc/mstatus after back to interrupted thread.
     */
    load_x  t0, 0  * portWORD_SIZE( sp )
    csrw mepc, t0
    load_x  t0, 29 * portWORD_SIZE( sp )
    csrw mstatus, t0

    load_x  x1 , 1  * portWORD_SIZE( sp )
    load_x  x5 , 2  * portWORD_SIZE( sp )	/* t0 */
    load_x  x6 , 3  * portWORD_SIZE( sp )	/* t1 */
    load_x  x7 , 4  * portWORD_SIZE( sp )	/* t2 */
    load_x  x8 , 5  * portWORD_SIZE( sp )	/* s0/fp */
    load_x  x9 , 6  * portWORD_SIZE( sp )	/* s1 */
    load_x  x10, 7  * portWORD_SIZE( sp )	/* a0 */
    load_x  x11, 8  * portWORD_SIZE( sp )	/* a1 */
    load_x  x12, 9  * portWORD_SIZE( sp )	/* a2 */
    load_x  x13, 10 * portWORD_SIZE( sp )	/* a3 */
    load_x  x14, 11 * portWORD_SIZE( sp )	/* a4 */
    load_x  x15, 12 * portWORD_SIZE( sp )	/* a5 */
    load_x  x16, 13 * portWORD_SIZE( sp )	/* a6 */
    load_x  x17, 14 * portWORD_SIZE( sp )	/* a7 */
    load_x  x18, 15 * portWORD_SIZE( sp )	/* s2 */
    load_x  x19, 16 * portWORD_SIZE( sp )	/* s3 */
    load_x  x20, 17 * portWORD_SIZE( sp )	/* s4 */
    load_x  x21, 18 * portWORD_SIZE( sp )	/* s5 */
    load_x  x22, 19 * portWORD_SIZE( sp )	/* s6 */
    load_x  x23, 20 * portWORD_SIZE( sp )	/* s7 */
    load_x  x24, 21 * portWORD_SIZE( sp )	/* s8 */
    load_x  x25, 22 * portWORD_SIZE( sp )	/* s9 */
    load_x  x26, 23 * portWORD_SIZE( sp )	/* s10 */
    load_x  x27, 24 * portWORD_SIZE( sp )	/* s11 */
    load_x  x28, 25 * portWORD_SIZE( sp )	/* t3 */
    load_x  x29, 26 * portWORD_SIZE( sp )	/* t4 */
    load_x  x30, 27 * portWORD_SIZE( sp )	/* t5 */
    load_x  x31, 28 * portWORD_SIZE( sp )	/* t6 */
    addi sp, sp, portCONTEXT_SIZE
    mret
#else
    jal  schedule_preempt_inirq

    li    a0, 0x8
    csrs  mstatus, a0

    ipop
#endif /* CONFIG_ARCH_RISCV_INTERRUPT_NEST */

/******************************************************************************
 * Functions:
 *     void trap(void);
 * default exception handler
 ******************************************************************************/
    .align  2
    .global trap
    .type   trap, %function
trap:
    /* Check for interrupt */
    addi    sp, sp, -4
    sw      t0, 0x0(sp)
    csrr    t0, mcause
    blt     t0, x0, .Lirq
    addi    sp, sp, 4
    la      t0, g_trap_sp

    addi    t0, t0, -132

    sw      x1,    0(t0)
    sw      x2,    4(t0)
    sw      x3,    8(t0)
    sw      x4,   12(t0)
    sw      x6,   20(t0)
    sw      x7,   24(t0)
    sw      x8,   28(t0)
    sw      x9,   32(t0)
    sw      x10,  36(t0)
    sw      x11,  40(t0)
    sw      x12,  44(t0)
    sw      x13,  48(t0)
    sw      x14,  52(t0)
    sw      x15,  56(t0)
    sw      x16,  60(t0)
    sw      x17,  64(t0)
    sw      x18,  68(t0)
    sw      x19,  72(t0)
    sw      x20,  76(t0)
    sw      x21,  80(t0)
    sw      x22,  84(t0)
    sw      x23,  88(t0)
    sw      x24,  92(t0)
    sw      x25,  96(t0)
    sw      x26, 100(t0)
    sw      x27, 104(t0)
    sw      x28, 108(t0)
    sw      x29, 112(t0)
    sw      x30, 116(t0)
    sw      x31, 120(t0)

    csrr    a0, mepc
    sw      a0, 124(t0)
    csrr    a0, mstatus
    sw      a0, 128(t0)

    mv      a0, t0
    lw      t0, -4(sp)
    mv      sp, a0
    sw      t0, 16(sp)

    jal     trap_c


.Lirq:
    lw      t0, 0x0(sp)
    addi    sp, sp, 4
    j       Default_IRQHandler

    .align  6
    .weak   Default_Handler
    .global Default_Handler
    .type   Default_Handler, %function
Default_Handler:
    /* Check for nmi */
    addi    sp, sp, -8
    sw      t0, 0x0(sp)
    sw      t1, 0x4(sp)
    csrr    t0, mcause
    andi    t0, t0, 0x3FF
    li      t1, 24
    beq     t0, t1, .NMI_Handler
    lw      t0, 0x0(sp)
    lw      t1, 0x4(sp)
    addi    sp, sp, 8
    j       trap

.NMI_Handler:
    lw      t0, 0x0(sp)
    lw      t1, 0x4(sp)
    addi    sp, sp, 8

    addi    sp, sp, -64

    sw      ra,  0(sp)
    sw      t0,  4(sp)
    sw      t1,  8(sp)
    sw      t2, 12(sp)
    sw      a0, 16(sp)
    sw      a1, 20(sp)
    sw      a2, 24(sp)
    sw      a3, 28(sp)
    sw      a4, 32(sp)
    sw      a5, 36(sp)
    sw      a6, 40(sp)
    sw      a7, 44(sp)
    sw      t3, 48(sp)
    sw      t4, 52(sp)
    sw      t5, 56(sp)
    sw      t6, 60(sp)

#ifdef CONFIG_ARCH_RISCV_FPU
    addi    sp,    sp, -(20*FREGBYTES)
    FSTORE  ft0,   0 * FREGBYTES(sp)
    FSTORE  ft1,   1 * FREGBYTES(sp)
    FSTORE  ft2,   2 * FREGBYTES(sp)
    FSTORE  ft3,   3 * FREGBYTES(sp)
    FSTORE  ft4,   4 * FREGBYTES(sp)
    FSTORE  ft5,   5 * FREGBYTES(sp)
    FSTORE  ft6,   6 * FREGBYTES(sp)
    FSTORE  ft7,   7 * FREGBYTES(sp)
    FSTORE  fa0,   8 * FREGBYTES(sp)
    FSTORE  fa1,   9 * FREGBYTES(sp)
    FSTORE  fa2,  10 * FREGBYTES(sp)
    FSTORE  fa3,  11 * FREGBYTES(sp)
    FSTORE  fa4,  12 * FREGBYTES(sp)
    FSTORE  fa5,  13 * FREGBYTES(sp)
    FSTORE  fa6,  14 * FREGBYTES(sp)
    FSTORE  fa7,  15 * FREGBYTES(sp)
    FSTORE  ft8,  16 * FREGBYTES(sp)
    FSTORE  ft9,  17 * FREGBYTES(sp)
    FSTORE  ft10, 18 * FREGBYTES(sp)
    FSTORE  ft11, 19 * FREGBYTES(sp)
#endif

    la      t0, g_nmivector
    lw      t0, (t0)
    jalr    t0

#ifdef CONFIG_ARCH_RISCV_FPU
    FLOAD   ft0,   0 * FREGBYTES(sp)
    FLOAD   ft1,   1 * FREGBYTES(sp)
    FLOAD   ft2,   2 * FREGBYTES(sp)
    FLOAD   ft3,   3 * FREGBYTES(sp)
    FLOAD   ft4,   4 * FREGBYTES(sp)
    FLOAD   ft5,   5 * FREGBYTES(sp)
    FLOAD   ft6,   6 * FREGBYTES(sp)
    FLOAD   ft7,   7 * FREGBYTES(sp)
    FLOAD   fa0,   8 * FREGBYTES(sp)
    FLOAD   fa1,   9 * FREGBYTES(sp)
    FLOAD   fa2,  10 * FREGBYTES(sp)
    FLOAD   fa3,  11 * FREGBYTES(sp)
    FLOAD   fa4,  12 * FREGBYTES(sp)
    FLOAD   fa5,  13 * FREGBYTES(sp)
    FLOAD   fa6,  14 * FREGBYTES(sp)
    FLOAD   fa7,  15 * FREGBYTES(sp)
    FLOAD   ft8,  16 * FREGBYTES(sp)
    FLOAD   ft9,  17 * FREGBYTES(sp)
    FLOAD   ft10, 18 * FREGBYTES(sp)
    FLOAD   ft11, 19 * FREGBYTES(sp)

    addi    sp, sp, (20 * FREGBYTES)
#endif

    lw      ra,  0(sp)
    lw      t0,  4(sp)
    lw      t1,  8(sp)
    lw      t2, 12(sp)
    lw      a0, 16(sp)
    lw      a1, 20(sp)
    lw      a2, 24(sp)
    lw      a3, 28(sp)
    lw      a4, 32(sp)
    lw      a5, 36(sp)
    lw      a6, 40(sp)
    lw      a7, 44(sp)
    lw      t3, 48(sp)
    lw      t4, 52(sp)
    lw      t5, 56(sp)
    lw      t6, 60(sp)

    addi    sp, sp, 64
    mret


    .size   Default_Handler, . - Default_Handler

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
    .macro  def_irq_handler handler_name
    .weak   \handler_name
    .globl  \handler_name
    .set    \handler_name, Default_Handler
    .endm

    def_irq_handler PendSV_Handler
    def_irq_handler SysTick_Handler
    def_irq_handler STIM0_IRQHandler
    def_irq_handler STIM1_IRQHandler
    def_irq_handler STIM2_IRQHandler
    def_irq_handler STIM3_IRQHandler
    def_irq_handler TIM0_IRQHandler
    def_irq_handler TIM1_IRQHandler
    def_irq_handler TIM2_IRQHandler
    def_irq_handler TIM3_IRQHandler
    def_irq_handler USART_IRQHandler
    def_irq_handler GPIO0_IRQHandler
    def_irq_handler GPIO1_IRQHandler
    def_irq_handler GPIO2_IRQHandler
    def_irq_handler GPIO3_IRQHandler
    def_irq_handler GPIO4_IRQHandler
    def_irq_handler GPIO5_IRQHandler
    def_irq_handler GPIO6_IRQHandler
    def_irq_handler GPIO7_IRQHandler
    def_irq_handler PAD_IRQHandler
    def_irq_handler TIM6_IRQHandler
    def_irq_handler TIM7_IRQHandler
    def_irq_handler TIM8_IRQHandler
    def_irq_handler TIM9_IRQHandler
    def_irq_handler TIM10_IRQHandler
    def_irq_handler TIM11_IRQHandler

