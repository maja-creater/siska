#include"siska_vfs.h"
#include"siska_mm.h"

static siska_dev_t        siska_dev_root;
static siska_file_t       siska_dir_root;
static siska_spinlock_t   siska_spinlock_root;

static siska_vfs_t*       siska_vfs_head  = NULL;
static siska_file_ops_t*  siska_fops_head = NULL;

#define ROOTFS_OFFSET_PTR (0x90000 + 500)
#define ROOTFS_SIZE_PTR   (0x90000 + 504)
#define ROOTFS_OFFSET     ((*(volatile unsigned long*) ROOTFS_OFFSET_PTR) << 9)
#define ROOTFS_SIZE       ((*(volatile unsigned long*) ROOTFS_SIZE_PTR)   << 9)

extern siska_vfs_t  siska_fs_siska0;

static siska_vfs_t* siska_vfs_array[] = 
{
	&siska_fs_siska0,
	NULL
};

extern siska_file_ops_t  siska_fops_memory_dev;
extern siska_file_ops_t  siska_fops_siska0;

static siska_file_ops_t* siska_fops_array[] = 
{
	&siska_fops_memory_dev,
	&siska_fops_siska0,
	NULL
};

void siska_vfs_init()
{
	siska_file_ops_t* fops;
	siska_vfs_t*      vfs;

	siska_vfs_head  = NULL;
	siska_fops_head = NULL;

	int i;
	for (i = 0; siska_vfs_array[i]; i++) {

		vfs = siska_vfs_array[i];

		vfs->next      = siska_vfs_head;
		siska_vfs_head = vfs;
	}

	for (i = 0; siska_fops_array[i]; i++) {

		fops = siska_fops_array[i];

		fops->next      = siska_fops_head;
		siska_fops_head = fops;
	}
}

int siska_vfs_mkfs(siska_file_t* devfile, const char* fsname)
{
	siska_vfs_t* vfs;

	if (!devfile || !fsname)
		return -1;

	for (vfs = siska_vfs_head; vfs; vfs = vfs->next) {
		if (!siska_strcmp(vfs->name, fsname))
			break;
	}

	if (!vfs)
		return -1;

	if (vfs->mkfs)
		return vfs->mkfs(devfile);

	return -1;
}

int siska_vfs_mount(siska_file_t* dir, siska_dev_t* dev, const char* fsname)
{
	siska_vfs_t* vfs;

	if (!dir || !dev || !fsname)
		return -1;

	if (dir->fs)
		return -1;

	for (vfs = siska_vfs_head; vfs; vfs = vfs->next) {
		if (!siska_strcmp(vfs->name, fsname))
			break;
	}

	if (!vfs)
		return -1;

	if (vfs->mount)
		return vfs->mount(&dir->fs, dir, dev);
	return -1;
}

int siska_vfs_umount(siska_file_t* dir)
{
	siska_vfs_t* vfs;

	if (dir && dir->fs) {
		vfs =  dir->fs->vfs;

		if (vfs && vfs->umount) {
			if (vfs->umount(dir) < 0)
				return -1;
		}

		dir->fs = NULL;
		return 0;
	}

	return -1;
}

siska_file_t* siska_dir_find_child(siska_file_t* dir, const char* name, int len)
{
	siska_file_t* file;
	siska_list_t* l;

	for (l = siska_list_head(&dir->childs); l != siska_list_sentinel(&dir->childs);
			l = siska_list_next(l)) {

		file  = siska_list_data(l, siska_file_t, list);

		if (siska_strlen(file->name) != len)
			continue;

		if (!siska_strncmp(file->name, name, len))
			return file;
	}
	return NULL;
}

siska_file_t* siska_dir_add_file(siska_file_t* dir, const char* name, int len)
{
	siska_file_t* file;

	file = siska_file_alloc();
	if (!file)
		return NULL;

	file->name = siska_kmalloc(len + 1);
	if (!file->name) {
		siska_file_free(file);
		return NULL;
	}

	siska_memcpy(file->name, name, len);
	file->name[len] = '\0';
	file->len       = len;

	file->parent    = dir;

	siska_list_add_tail(&dir->childs, &file->list);

	return file;
}

