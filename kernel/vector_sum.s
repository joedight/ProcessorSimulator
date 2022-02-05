	.file	"tight_loop.c"
	.option nopic
	.attribute arch, "rv32i2p0"
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.text
	.section	.rodata.str1.4,"aMS",@progbits,1
	.align	2
.LC0:
	.string	"Bench name: Vector Sum"
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

	li a3,16384
	li a0, 0

# Random mem in heap.
	addi a0, tp, 0

	beqz a3, loop_end
	slli a3,a3,0x2		# len *= 4
	add     a3,a0,a3	# end = a + len
loop_start:
	lw      a5,0(a0)	# a5 <- *a
	addi    a0,a0,4		# a += 4
	add     a1,a1,a5	# sum += a5
	bne     a0,a3,loop_start# while (a != end)
loop_end:

# End bench
	addi t3, zero, 6
	ebreak
# Quit
	addi t3, zero, 2
	ebreak

	ret
