
.code32
.text
.global _timer_int_handler, _syscall_handler, _ret_from_fork, _syscall_table
.global _jiffies, _do_timer, _printk
.global _fork0_msg, _fork1_msg

.align 4
_timer_msg:
.asciz "timer interrupt!\n"

.align 4
_timer_int_handler:
#48  old ss
#44  old esp
#40  eflags
#36  cs
#32  eip
#28  ds
#24  es
#20  fs
#16  gs
#12  edx
#8   ecx
#4   ebx
#0   eax

	push  %ds
	push  %es
	push  %fs
	push  %gs

	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	movl  $0x10, %eax
	mov   %ax, %ds
	mov   %ax, %es
	mov   %ax, %fs
	mov   %ax, %gs

	incl  _jiffies

	movb  $0x20, %al
	outb  %al, $0x20

#	pushl $_timer_msg
#	call  _printk
#	addl  $4, %esp

	movl  36(%esp), %eax
	testl $0x3, %eax
	je    1f

	call  _do_timer

1:
	popl  %eax
_ret_from_syscall:
	popl  %ebx
	popl  %ecx
	popl  %edx

	pop   %gs
	pop   %fs
	pop   %es
	pop   %ds
	iret

.align 4
_syscall_msg:
.asciz "syscall interrupt!\n"

.align 4
_syscall_handler:
	push  %ds
	push  %es
	push  %fs
	push  %gs

	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax

	movl  $0x10, %eax
	mov   %ax, %ds
	mov   %ax, %es
	mov   %ax, %fs
	mov   %ax, %gs

	popl  %eax
	movl  _syscall_table(, %eax, 4), %eax

	pushl %esp
	call  *%eax
	popl  %esp
	jmp   _ret_from_syscall

_ret_from_fork:
	xorl %eax, %eax
	jmp  _ret_from_syscall

.align 4
_syscall_table:
	.fill 256, 4, 0

_fork0_msg:
.asciz "fork0 process!\n"
_fork1_msg:
.asciz "fork1 process!\n"
