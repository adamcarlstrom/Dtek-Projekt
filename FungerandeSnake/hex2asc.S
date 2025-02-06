  # hexmain.S
  # Written 2015-09-04 by F Lundevall
  # Copyright abandonded - this file is in the public domain.

	.text
	.globl hex2asc

hex2asc:
	li	a0, 12		# test number (from 0 to 15)
	
	addi    sp,sp,-4
	sw      ra,0(sp)
	
	jal	hexasc		# call hexasc
	
	li	a7, 11	# write a0 to stdout
	ecall

	lw      ra,0(sp)
	addi    sp,sp,4
	jr      ra	
	
hexasc:
	li t1, 0xF
	and a0, a0, t1
	
	li t2, 9
	bge t2, a0, small
		addi a0, a0, 0x07
	small:
		addi a0, a0, 0x030

	jr ra
  # You can write your own code for hexasc here
  #
