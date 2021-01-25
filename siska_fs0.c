#include"siska_vfs.h"
#include"siska_fs.h"
#include"siska_mm.h"

extern siska_vfs_t siska_fs_siska0;

int siska0_find_in_dir(siska_dir_dev_t* dir_dev, siska_inode_dev_t* inode_dev, const char* filename, siska_fs_t* fs)
{
	unsigned long block_size;
	unsigned long offset;
	unsigned long i;

	block_size = 1024 << fs->sblock->sblock_dev->block_size;

	offset = SISKA_FS0_BLOCK_START + inode_dev->block_offset * block_size;

	int ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
	if (ret < 0)
		return -1;

	for (i = 0; i < inode_dev->size; ) {

		ret = fs->dev->fops->read(fs->dir, dir_dev, sizeof(siska_dir_dev_t));
		if (ret != sizeof(siska_dir_dev_t))
			return -1;
		i += ret;

		if (i + dir_dev->name_len > inode_dev->size)
			return -1;

		char* name = siska_kmalloc(dir_dev->name_len);
		if (!name)
			return -1;

		ret = fs->dev->fops->read(fs->dir, name, dir_dev->name_len);
		if (ret != dir_dev->name_len) {
			siska_kfree(name);
			return -1;
		}
		i += ret;

		if (!siska_strcmp(filename, name)) {
			siska_kfree(name);
			break;
		}

		siska_kfree(name);
	}

	if (i >= inode_dev->size)
		return -404;
	return 0;
}

int siska0_read_inode(siska_file_t* file, siska_fs_t* fs, siska_inode_dev_t* inode_dev, unsigned long index)
{
	siska_inode_t* inode;
	unsigned long  offset;

	offset = SISKA_FS0_INODE_START + index * sizeof(siska_inode_dev_t);

	int ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
	if (ret < 0)
		return -1;

	ret = fs->dev->fops->read(fs->dir, inode_dev, sizeof(siska_inode_dev_t));
	if (ret != sizeof(siska_inode_dev_t))
		return -1;

	inode = siska_inode_alloc();
	if (!inode)
		return -1;

	inode->file      = file;
	inode->sblock    = fs->sblock;
	inode->index     = index;
	inode->inode_dev = inode_dev;

	file->inode = inode;
	return 0;
}

static unsigned long __siska0_find_blocks(siska_sblock_dev_t* sblock_dev, unsigned int nblocks)
{
	unsigned long block_index = 0;
	unsigned long k = 0;
	unsigned long i;
	unsigned long j;

	for (i = 0; i < sizeof(sblock_dev->block_map); i++) {

		for (j = 0; j < 8; j++) {

			if (sblock_dev->block_map[i] & (1u << j)) {
				k = 0;
				break;
			}

			if (0 == k)
				block_index = i * 8 + j;

			if (++k == nblocks)
				return block_index;
		}
	}
	return 0;
}

static void __siska0_blocks_set_used(siska_sblock_dev_t* sbd, unsigned long block_index, unsigned int nblocks)
{
	unsigned long i;
	unsigned long j;
	unsigned long k;

	for (k = 0; k < nblocks; k++) {
		i = (block_index + k) >> 3;
		j = (block_index + k) & 0x7;

		sbd->block_map[i] |= 1u << j;
	}
}

static void __siska0_blocks_set_free(siska_sblock_dev_t* sbd, unsigned long block_index, unsigned int nblocks)
{
	unsigned long i;
	unsigned long j;
	unsigned long k;

	for (k = 0; k < nblocks; k++) {
		i = (block_index + k) >> 3;
		j = (block_index + k) & 0x7;

		sbd->block_map[i] &= ~(1u << j);
	}
}

