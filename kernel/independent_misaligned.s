	.file	"tight_loop.c"
	.option nopic
	.attribute arch, "rv32i2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align	2
.LC0:
	.string	"Bench name: Independent"
	.text
	.align	2
	.globl	main
	.type	main, @function
main:
# Print name
	lui	a5,%hi(.LC0)
	addi	a5,a5,%lo(.LC0)
	addi t3, zero, 4
	addi t4, a5, 0
	ebreak
# Bench start
	addi t3, zero, 5
	ebreak

	li a0,16384

loop_start:
	addi a2, a2, 1
	li a1, 0
	li a1, 0
	li a1, 0

	li a1, 0
	li a1, 0
	li a1, 0
	li a1, 0

	li a1, 0
	li a1, 0
	li a1, 0
	li a1, 0

	bne	a0, a2, loop_start

# End bench
	addi t3, zero, 6
	ebreak
# Quit
	addi t3, zero, 2
	ebreak

	ret
