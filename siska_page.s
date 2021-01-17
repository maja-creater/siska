
.code32
.text
.global _page_fault_int_handler, _do_page_fault

.align 4
_page_fault_msg:
.asciz "page fault interrupt!\n"

.align 4
_page_fault_int_handler:
#40  cs
#36  eip
#32  error_code
#28  ds
#24  es
#20  fs
#16  gs
#12  eax
#8   ebx
#4   ecx
#0   edx

	cli

	push  %ds
	push  %es
	push  %fs
	push  %gs

	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx

	movl  $0x10, %eax
	mov   %ax, %ds
	mov   %ax, %es
	mov   %ax, %fs
	mov   %ax, %gs

	movl  40(%esp), %edx
	movl  32(%esp), %ecx
	movl  %cr2,     %eax
	sti

	andl  $0x3, %edx

	pushl %edx
	pushl %ecx
	pushl %eax
	call  _do_page_fault
	addl  $12, %esp

	popl  %edx
	popl  %ecx
	popl  %ebx
	popl  %eax

	pop   %gs
	pop   %fs
	pop   %es
	pop   %ds

	addl  $4, %esp #drop error code
	iret

