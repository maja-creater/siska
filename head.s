
.code32
.text
.global _start, _task0, _tss0, _idt
.global siska_printk, _main

.align 4
_start:
_pg_dir:

	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	lss  _init_stack, %esp

	call _setup_idt
	call _setup_gdt

	movl $0x10, %eax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	lss  _init_stack, %esp

	xorl %eax, %eax
1:
	incl %eax
	movl %eax, 0x000000
	cmpl %eax, 0x100000
	je   1b

	movl %cr0, %eax
	andl $0x80000011, %eax
	orl  $0x2, %eax
	movl %eax, %cr0

	call _check_x87
	jmp  _after_page_tables

_check_x87:
	fninit
	fstsw %ax
	cmpb  $0, %al
	je    1f

	movl  %cr0, %eax
	xorl  $0x6, %eax
	movl  %eax, %cr0
	ret
.align 4
1:
	.byte 0xdb, 0xe4
	ret

.align 4
_setup_idt:
	lea _ignore_int_handler, %edx

	movl $0x00080000, %eax
	movw %dx, %ax
	movw $0x8e00, %dx

	lea  _idt, %edi
	movl $256, %ecx
1:
	movl %eax, (%edi)
	movl %edx, 4(%edi)
	addl $8, %edi
	decl %ecx
	jne  1b

	lidt _idt_desc
	ret

.align 4
_setup_gdt:
	lgdt _gdt_desc
	ret

.org 0x1000
_pg_table0:

.org 0x5000
_task0:
	.fill 4096, 1, 0
_init_stack:
	.long _init_stack
	.word 0x10
.align 4
_tmp_floppy_area:
	.fill 1024, 1, 0

.align 4
_after_page_tables:
	pushl $0
	pushl $0
	pushl $0
	pushl $_L6
	pushl $_main

	jmp  _setup_pages
_L6:
	jmp _L6

.align 4
_setup_pages:
	movl $1024 * 5, %ecx
	xorl %eax, %eax
	xorl %edi, %edi
	cld
	rep  stosl

	xorl %edi, %edi
	movl $0x1007,   (%edi)
	movl $0x2007,  4(%edi)
	movl $0x3007,  8(%edi)
	movl $0x4007, 12(%edi)

	movl $0x0007, %eax
	movl $0x1000, %edi
1:
	movl %eax, (%edi)
	addl $0x1000, %eax
	addl $4, %edi
	cmpl $0x1000007, %eax
	jl   1b

	xorl %eax, %eax
	movl %eax, %cr3
	movl %cr0, %eax
	orl  $0x80000000, %eax
	movl %eax, %cr0
	ret

.align 4
_init_msg:
.asciz "unknown interrupt!\n"

.align 4
_ignore_int_handler:
	pushl %eax
	pushl %ecx
	pushl %edx
	push  %ds
	push  %es
	push  %fs

	movl  $0x10, %eax
	mov   %ax, %ds
	mov   %ax, %es
	mov   %ax, %fs

	pushl $_init_msg
	call  siska_printk
	popl  %eax

	pop   %fs
	pop   %es
	pop   %ds
	popl  %edx
	popl  %ecx
	popl  %eax
	iret

.align 4
.word  0
_idt_desc:
	.word 256 * 8 - 1
	.long _idt

.align 4
.word  0
_gdt_desc:
	.word 256 * 8 - 1
	.long _gdt

.align 4
_idt:
	.fill 256, 8, 0

_gdt:
	.word 0x0000, 0x0000, 0x0000, 0x0000 # 0    not use

	.word 0xffff, 0x0000, 0x9a00, 0x00cf # 0x8  kernel code
	.word 0xffff, 0x0000, 0x9200, 0x00cf # 0x10 kernel data

	.word 0x0000, 0x0000, 0x0000, 0x0000 # 0x18 not use

	.word 0x0068, _tss0,  0xe900, 0x0000 # 0x20 tss0
	.word 0x0040, _ldt0,  0xe200, 0x0000 # 0x28 ldt0

	.fill 250, 8, 0
_ldt0:
	.word 0x0000, 0x0000, 0x0000, 0x0000 # 0    not use
	.word 0xffff, 0x0000, 0xfa00, 0x00cf # 0xf  user code
	.word 0xffff, 0x0000, 0xf200, 0x00cf # 0x17 user data
_tss0:
	.fill 104, 1, 0

