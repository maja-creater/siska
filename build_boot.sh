#!/bin/bash

as boot.s --32 -o boot.o
as init.s --32 -o init.o
as head.s --32 -o head.o
as siska_syscall.s --32 -o siska_syscall.o
as siska_page.s    --32 -o siska_page.o

gcc -c -O3 -m32 main.c -fno-stack-protector
gcc -c -O3 -m32 siska_mm.c -fno-stack-protector
gcc -c -O3 -m32 siska_task.c -fno-stack-protector
gcc -c -O3 -m32 siska_printf.c  -fno-stack-protector
gcc -c -O3 -m32 siska_console.c -fno-stack-protector
gcc -c -O3 -m32 siska_api.c     -fno-stack-protector

FILES="head.o \
	   main.o \
	   siska_syscall.o \
	   siska_task.o \
	   siska_mm.o siska_page.o \
	   siska_printf.o siska_console.o \
	   siska_api.o"

ld -static -melf_i386 -Ttext 0x00000000 $FILES -o siska

#objdump -d boot.o -M i8086
#objdump -d init.o -M i8086
#objdump -d head.o -M i8086
objdump -d siska -M i386 > siska.txt

objcopy -S -O binary boot.o  boot.bin
objcopy -S -O binary init.o  init.bin
objcopy -S -O binary siska siska.bin

dd if=boot.bin of=../bochs/a.img bs=512 count=1 conv=notrunc
dd if=init.bin of=../bochs/a.img bs=512 count=1 conv=notrunc seek=1
dd if=siska.bin of=../bochs/a.img bs=512 conv=notrunc seek=2

