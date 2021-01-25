
CFILES += siska_vfs.c
CFILES += siska_fs0.c
CFILES += siska_printf.c
CFILES += siska_fops_memory_dev.c

all:
	gcc -g -O3 $(CFILES)