static int __siska0_realloc_blocks(siska_fs_t* fs, siska_sblock_dev_t* sbd, siska_inode_dev_t* inode_dev, unsigned long append_size)
{
	unsigned long block_size = 1024 << sbd->block_size;

	unsigned long empty_len  = inode_dev->block_count * block_size - inode_dev->size;

	if (empty_len >= append_size)
		return 0;

	empty_len = append_size - empty_len;

	unsigned long nblocks     = (empty_len + block_size - 1) / block_size;

	unsigned long block_index = inode_dev->block_offset + inode_dev->block_count;
	unsigned long i;
	unsigned long j;
	unsigned long k;

	for (k = 0; k < nblocks; k++) {
		i = (block_index + k) / 8;
		j = (block_index + k) % 8;

		if (sbd->block_map[i] & (1u << j))
			break;
	}

	if (k == nblocks) {

		__siska0_blocks_set_used(sbd, block_index, nblocks);

		inode_dev->block_count += nblocks;
		return 0;
	}

	nblocks += inode_dev->block_count;

	block_index = __siska0_find_blocks(sbd, nblocks);
	if (0 == block_index)
		return -1;

	char* buf = siska_kmalloc(1024);
	if (!buf)
		return -1;

	for (i = 0; i < inode_dev->size; ) {

		unsigned long offset;

		offset  = SISKA_FS0_BLOCK_START + inode_dev->block_offset * block_size;
		offset += i;
		int ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
		if (ret < 0) {
			siska_kfree(buf);
			return -1;
		}

		k = inode_dev->size - i;
		k = k > 1024 ? 1024 : k;
		ret = fs->dev->fops->read(fs->dir, buf, k);
		if (ret != k) {
			siska_kfree(buf);
			return -1;
		}

		offset  = SISKA_FS0_BLOCK_START + block_index * block_size;
		offset += i;
		ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
		if (ret < 0) {
			siska_kfree(buf);
			return -1;
		}

		ret = fs->dev->fops->read(fs->dir, buf, k);
		if (ret != k) {
			siska_kfree(buf);
			return -1;
		}

		i += k;
	}

	siska_kfree(buf);
	buf = NULL;

	__siska0_blocks_set_free(sbd, inode_dev->block_offset, inode_dev->block_count);

	__siska0_blocks_set_used(sbd, block_index, nblocks);

	inode_dev->block_offset = block_index;
	inode_dev->block_count  = nblocks;
	return 0;
}

int siska0_write_dir(siska_file_t* file, siska_fs_t* fs, siska_inode_dev_t* parent)
{
	siska_dir_dev_t* dir_dev;
	unsigned long    block_size;
	unsigned long    offset;
	unsigned long    i;

	unsigned long    empty_pos = 0;
	unsigned long    empty_len = 0;
	unsigned long    filename_len = siska_strlen(file->name) + 1;

	dir_dev = siska_kmalloc(sizeof(siska_dir_dev_t));
	if (!dir_dev)
		return -1;

	block_size = 1024 << fs->sblock->sblock_dev->block_size;

	offset = SISKA_FS0_BLOCK_START + parent->block_offset * block_size;

	int ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
	if (ret < 0)
		goto _error;

	for (i = 0; i < parent->size; ) {

		ret = fs->dev->fops->read(fs->dir, dir_dev, sizeof(siska_dir_dev_t));
		if (ret != sizeof(siska_dir_dev_t))
			goto _error;
		i += ret;

		if (i + dir_dev->name_len > parent->size)
			goto _error;

		if (0 == dir_dev->inode_index && dir_dev->name_len >= filename_len) {
			if (empty_len > dir_dev->name_len) {
				empty_len = dir_dev->name_len;
				empty_pos = i - ret;
			}
		}

		char* name = siska_kmalloc(dir_dev->name_len);
		if (!name)
			goto _error;

		ret = fs->dev->fops->read(fs->dir, name, dir_dev->name_len);
		if (ret != dir_dev->name_len) {
			siska_kfree(name);
			goto _error;
		}
		i += ret;

		if (!siska_strcmp(file->name, name)) {
			siska_kfree(name);
			goto _error;
		}

		siska_kfree(name);
	}

	if (0 == empty_len) {

		unsigned long append_size = filename_len + sizeof(siska_dir_dev_t);

		ret = __siska0_realloc_blocks(fs, fs->sblock->sblock_dev, parent, append_size);
		if (ret < 0)
			goto _error;

		empty_len = parent->block_count * block_size - parent->size;
		empty_pos = parent->size;
	}

	if (empty_len > 0) {
		offset  = SISKA_FS0_BLOCK_START + parent->block_offset * block_size;
		offset += empty_pos;

		ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
		if (ret < 0)
			goto _error;

		dir_dev->inode_index = file->inode->index;
		dir_dev->name_len    = filename_len;

		ret = fs->dev->fops->write(fs->dir, dir_dev, sizeof(siska_dir_dev_t));
		if (ret != sizeof(siska_dir_dev_t))
			goto _error;

		ret = fs->dev->fops->write(fs->dir, file->name, filename_len);
		if (ret != filename_len)
			goto _error;

		if (empty_pos == parent->size)
			parent->size += sizeof(siska_dir_dev_t) + filename_len;

		return 0;
	}

_error:
	siska_kfree(dir_dev);
	return -1;
}

