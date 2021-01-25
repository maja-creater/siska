#ifndef SISKA_VFS_H
#define SISKA_VFS_H

#include"siska_core.h"

struct siska_file_s
{
	siska_list_t      list;

	siska_list_t      childs;
	siska_file_t*     parent;

	char*             name;
	int               len;

#define SISKA_FILE_MOD_X  1u
#define SISKA_FILE_MOD_W (1u << 1)
#define SISKA_FILE_MOD_R (1u << 2)

#define SISKA_FILE_FLAG_R        (1u)
#define SISKA_FILE_FLAG_W        (1u << 1)
#define SISKA_FILE_FLAG_NONBLOCK (1u << 2)

#define SISKA_FILE_DIR  (1u << 24)
#define SISKA_FILE_FILE (2u << 24)
#define SISKA_FILE_DEV  (3u << 24)
#define SISKA_FILE_SOCK (4u << 24)

#define SISKA_SEEK_SET  0
#define SISKA_SEEK_CUR  1
#define SISKA_SEEK_END  2

#define siska_file_type_of_flags(flags) (flags & ~0xffffff)

	unsigned int      flags;
	unsigned int      mode;

	unsigned char*    data;
	long              size;
	long              offset;

	siska_file_ops_t* ops;

	siska_inode_t*    inode;
	siska_dev_t*      dev;

	siska_fs_t*       fs;
};

struct siska_dev_s
{
	char*  name;

	void*  priv;
	int    priv_size;
	int    priv_pos;

	siska_file_ops_t* fops;
};

struct siska_file_ops_s
{
	const char* type;

	siska_file_ops_t* next;

	int   (*open )(siska_file_t* file, int flags, unsigned int mode);
	int   (*close)(siska_file_t* file);

	int   (*read )(siska_file_t* file,       void* buf, int size);
	int   (*write)(siska_file_t* file, const void* buf, int size);

	int   (*lseek)(siska_file_t* file, long offset, int whence);
	int   (*sync )(siska_file_t* file);

	int   (*ioctl)(siska_file_t* file, unsigned long cmd, ...);
};

struct siska_vfs_s
{
	const char*  name;

	siska_vfs_t* next;

	int   (*mkfs) (siska_file_t* devfile);

	int   (*mount) (siska_fs_t**  pfs, siska_file_t* dir, siska_dev_t* dev);
	int   (*umount)(siska_file_t* dir);
};

struct siska_fs_s
{
	siska_file_t*     dir;
	siska_dev_t*      dev;
	siska_vfs_t*      vfs;

	siska_file_ops_t* fops;

	siska_sblock_t*   sblock;
};

struct siska_sblock_s
{
	siska_sblock_t* next;
	unsigned long   number;

	siska_inode_t*  inodes;

	siska_sblock_dev_t* sblock_dev;
};

struct siska_inode_s
{
	siska_inode_t*  next;

	siska_file_t*   file;

	siska_sblock_t* sblock;
	unsigned long   index;

	siska_inode_dev_t* inode_dev;
};

siska_file_t*   siska_file_alloc();
void            siska_file_free(siska_file_t* file);

siska_sblock_t* siska_sblock_alloc();
siska_inode_t*  siska_inode_alloc();

void siska_sblock_free(siska_sblock_t* sblock);
void siska_inode_free(siska_inode_t* inode);

void siska_vfs_init();

int  siska_vfs_mkfs  (siska_file_t* devfile, const char* fsname);

int  siska_vfs_mount (siska_file_t* dir, siska_dev_t* dev, const char* fsname);
int  siska_vfs_umount(siska_file_t* dir);

int  siska_vfs_open (siska_file_t** pfile, const char* path, int flags, int mode);
int  siska_vfs_write(siska_file_t*  file,  const char* buf,  int count);
int  siska_vfs_read (siska_file_t*  file,        char* buf,  int count);

void siska_dir_print(siska_file_t* dir);
void siska_dir_print_recursive(siska_file_t* dir, siska_file_t* root);

int siska_fs_init();

static inline unsigned int siska_file_type(siska_file_t* file)
{
	return file->flags & ~0xffffff;
}
static inline unsigned int siska_file_get_type(siska_file_t* file)
{
	return (file->flags >> 24) & 0xff;
}
static inline void siska_file_set_type(siska_file_t* file, unsigned int type)
{
	file->flags = (file->flags & 0xffffff) | type;
}

static inline siska_fs_t* siska_vfs_find_fs(siska_file_t* file)
{
	siska_file_t* p;

	for (p = file; p; p = p->parent) {

		if (p->fs)
			return p->fs;
	}
	return NULL;
}

#endif

