#include"siska_vfs.h"
#include"siska_mm.h"

int memory_dev_open(siska_file_t* file, int flags, unsigned int mode)
{
	if (file && file->dev) {

		if (file->dev->priv && file->dev->priv_size > 0) {
			file->dev->priv_pos = 0;
			return 0;
		}
	}
	return -1;
}

int memory_dev_close(siska_file_t* file)
{
	return 0;
}

int memory_dev_read(siska_file_t* file, void* buf, int size)
{
	if (!file || !buf || size < 0)
		return -1;

	siska_dev_t* dev = file->dev;

	if (!dev || !dev->priv || dev->priv_size <= 0)
		return -1;

	int len = dev->priv_size - dev->priv_pos;
	if (len < 0)
		return -1;

	len = size > len ? len : size;

	siska_memcpy(buf, dev->priv + dev->priv_pos, len);
	dev->priv_pos += len;

	return len;
}

int memory_dev_write(siska_file_t* file, const void* buf, int size)
{
	if (!file || !buf || size < 0)
		return -1;

	siska_dev_t* dev = file->dev;

	if (!dev || !dev->priv || dev->priv_size <= 0)
		return -1;

	int len = dev->priv_size - dev->priv_pos;
	if (len < 0)
		return -1;

	len = size > len ? len : size;

	siska_memcpy(dev->priv + dev->priv_pos, buf, len);
	dev->priv_pos += len;

	return len;
}

int memory_dev_lseek(siska_file_t* file, long offset, int whence)
{
	if (!file || !file->dev || offset < 0 || SISKA_SEEK_SET != whence)
		return -1;

	file->dev->priv_pos = offset;
	return 0;
}

int memory_dev_sync(siska_file_t* file)
{
	return 0;
}

int memory_dev_ioctl(siska_file_t* file, unsigned long cmd, ...)
{
	return 0;
}


siska_file_ops_t siska_fops_memory_dev = 
{
	.type  = "memory_dev",

	.next  = NULL,

	.open  = memory_dev_open,
	.close = memory_dev_close,

	.read  = memory_dev_read,
	.write = memory_dev_write,

	.lseek = memory_dev_lseek,
	.sync  = memory_dev_sync,

	.ioctl = memory_dev_ioctl,
};