int siska0_create_inode(siska_file_t* file, siska_fs_t* fs, siska_inode_dev_t* parent)
{
	siska_sblock_dev_t* sblock_dev;
	siska_sblock_t*     sblock;

	siska_inode_dev_t*  inode_dev;
	siska_inode_t*      inode;

	unsigned long i;
	unsigned long j;
	unsigned long index;

	sblock = fs->sblock;

	while (sblock) {

		sblock_dev = sblock->sblock_dev;

		for (i = 0; i < sizeof(sblock_dev->inode_map); i++) {

			if (0xff == sblock_dev->inode_map[i])
				continue;

			for (j = 0; j < 8; j++) {

				if (0 == i && 0 == j)
					continue;

				if (!(sblock_dev->inode_map[i] & (1u << j)))
					goto _found;
			}
		}

		sblock = sblock->next;
	}
	return -1;

_found:
	inode = siska_inode_alloc();
	if (!inode)
		return -1;

	inode_dev = siska_kmalloc(sizeof(siska_inode_dev_t));
	if (!inode_dev) {
		siska_inode_free(inode);
		return -1;
	}

	inode_dev->size = 0;
	inode_dev->block_offset = 0;
	inode_dev->block_count  = 0;

	if (file->parent->fs == fs)
		inode_dev->inode_parent = 0;
	else
		inode_dev->inode_parent = file->parent->inode->index;

	inode_dev->name_parent_offset = 0;

	inode->index  = i * 8 + j;
	inode->file   = file;
	inode->sblock = sblock;
	inode->inode_dev = inode_dev;

	file->inode = inode;

	if (siska0_write_dir(file, fs, parent) < 0) {
		siska_inode_free(inode);
		return -1;
	}

	sblock_dev->inode_map[i] |= 1u << j;

	inode->next    = sblock->inodes;
	sblock->inodes = inode;
	return 0;
}

