.extern interrupt_entry

.section .vectors, "aw", @progbits
	.balign 64
	.globl  clic_interrupt_vectors
	.type   clic_interrupt_vectors, @object
	clic_interrupt_vectors:
	.rept   (CONFIG_IRQ_SOUCE_COUNT)
	.long   clic_interrupt_handler
	.endr

.text
	.global clic_interrupt_handler
	.type   clic_interrupt_handler, %function
clic_interrupt_handler:
	addi sp, sp, -(16 * 4)
	sw ra, 0  * 4( sp )
	sw t0, 1  * 4( sp )
	sw t1, 2  * 4( sp )
	sw t2, 3  * 4( sp )
	sw t3, 4  * 4( sp )
	sw t4, 5  * 4( sp )
	sw t5, 6  * 4( sp )
	sw t6, 7  * 4( sp )
	sw a0, 8  * 4( sp )
	sw a1, 9  * 4( sp )
	sw a2, 10 * 4( sp )
	sw a3, 11 * 4( sp )
	sw a4, 12 * 4( sp )
	sw a5, 13 * 4( sp )
	sw a6, 14 * 4( sp )
	sw a7, 15 * 4( sp )

	addi sp, sp, -(20 * 8)
	fsd ft0,  0  * 8( sp )
	fsd ft1,  1  * 8( sp )
	fsd ft2,  2  * 8( sp )
	fsd ft3,  3  * 8( sp )
	fsd ft4,  4  * 8( sp )
	fsd ft5,  5  * 8( sp )
	fsd ft6,  6  * 8( sp )
	fsd ft7,  7  * 8( sp )
	fsd ft8,  8  * 8( sp )
	fsd ft9,  9  * 8( sp )
	fsd ft10, 10 * 8( sp )
	fsd ft11, 11 * 8( sp )
	fsd fa0,  12 * 8( sp )
	fsd fa1,  13 * 8( sp )
	fsd fa2,  14 * 8( sp )
	fsd fa3,  15 * 8( sp )
	fsd fa4,  16 * 8( sp )
	fsd fa5,  17 * 8( sp )
	fsd fa6,  18 * 8( sp )
	fsd fa7,  19 * 8( sp )

	call interrupt_entry

	fld ft0,  0  * 8( sp )
	fld ft1,  1  * 8( sp )
	fld ft2,  2  * 8( sp )
	fld ft3,  3  * 8( sp )
	fld ft4,  4  * 8( sp )
	fld ft5,  5  * 8( sp )
	fld ft6,  6  * 8( sp )
	fld ft7,  7  * 8( sp )
	fld ft8,  8  * 8( sp )
	fld ft9,  9  * 8( sp )
	fld ft10, 10 * 8( sp )
	fld ft11, 11 * 8( sp )
	fld fa0,  12 * 8( sp )
	fld fa1,  13 * 8( sp )
	fld fa2,  14 * 8( sp )
	fld fa3,  15 * 8( sp )
	fld fa4,  16 * 8( sp )
	fld fa5,  17 * 8( sp )
	fld fa6,  18 * 8( sp )
	fld fa7,  19 * 8( sp )
	addi sp, sp, (20 * 8)

	lw ra, 0  * 4( sp )
	lw t0, 1  * 4( sp )
	lw t1, 2  * 4( sp )
	lw t2, 3  * 4( sp )
	lw t3, 4  * 4( sp )
	lw t4, 5  * 4( sp )
	lw t5, 6  * 4( sp )
	lw t6, 7  * 4( sp )
	lw a0, 8  * 4( sp )
	lw a1, 9  * 4( sp )
	lw a2, 10 * 4( sp )
	lw a3, 11 * 4( sp )
	lw a4, 12 * 4( sp )
	lw a5, 13 * 4( sp )
	lw a6, 14 * 4( sp )
	lw a7, 15 * 4( sp )
	addi sp, sp, (16 * 4)

	mret
