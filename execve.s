
.code32
.text
.global _start

.align 4
_start:
	jmp 2f
1:
	popl %ebx
	movl $3, %eax
	movl $9, %ecx
	int  $0x80

	movl $6, %eax
	int  $0x80
2:
	call 1b
_exec_msg:
.asciz "exec ok!\n"