int siska0_open_in_sblock(siska_file_t* file, siska_fs_t* fs, int flags, unsigned int mode)
{
	siska_inode_dev_t*  inode_dev;
	siska_dir_dev_t*    dir_dev;
	unsigned long       block_size;

	inode_dev = siska_kmalloc(sizeof(siska_inode_dev_t));
	if (!inode_dev)
		return -1;

	dir_dev = siska_kmalloc(sizeof(siska_dir_dev_t));
	if (!dir_dev) {
		siska_kfree(inode_dev);
		return -1;
	}

	block_size = 1024 << fs->sblock->sblock_dev->block_size;

	// read first inode
	int ret = fs->dev->fops->lseek(fs->dir, SISKA_FS0_INODE_START, SISKA_SEEK_SET);
	if (ret < 0)
		goto _error;

	ret = fs->dev->fops->read(fs->dir, inode_dev, sizeof(siska_inode_dev_t));
	if (ret != sizeof(siska_inode_dev_t))
		goto _error;

	// read sub dirs of fs rootdir
	ret = siska0_find_in_dir(dir_dev, inode_dev, file->name, fs);
	if (-1 == ret)
		goto _error;

	if (-404 == ret) {
		siska_printk("%s(), %d, file->name: %s\n", __func__, __LINE__, file->name);

		ret = siska0_create_inode(file, fs, inode_dev);
		if (ret < 0)
			goto _error;
	} else {
		ret = siska0_read_inode(file, fs, inode_dev, dir_dev->inode_index);
		if (ret < 0)
			goto _error;
	}

	siska_kfree(dir_dev);
	return 0;

_error:	
	siska_kfree(inode_dev);
	siska_kfree(dir_dev);
	return -1;
}

int siska0_recursive_open(siska_file_t* file, siska_fs_t* fs, int flags, unsigned int mode)
{
	if (!file->parent)
		return -1;

	siska_printk("%s(), %d, file->name: %s, type: %d\n", __func__, __LINE__, file->name, siska_file_type(file));

	if (file->parent->fs == fs)
		return siska0_open_in_sblock(file, fs, flags, mode);

	if (!file->parent->inode || !file->parent->inode->inode_dev) {
//		siska_printk("%s(), %d, file->parent: %s\n", __func__, __LINE__, file->parent->name);

		int ret = siska0_recursive_open(file->parent, fs, file->parent->flags, 0);
		if (ret < 0)
			return -1;
	}

	siska_inode_dev_t*  inode_dev;
	siska_dir_dev_t*    dir_dev;

	inode_dev = file->parent->inode->inode_dev;

	dir_dev = siska_kmalloc(sizeof(siska_dir_dev_t));
	if (!dir_dev)
		return -1;

	int ret = siska0_find_in_dir(dir_dev, inode_dev, file->name, fs);
	if (-1 == ret)
		goto _error;

	if (-404 == ret) {
		ret = siska0_create_inode(file, fs, inode_dev);
		if (ret < 0)
			goto _error;
	} else {
		ret = siska0_read_inode(file, fs, inode_dev, dir_dev->inode_index);
		if (ret < 0)
			goto _error;
	}

	siska_kfree(dir_dev);
	return 0;

_error:	
	siska_kfree(dir_dev);
	return -1;
}

int siska0_open(siska_file_t* file, int flags, unsigned int mode)
{
	if (!file || !file->name)
		return -1;

	siska_fs_t* fs;

	fs = siska_vfs_find_fs(file->parent);
	if (!fs)
		return -1;

	if (!fs->sblock || !fs->sblock->sblock_dev)
		return -1;

	if (siska_strncmp(fs->sblock->sblock_dev->magic, "siska00", 7))
		return -1;

	return siska0_recursive_open(file, fs, flags, mode);
}

int siska0_close(siska_file_t* file)
{
	return 0;
}

int siska0_read(siska_file_t* file, void* buf, int size)
{
	if (!file || !buf || size < 0)
		return -1;

	return -1;
}