int siska_vfs_open(siska_file_t** pfile, const char* path, int flags, int mode)
{
	if (!pfile || !path)
		return -1;

	char* p0 = (char*)path;
	char* p1 = p0;

	if ('.' == *p1)
		return -1;

	if ('/' != *p1)
		return -1;

	siska_file_t* sentinel;
	siska_file_t* file;
	siska_file_t* dir;

	dir = &siska_dir_root;
	sentinel = NULL;

	p0++;
	p1 = p0;

	unsigned long eflags;
	siska_spin_lock_irqsave(&siska_spinlock_root, eflags);
	while (*p1) {
		p1++;

		if ('/' != *p1)
			continue;

		file = siska_dir_find_child(dir, p0, (int)(p1 - p0));

		if (!file) {
			file = siska_dir_add_file(dir, p0, (int)(p1 - p0));
			if (!file)
				goto _error;

			if (!sentinel) {
				sentinel = dir;
//				siska_printk("sentinel: %s, type: %d, file: %s\n",
//						sentinel->name, siska_file_get_type(sentinel), file->name);
			}

			siska_file_set_type(file, SISKA_FILE_DIR);

		} else if (SISKA_FILE_DIR != siska_file_type(file)) {
			siska_printk("file: %s, already exist\n", file->name);

			siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);
			return -1;
		}

		dir = file;

		p1++;
		while (*p1 && '/' == *p1)
			p1++;
		p0 = p1;
	}

	if ('/' != *p0) {

		file = siska_dir_find_child(dir, p0, (int)(p1 - p0));

		if (!file) {
			file = siska_dir_add_file(dir, p0, (int)(p1 - p0));
			if (!file)
				goto _error;

			if (!sentinel)
				sentinel = dir;

			siska_file_set_type(file, siska_file_type_of_flags(flags));
//			siska_printk("file: %s, type: %d\n", file->name, siska_file_get_type(file));
//			siska_printk("file: %s, type: %d\n", file->name, siska_file_get_type(file));

		} else if (siska_file_type(file) == siska_file_type_of_flags(flags)) {
			siska_printk("file: %s, already exist\n", file->name);

			siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);
			return -1;
		}
	}

	if (!file->ops) {

		siska_file_t* f;

		for (f = file; f; f = f->parent) {
			if (f->fs) {
				file->ops = f->fs->fops;
				break;
			}
		}

		if (!file->ops) {
			siska_printk("file: %s, ops not found\n", file->name);

			dir = file;
			goto _error;
		}

		if (file->ops->open) {
			unsigned int fflags;
			int ret;

			fflags = file->flags;

			siska_spin_lock(&file->lock);
			ret   = file->ops->open(file, flags, mode);
			siska_spin_unlock(&file->lock);

			file->flags = fflags;

			if (ret < 0) {
				siska_printk("file: %s, ops->open() error\n", file->name);

				dir = file;
				goto _error;
			}
		}
	}
	siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);

	*pfile = file;
	return 0;

_error:
	if (!sentinel) {
		siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);
		return -1;
	}

//	siska_printk("delete sentinel file: %s, type: %d\n", sentinel->name, siska_file_get_type(sentinel));

	while (dir != sentinel) {

		if (!siska_list_empty(&dir->childs)) {
			siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);
			return -1;
		}

		siska_file_t* tmp = dir->parent;

//		siska_printk("delete new file: %s, type: %d\n", dir->name, siska_file_get_type(dir));

		siska_list_del(&dir->list);
		siska_file_free(dir);
		dir = tmp;
	}

	siska_spin_unlock_irqrestore(&siska_spinlock_root, eflags);
	return -1;
}

int siska_vfs_write(siska_file_t* file, const char* buf, int count)
{
	if (!file || !buf || count < 0)
		return -1;

	if (0 == count)
		return 0;

	unsigned long flags;
	int ret = -1;

	siska_spin_lock_irqsave(&file->lock, flags);
	if (file->ops && file->ops->write)
		ret = file->ops->write(file, buf, count);
	siska_spin_unlock_irqrestore(&file->lock, flags);

	if (ret < 0)
		siska_printk("write file: %s, file->ops: %p\n", file->name, file->ops);

	return ret;
}

int siska_vfs_read(siska_file_t* file, char* buf, int count)
{
	if (!file || !buf || count < 0)
		return -1;

	if (0 == count)
		return 0;

	unsigned long flags;
	int ret = -1;

	siska_spin_lock_irqsave(&file->lock, flags);
	if (file->ops && file->ops->read)
		ret = file->ops->read(file, buf, count);
	siska_spin_unlock_irqrestore(&file->lock, flags);

	if (ret < 0)
		siska_printk("read file: %s, file->ops: %p\n", file->name, file->ops);

	return ret;
}

