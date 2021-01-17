
#include"siska_task.h"
#include"siska_mm.h"

static int console_x = 0;
static int console_y = 0;

static int console_rows = 25;
static int console_cols = 80;

void siska_console_init()
{
	console_x = 0;
	console_y = 0;

	console_rows = 25;
	console_cols = 80;
}

int siska_console_write(const char* fmt)
{
	unsigned char* buf = (unsigned char*)0xb8000;
	unsigned char* p   = (unsigned char*)fmt;

	unsigned long flags;
	siska_irqsave(flags);
	while (*p) {

		if (console_x >= console_cols) {
			console_x =  0;
			console_y++;

			if (console_y >= console_rows)
				console_y =  0;
		}

		if ('\n' == *p) {
			console_x = 0;
			console_y++;
			p++;
			continue;
		}

		int pos = console_y * console_cols + console_x;

		buf[pos * 2]     = *p++;
		buf[pos * 2 + 1] = 0x7f;

		console_x++;
	}
	siska_irqrestore(flags);

	return (int)(p - (unsigned char*)fmt);
}

int siska_printk(const char* fmt, ...)
{
	char buf[128];

	siska_va_list ap;

	siska_va_start(ap, fmt);
	int n = siska_vsnprintf(buf, sizeof(buf), fmt, ap);
	siska_va_end(ap);

	if (n > 0)
		return siska_console_write(buf);
	return n;
}