int siska0_write(siska_file_t* file, const void* buf, int size)
{
	if (!file || !buf || size < 0)
		return -1;

	if (!file->inode || !file->inode->inode_dev) {
		printf("%s(),%d, inode: %p\n", __func__, __LINE__, file->inode);
		printf("%s(),%d, inode_dev: %p\n", __func__, __LINE__, file->inode->inode_dev);
		return -1;
	}

	siska_fs_t* fs;

	fs = siska_vfs_find_fs(file->parent);
	if (!fs) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	siska_sblock_dev_t* sblock_dev = file->inode->sblock->sblock_dev;
	siska_inode_dev_t*  inode_dev  = file->inode->inode_dev;

	unsigned long block_size = 1024u << sblock_dev->block_size;
	unsigned long nblocks    = (file->offset + size + block_size - 1) / block_size;

	if (0 == inode_dev->block_offset) {

		inode_dev->block_offset = __siska0_find_blocks(sblock_dev, nblocks);

		if (0 == inode_dev->block_offset) {
			printf("%s(),%d\n", __func__, __LINE__);
			return -1;
		}

		inode_dev->block_count = nblocks;

		__siska0_blocks_set_used(sblock_dev, inode_dev->block_offset, nblocks);

	} else if (nblocks > inode_dev->block_count) {

		unsigned long append_size = (nblocks - inode_dev->block_count) * block_size;

		int ret = __siska0_realloc_blocks(fs, sblock_dev, inode_dev, append_size);
		if (ret < 0) {
			printf("%s(),%d\n", __func__, __LINE__);
			return -1;
		}
	}

	unsigned long offset = SISKA_FS0_BLOCK_START
		+ inode_dev->block_offset * block_size
		+ file->offset;

	int ret = fs->dev->fops->lseek(fs->dir, offset, SISKA_SEEK_SET);
	if (ret < 0) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	ret = fs->dev->fops->write(fs->dir, buf, size);
	if (ret != size) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}
	return ret;
}

int siska0_lseek(siska_file_t* file, long offset, int whence)
{
	if (!file || !file->dev || offset < 0 || SISKA_SEEK_SET != whence)
		return -1;

	file->dev->priv_pos = offset;
	return 0;
}

int siska0_sync(siska_file_t* file)
{
	return 0;
}

int siska0_ioctl(siska_file_t* file, unsigned long cmd, ...)
{
	return 0;
}

siska_file_ops_t siska_fops_siska0 = 
{
	.type   = "siska0",
	.next   = NULL,

	.open   = siska0_open,
	.close  = siska0_close,
	.read   = siska0_read,
	.write  = siska0_write,
	.lseek  = siska0_lseek,
	.sync   = siska0_sync,
	.ioctl  = siska0_ioctl,
};

int siska0_mkfs(siska_file_t* devfile)
{
	siska_sblock_dev_t* sblock_dev;
	siska_inode_dev_t*  inode_dev;

	siska_dev_t* dev;

	if (!devfile || !devfile->dev) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	dev = devfile->dev;

	if (!dev->fops->write || !dev->fops->lseek) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	int size   = sizeof(siska_sblock_dev_t) + sizeof(siska_inode_dev_t);

	sblock_dev = siska_kmalloc(size);
	if (!sblock_dev) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	inode_dev = (siska_inode_dev_t*)((char*)sblock_dev + sizeof(siska_sblock_dev_t));

	sblock_dev->magic[0]   = 's';
	sblock_dev->magic[1]   = 'i';
	sblock_dev->magic[2]   = 's';
	sblock_dev->magic[3]   = 'k';
	sblock_dev->magic[4]   = 'a';
	sblock_dev->magic[5]   = '0';
	sblock_dev->magic[6]   = '0';

	sblock_dev->block_size =  0;
	sblock_dev->next       =  0;

	siska_memset(sblock_dev->inode_map, 0, sizeof(sblock_dev->inode_map));
	siska_memset(sblock_dev->block_map, 0, sizeof(sblock_dev->block_map));

	sblock_dev->inode_map[0] |= 0x1;
	sblock_dev->block_map[0] |= 0x1;

	inode_dev->size = 0;
	inode_dev->mode = 0x777;
	inode_dev->block_offset = 0;
	inode_dev->block_count  = 1;
	inode_dev->inode_parent = 0xffffffff;
	inode_dev->name[0] = 'r';
	inode_dev->name[1] = 'o';
	inode_dev->name[2] = 'o';
	inode_dev->name[3] = 't';
	inode_dev->name[4] = 'd';
	inode_dev->name[5] = 'i';
	inode_dev->name[6] = 'r';
	inode_dev->name[7] = '\0';

	int ret = dev->fops->lseek(devfile, 0, SISKA_SEEK_SET);
	if (ret < 0) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	ret = dev->fops->write(devfile, sblock_dev, size);
	if (ret != size) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}

	return 0;
}

