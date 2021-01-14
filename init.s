
INIT_SEG     = 0x9000
INIT_OFFSET  = 0x0200

SYS_SEG      = 0x1000

DATA_OFFSET_VGA_CUR_COL   = 0x0
DATA_OFFSET_VGA_CUR_ROW   = 0x1

DATA_OFFSET_MEM_SIZE  = 0x2

DATA_OFFSET_VGA_CUR_PAGE  = 0x5
DATA_OFFSET_VGA_CUR_MODE  = 0x6
DATA_OFFSET_VGA_CUR_WIDTH = 0x7

DATA_OFFSET_VGA_SIZE  = 0xa
DATA_OFFSET_VGA_MODE  = 0xb
DATA_OFFSET_VGA_PARAM = 0xc
DATA_OFFSET_VGA_ROW   = 0xe
DATA_OFFSET_VGA_COL   = 0xf

.code16
.text
.global _start

.align 4
_start:

#get extended memory size > 1M, based on KB
	movb $0x88, %ah
	int  $0x15
	movw %ax, (DATA_OFFSET_MEM_SIZE)

#get VGA params
	movb $0x12, %ah
	movb $0x10, %bl
	int  $0x10
	movb %bl, (DATA_OFFSET_VGA_SIZE)
	movb %bh, (DATA_OFFSET_VGA_MODE)
	movw %cx, (DATA_OFFSET_VGA_PARAM)
	movw $0x5019, %ax
	movb %al, (DATA_OFFSET_VGA_ROW)
	movb %ah, (DATA_OFFSET_VGA_COL)

	movb $0x03, %ah
	xorb %bh,   %bh
	int  $0x10
	movb %dl, (DATA_OFFSET_VGA_CUR_COL)
	movb %dh, (DATA_OFFSET_VGA_CUR_ROW)

	movb $0x0f, %ah
	int  $0x10
	movb %al, (DATA_OFFSET_VGA_CUR_MODE)
	movb %ah, (DATA_OFFSET_VGA_CUR_WIDTH)
	movb %bh, (DATA_OFFSET_VGA_CUR_PAGE)

	cli
	xorw %ax, %ax
	cld
_mov_sys:
	movw %ax, %es

	addw $SYS_SEG,  %ax
	cmpw $INIT_SEG, %ax
	jz   _end_mov_sys

	movw %ax, %ds
	xorw %si, %si
	xorw %di, %di
	movw $0x8000, %cx
	rep  movsw
	jmp  _mov_sys

_end_mov_sys:

	movw $INIT_SEG, %ax
	movw %ax, %ds

#load global description table & interrupt description table
	lidt _idt_48 + INIT_OFFSET
	lgdt _gdt_48 + INIT_OFFSET

#enable A20
	call _empty_8042

	movb $0xd1, %al
	out  %al,   $0x64
	call _empty_8042

	movb $0xdf, %al
	out  %al,   $0x60
	call _empty_8042

#set 8259 interrupt chips, 0x20-0x21 for master, 0xa0-0xa1 for slave

#start programming
	movb $0x11, %al
	out  %al,   $0x20
	.word 0x00eb, 0x00eb
	out  %al,   $0xa0
	.word 0x00eb, 0x00eb

#interrupt NO. from 0x20 in master
	movb $0x20, %al
	out  %al,   $0x21
	.word 0x00eb, 0x00eb

#interrupt NO. from 0x28 in slave
	movb $0x28, %al
	out  %al,   $0xa1
	.word 0x00eb, 0x00eb

#master irq 2 connect to slave
	movb $0x04, %al
	out  %al,   $0x21
	.word 0x00eb, 0x00eb

#slave connect to master's irq 2
	movb $0x02, %al
	out  %al,   $0xa1
	.word 0x00eb, 0x00eb

	movb $0x01, %al
	out  %al,   $0x21
	.word 0x00eb, 0x00eb
	out  %al,   $0xa1
	.word 0x00eb, 0x00eb

#disable all interrupts of master and slave
	movb $0xff, %al
	out  %al,   $0x21
	.word 0x00eb, 0x00eb
	out  %al,   $0xa1
	.word 0x00eb, 0x00eb

	movw $0x1, %ax
	lmsw %ax
	ljmp $8, $0

1:
	jmp 1b

.align 4
_empty_8042:
	.word 0x00eb, 0x00eb
	in   $0x64, %al
	test $0x2,  %al
	jnz  _empty_8042
	ret

.align 16
_gdt:
	.word 0, 0, 0, 0

	.word 0x07ff
	.word 0x0000
	.word 0x9a00
	.word 0x00c0

	.word 0x07ff
	.word 0x0000
	.word 0x9200
	.word 0x00c0

.align 16
_idt_48:
	.word 0, 0, 0

.align 16
_gdt_48:
	.word 0x800
	.word INIT_OFFSET + _gdt
	.word 0x9

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
_init_msg:
.asciz "init ok"
