#ifndef SISKA_FS_H
#define SISKA_FS_H

#include"siska_vfs.h"

// super block, 512
// inode, 244 * 8 * sizeof(inode_dev)
// block, 244 * 8 * (1024 << sblock_dev->block_size)

#define SISKA_FS0_NB_INODES    (16u << 3)
#define SISKA_FS0_NB_BLOCKS    (16u << 3)
#define SISKA_FS0_INODE_START  (sizeof(siska_sblock_dev_t))
#define SISKA_FS0_BLOCK_START  (sizeof(siska_sblock_dev_t) + SISKA_FS0_NB_INODES * sizeof(siska_inode_dev_t))

struct siska_sblock_dev_s
{
	unsigned char magic[7];
	unsigned char block_size;

	union {
		unsigned long next;
		unsigned char fill[8];
	};

	unsigned char inode_map[SISKA_FS0_NB_INODES >> 3];
	unsigned char block_map[SISKA_FS0_NB_BLOCKS >> 3];
	unsigned char reserved[8];
};

struct siska_inode_dev_s
{
	union {
		unsigned long size;
		unsigned char fill0[8];
	};
#define SISKA_INODE_NAME_OFFSET (1u << 29)
	union {
		unsigned long mode;
		unsigned char fill1[8];
	};

	union {
		unsigned long block_offset;
		unsigned char fill2[8];
	};

	union {
		unsigned long block_count;
		unsigned char fill3[8];
	};

	union {
		unsigned long inode_parent;
		unsigned char fill4[8];
	};

	union {
		unsigned long name_parent_offset;
		unsigned char name[8];
	};
};

typedef struct siska_dir_dev_s siska_dir_dev_t;

struct siska_dir_dev_s
{
	union {
		unsigned long inode_index;
		unsigned char fill0[8];
	};

	union {
		unsigned long name_len;
		unsigned char fill1[8];
	};

	unsigned char name[0]; // string ended by '\0'
};

#endif