int siska0_mount(siska_fs_t** pfs, siska_file_t* dir, siska_dev_t* dev)
{
	siska_sblock_dev_t* sblock_dev;
	siska_sblock_t*     sblock;

	siska_inode_dev_t*  inode_dev;
	siska_inode_t*      inode;

	siska_fs_t* fs;
	int ret;

	if (!pfs || !dir || !dev || !dev->fops)
		return -1;

	if (!dev->fops->read || !dev->fops->lseek)
		return -1;

	if (!siska_list_empty(&dir->childs))
		return -1;

	fs = siska_kmalloc(sizeof(siska_fs_t));
	if (!fs)
		return -1;

	dir->dev =  dev;

	fs->dir  =  dir;
	fs->dev  =  dev;
	fs->vfs  = &siska_fs_siska0;
	fs->fops = &siska_fops_siska0;
	fs->sblock = NULL;

	if (dev->fops->open) {
		ret = dev->fops->open(dir, SISKA_FILE_FLAG_R | SISKA_FILE_FLAG_W, 0);
		if (ret < 0) {
			siska_printk("fs open dev error");
			goto _open_dev;
		}
	}

	unsigned long number = 0;
	unsigned long block_offset = 0;
	while (1) {
		sblock = siska_sblock_alloc();
		if (!sblock) {
			siska_printk("fs sblock error");
			goto _error;
		}

		sblock_dev = siska_kmalloc(sizeof(siska_sblock_dev_t));
		if (!sblock_dev) {
			siska_printk("fs sblock dev error");

			siska_kfree(sblock);
			goto _error;
		}
		sblock->sblock_dev = sblock_dev;
		sblock->number     = number++;

		siska_sblock_t** pp = &fs->sblock;
		while (*pp)
			pp = &(*pp)->next;
		*pp = sblock;

		ret = dev->fops->read(dir, sblock_dev, sizeof(siska_sblock_dev_t));
		if (ret != sizeof(siska_sblock_dev_t)) {
			siska_printk("fs sblock read error");
			goto _error;
		}

		if (siska_strncmp(sblock_dev->magic, "siska00", 7)) {
			siska_printk("fs magic error");
			goto _error;
		}

		if (0 == sblock_dev->next)
			break;

		block_offset += sizeof(siska_sblock_dev_t);
		block_offset += sizeof(siska_inode_dev_t) * 8 * sizeof(sblock_dev->inode_map);
		block_offset += (1024 << sblock_dev->block_size) * 8 * sizeof(sblock_dev->block_map);
		block_offset += sizeof(siska_sblock_dev_t);

		ret = dev->fops->lseek(dir, block_offset, SISKA_SEEK_SET);
		if (ret != block_offset) {
			siska_printk("fs sblock seek error");
			goto _error;
		}
	}

	*pfs = fs;
	return 0;

_error:
	while (fs->sblock) {
		sblock     = fs->sblock;
		fs->sblock = sblock->next;

		siska_sblock_free(sblock);
	}
_close_dev:
	if (dev->fops->close)
		dev->fops->close(dir);
	dir->dev = NULL;
_open_dev:
	siska_kfree(fs);
	return -1;
}

int siska0_umount(siska_file_t* dir)
{
	return 0;
}

siska_vfs_t siska_fs_siska0 = 
{
	.name   = "siska0",

	.next   = NULL,

	.mkfs   = siska0_mkfs,

	.mount  = siska0_mount,
	.umount = siska0_umount,
};