int siska_vfs_lseek(siska_file_t* file, long offset, int whence)
{
	if (!file)
		return -1;

	unsigned long flags;
	int ret = -1;

	siska_spin_lock_irqsave(&file->lock, flags);
	if (file->ops && file->ops->lseek)
		ret = file->ops->lseek(file, offset, whence);
	siska_spin_unlock_irqrestore(&file->lock, flags);

	if (ret < 0)
		siska_printk("lseek file: %s, file->ops: %p\n", file->name, file->ops);

	return ret;
}

void siska_dir_print(siska_file_t* dir)
{
	siska_file_t* file;
	siska_list_t* l;

	if (!dir)
		return;

	if (siska_list_empty(&dir->childs))
		siska_printk("%s\n", dir->name);
	else
		siska_printk("%s/\n", dir->name);

	for (l = siska_list_head(&dir->childs); l != siska_list_sentinel(&dir->childs);
			l = siska_list_next(l)) {

		file  = siska_list_data(l, siska_file_t, list);

		siska_printk("%s/%s\n", dir->name, file->name);
	}
}

void siska_dir_print_recursive(siska_file_t* dir, siska_file_t* root)
{
	siska_file_t* file;
	siska_list_t* l;

	if (!dir)
		return;

	int n = 0;
	file  = dir;
	while (file != root) {
		file = file->parent;
		n++;
	}

	siska_file_t** path = siska_kmalloc(sizeof(siska_file_t*) * n);
	if (!path)
		return;

	n    = 0;
	file = dir;
	while (file != root) {
		path[n++] = file;
		file = file->parent;
	}

	if (root && root != dir) {
		if ('/' == root->name[0])
			siska_printk("/");
		else
			siska_printk("./%s/", root->name);
	}

	while (--n > 0)
		siska_printk("%s/", path[n]->name);

	siska_printk("%s\n", dir->name);
	siska_kfree(path);
	path = NULL;

	for (l = siska_list_head(&dir->childs); l != siska_list_sentinel(&dir->childs);
			l = siska_list_next(l)) {

		file  = siska_list_data(l, siska_file_t, list);

		siska_dir_print_recursive(file, root);
	}
}

siska_file_t* siska_file_alloc()
{
	siska_file_t* file = siska_kmalloc(sizeof(siska_file_t));
	if (!file)
		return NULL;

	siska_list_init(&file->list);
	siska_list_init(&file->childs);
	file->parent = NULL;

	file->name   = NULL;
	file->len    = 0;

	file->flags  = 0;
	file->mode   = 0;

	siska_spinlock_init(&file->lock);

	file->data   = NULL;
	file->size   = 0;
	file->offset = 0;

	file->ops    = NULL;
	file->inode  = NULL;
	file->dev    = NULL;
	file->fs     = NULL;

	return file;
}

void siska_file_free(siska_file_t* file)
{
	if (file)
		siska_kfree(file);
}

siska_sblock_t* siska_sblock_alloc()
{
	siska_sblock_t* sblock = siska_kmalloc(sizeof(siska_sblock_t));
	if (!sblock)
		return NULL;

	sblock->next   = NULL;
	sblock->number = 0;
	sblock->inodes = NULL;
	sblock->sblock_dev = NULL;

	return sblock;
}

void siska_sblock_free(siska_sblock_t* sblock)
{
	if (sblock) {

		if (sblock->sblock_dev)
			siska_kfree(sblock->sblock_dev);

		siska_kfree(sblock);
	}
}

siska_inode_t* siska_inode_alloc()
{
	siska_inode_t* inode = siska_kmalloc(sizeof(siska_inode_t));
	if (!inode)
		return NULL;

	inode->next   = NULL;
	inode->file   = NULL;
	inode->sblock = NULL;
	inode->index  = 0;
	inode->inode_dev = NULL;

	return inode;
}

void siska_inode_free(siska_inode_t* inode)
{
	if (inode) {

		if (inode->inode_dev)
			siska_kfree(inode->inode_dev);

		siska_kfree(inode);
	}
}
#ifndef ON_BOCHS
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<fcntl.h>
#include<unistd.h>
#endif

