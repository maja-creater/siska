
BOOT_SEG       = 0x07c0

INIT_SEG       = 0x9000
INIT_OFFSET    = 0x0200
INIT_SECTIONS  = 1
INIT_SECTION_START = 1

SYS_SEG       = 0x1000
SYS_SECTIONS  = 105
SYS_SECTION_START = 2

.code16
.text
.global _start

.align 4
_start:
	ljmp $BOOT_SEG, $_start2

_start2:
	movw %cs, %ax
	movw %ax, %ds

	movw $INIT_SEG, %ax
	movw %ax, %es

	xorw %si, %si
	xorw %di, %di
	movw $0x100, %cx
	rep movsw

	ljmp $INIT_SEG, $_read
_read:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0x1000, %sp

#read init code
	movw $INIT_OFFSET,   %bx
	movw $INIT_SECTION_START, %cx
	movw $INIT_SECTIONS, %dx
	call _read_floppy

#read sys code
	movw $SYS_SEG, %ax
	xorw %bx, %bx
	movw $SYS_SECTION_START, %cx
	movw $SYS_SECTIONS, %dx
	call _read_floppy

	ljmp $INIT_SEG, $INIT_OFFSET

_read_error:
	movw $_msg_err, %ax
	movw $8, %cx
	call _print
1:
	jmp 1b

.align 4
_nb_sections:
	.word 0
_cylind:
	.byte 0
_section:
	.byte 0
_head:
	.byte 0
.align 4
_read_floppy:
#BIOS int 13H
#ah = 02H
#al = section count
#ch = cylind  number
#cl = section number start
#dh = head    number
#dl = driver  number
#es:bx = buffer
#return value: cf  = 0 ok,    ah = 0, al = section count
#return value: cf != 0 error, ah = error number

#input args:
#ax, buffer seg
#bx, buffer offset
#cx, start section
#dx, total sections

	cmpw $1024, %dx
	jl   1f 

	movw $_msg_kernel_too_big, %ax
	movw $14, %cx
	call _print
_too_big:
	jmp _too_big

1:
	movw %dx, _nb_sections
	movw %ax, %es

#floppy 80 cylinds, 2 head, 18 sections, we count section No. from 0
#get start cylind, head, section.
	movw %cx, %ax
	movb $36, %cl
	divb %cl
	movb %al, _cylind

	shrw $8,  %ax
	movb $18, %cl
	divb %cl
	movb %al, _head
	movb %ah, _section

_rep_read:
	movb _section, %cl
	movb $18, %al
	subb %cl, %al
	addb $1, %cl
	xorb %ah, %ah

	cmpw %ax, _nb_sections
	jge  2f
	movw _nb_sections, %ax
2:
	movw %bx, %dx
	shrw $9,  %dx
	addw %ax, %dx
	subw $128, %dx
	jle  3f
	movb %dl, %al
3:
	movb $0x2, %ah

	movb _cylind, %ch
	movb _head, %dh
	movb $0, %dl
	int  $0x13
	jc   _read_error

	movb $0, %ah
	subw %ax, _nb_sections
	je   _read_ok

	movw %ax, %dx
	shlw $9,  %dx

	addb %al, _section
	cmpb $18, _section
	jl   4f

	movb $0, _section
	addb $1, _head
	cmpb $2, _head
	jl   4f

	movb $0, _head
	addb $1, _cylind
4:
	addw %dx, %bx
	jnc  _rep_read

	movw %es, %ax
	addw $0x1000, %ax
	movw %ax, %es
	xorw %bx, %bx
	jmp  _rep_read

_read_ok:
	movw $_msg_ok, %ax
	movw $7, %cx
	call _print
	ret

.align 4
_print:
#ah = 13h
#al = show mode
#bh = page No.
#bl = priority
#cx = string len
#dh = row
#dl = col
#es:bp = string addr
#int 10h

	movw %ax, %bp
	movw %cs, %ax
	movw %ax, %es

	movb $0x13, %ah
	movb $0x0,  %al
	movb $0x0,  %bh
	movb $0x8f, %bl
	movw $0x0,  %dx
	int  $0x10
	ret

.align 4
_msg_ok:
.asciz "read ok"

.align 4
_msg_err:
.asciz "read err"

.align 4
_msg_kernel_too_big:
.asciz "kernel too big"

.org 500
_rootfs_data:
.word 0, 0
_rootfs_size:
.word 0, 0
.word 0

.byte 0x55
.byte 0xAA