int siska_fs_init()
{
	siska_vfs_init();

	siska_spinlock_init(&siska_spinlock_root);

	siska_dev_root.name ="rootdev";
	siska_dev_root.fops = &siska_fops_memory_dev;
#ifdef ON_BOCHS
	siska_dev_root.priv = (void*)ROOTFS_OFFSET;
	siska_dev_root.priv_size = ROOTFS_SIZE;
#else
#if 0
	siska_dev_root.priv = malloc(1024 * 10);
	siska_dev_root.priv_size = 1024 * 10;
#else
	int fd = open("fs.bin", O_RDONLY, 0666);
	if (fd < 0)
		return -1;

	siska_dev_root.priv = mmap(NULL, 1024 * 10, PROT_READ , MAP_PRIVATE, fd, 0);
	if (MAP_FAILED == siska_dev_root.priv)
		return -1;
	siska_dev_root.priv_size = 1024 * 10;
#endif
#endif
	siska_dev_root.priv_pos  = 0;

	siska_list_init(&siska_dir_root.list);
	siska_list_init(&siska_dir_root.childs);

	siska_dir_root.parent = NULL;

	siska_dir_root.name  = "/";
	siska_dir_root.len   = 1;
	siska_dir_root.flags = SISKA_FILE_FLAG_R | SISKA_FILE_FLAG_W;

	siska_file_set_type(&siska_dir_root, SISKA_FILE_DIR);

	siska_dir_root.mode = SISKA_FILE_MOD_R | SISKA_FILE_MOD_W | SISKA_FILE_MOD_X;

	siska_dir_root.data   = NULL;
	siska_dir_root.size   = 0;
	siska_dir_root.offset = 0;

	siska_dir_root.ops    = NULL;
	siska_dir_root.inode  = NULL;
	siska_dir_root.dev    = &siska_dev_root;
	siska_dir_root.fs     = NULL;
#if 0
	if (siska_vfs_mkfs(&siska_dir_root, "siska0") < 0) {
		printf("%s(),%d\n", __func__, __LINE__);
		return -1;
	}
#endif
	int ret = siska_vfs_mount(&siska_dir_root, &siska_dev_root, "siska0");
	siska_printk("root fs init, ret: %d\n", ret);
	return ret;
}

#ifndef ON_BOCHS
static int make_file(siska_file_t** pfile, const char* fname, const char* data, int len)
{
	siska_file_t* file = NULL;

	int ret = siska_vfs_open(&file, fname,
			SISKA_FILE_FILE | SISKA_FILE_FLAG_R | SISKA_FILE_FLAG_W,
			0777);
	if (ret < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	if (data && len > 0) {
		ret = siska_vfs_write(file, data, len);
		if (ret < 0) {
			printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);
			return -1;
		}
		printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);
	}

	*pfile = file;
	return 0;
}

static int read_file(siska_file_t* file)
{
	unsigned char buf[128] = {0};

	int ret = siska_vfs_lseek(file, 0, SISKA_SEEK_SET);
	if (ret < 0) {
		printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);
		return -1;
	}

	ret = siska_vfs_read(file, buf, sizeof(buf) - 1);
	if (ret < 0) {
		printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);
		return -1;
	}
	printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);
	printf("%s(),%d, buf: %s\n", __func__, __LINE__, buf);

	printf("%s(),%d, binary:\n", __func__, __LINE__);

	int i;
	for (i = 0; i < ret; i++)
		printf("%#x ", buf[i]);
	printf("\n");
	return 0;
}

int main()
{
	int ret = siska_fs_init();
	if (ret < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	siska_file_t* file  = NULL;
	siska_file_t* file2 = NULL;
#if 0
	if (make_file(&file, "/home/my", "hello", 6) < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}
	printf("\n");

	int fd_exec = open("./execve.bin", O_RDONLY);
	if (fd_exec < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}

	char buf[256];
	ret = read(fd_exec, buf, 256);
	if (ret < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}
	printf("%s(),%d, ret: %d\n", __func__, __LINE__, ret);

	if (make_file(&file2, "/home/execve", buf, ret) < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}
#else
#if 1
	if (make_file(&file, "/home/my", NULL, 0) < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}
#endif
	if (make_file(&file2, "/home/execve", NULL, 0) < 0) {
		printf("%s(),%d, error\n", __func__, __LINE__);
		return -1;
	}
#endif
	printf("\n");
	siska_dir_print_recursive(&siska_dir_root, &siska_dir_root);

	read_file(file);
	read_file(file2);
#if 0
	int fd = open("fs.bin", O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fd > 0) {
		ret = write(fd, siska_dev_root.priv, siska_dev_root.priv_size);
	}
#endif
	return 0;
}
#endif
