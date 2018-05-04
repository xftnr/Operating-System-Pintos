/*
* Authors: Yige Wang, Pengdi Xia, Peijie Yang, Wei Po Chen
* Date: 04/30/2018
* Description: Manages the data structure representing the
*              layout of a file's data on disk.
*/
#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_NUM 10
#define BLOCKS_IN_INDIRECT (BLOCK_SECTOR_SIZE/sizeof(block_sector_t))

/* On-disk inode.
Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  /* A total of 12 entries with 10 direct blocks, 1 indirect block,
  and 1 double-indirect block. */
  block_sector_t direct_blocks[DIRECT_NUM];
  block_sector_t indirect_block;
  block_sector_t double_indirect_block;
  bool isdir;                         /* Directory or file? */

  off_t eof;                          /* EOF for readers. */
  off_t length;                       /* File size in bytes. */
  unsigned magic;                     /* Magic number. */
  uint32_t unused[112];               /* Not used. */
};

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct inode_disk data;             /* Inode content. */
  struct lock inode_lock;             /* Synchronize changes to files. */
};

/* Returns the block device sector that contains byte offset POS
within INODE.
Returns -1 if INODE does not contain data for a byte at offset
POS. */
// Peijie and Pengdi Driving
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  size_t sectors;         /* Number of sectors */
  block_sector_t buffer[BLOCKS_IN_INDIRECT];
  block_sector_t buffer2[BLOCKS_IN_INDIRECT];

  ASSERT (inode != NULL);
  if (pos <= inode->data.length) {
    sectors = pos / BLOCK_SECTOR_SIZE;
    if (sectors < DIRECT_NUM) {
      /* Get sector number from direct blcoks. */
      return inode->data.direct_blocks[sectors];
    } else if (sectors < BLOCKS_IN_INDIRECT + DIRECT_NUM) {
      /* Get sector number from indirect blcoks. */
      block_read (fs_device, inode->data.indirect_block, buffer);
      return buffer[sectors - DIRECT_NUM];
    } else {
      /* Get sector number from doubleindirect blcoks. */
      block_sector_t buffer[BLOCKS_IN_INDIRECT];
      block_read (fs_device, inode->data.double_indirect_block, buffer);
      size_t current_indirect = (sectors - DIRECT_NUM - BLOCKS_IN_INDIRECT)
                                / BLOCKS_IN_INDIRECT;
      block_read (fs_device, buffer[current_indirect], buffer2);
      return buffer2[(sectors - DIRECT_NUM - BLOCKS_IN_INDIRECT)
                                % BLOCKS_IN_INDIRECT];
    }
  } else {
    return -1;
  }
}


/* List of open inodes, so that opening a single inode twice
  returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void)
{
  list_init (&open_inodes);
}

/* Add block sectors into indirect block. If indirect_block is
not allocated, allocate a sector for it. Else, read the sector
into a buffer and update it when block sectors are allocated.

current_sectors - nunber of sectors already allocated.
sectors_to_add - nunber of sectors to be allocated.
indirect_block - the indirect block to add the sectors to.

Returns the updated indirect_block.
*/
// Yige Driving
block_sector_t
indirect_block_allocate(size_t current_sectors, size_t sectors_to_add,
                        block_sector_t indirect_block) {
  block_sector_t buffer[BLOCKS_IN_INDIRECT];    /* Old indirect_block. */
  memset (buffer, 0, BLOCKS_IN_INDIRECT);

  if (indirect_block == 0) {
    /* indirect_block not allocated*/
    if (!free_map_allocate (1, &indirect_block)) {
      return NULL;
    }
  } else {
    /* indirect_block not empty, read to buffer. */
    block_read (fs_device, indirect_block, buffer);
  }

  /* Allocates sectors_to_add number of sectors to buffer. */
  while (sectors_to_add > 0) {
    if (!free_map_allocate (1, &buffer[current_sectors])) {
      break;
    }
    /* Writes zero to the allocated sector. */
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write (fs_device, buffer[current_sectors], zeros);
    sectors_to_add--;
    current_sectors++;
  }

  /* Writes the upadted indirect_block to disk. */
  block_write (fs_device, indirect_block, buffer);
  return indirect_block;
}

/* Add block sectors into double indirect block. If double_indirect_block is
not allocated, allocate a sector for it. Else, read the sector
into a buffer and update it when block sectors are allocated.

current_sectors - nunber of sectors already allocated.
sectors_to_add - nunber of sectors to be allocated.
double_indirect_block - the double indirect block to add the sectors to.

Returns the updated double_indirect_block.
*/
// Pengdi Driving
block_sector_t
double_indirect_block_allocate(size_t current_sectors, size_t sectors_to_add,
                               block_sector_t double_indirect_block) {
  block_sector_t buffer[BLOCKS_IN_INDIRECT];   /* Old double_indirect_block. */
  memset (buffer, 0, BLOCKS_IN_INDIRECT);
  size_t current_indirect;    /* Indirect block to allocate the sector to. */
  size_t remaining_sectors;    /* Number of sectors left in indirect block. */
  size_t sectors;           /* Number of sectors to add to indirect block. */

  if (double_indirect_block == 0) {
    /* double_indirect_block not allocated*/
    if (!free_map_allocate (1, &double_indirect_block)) {
      return NULL;
    }
  } else {
    /* double_indirect_block not empty, read to buffer. */
    block_read (fs_device, double_indirect_block, buffer);
  }

  /* Allocates sectors_to_add number of sectors to buffer. */
  while (sectors_to_add > 0) {
    /* Computes which indirect sector to start allocating. */
    current_indirect = current_sectors / BLOCKS_IN_INDIRECT;

    /* Computes number of sectors to allocate to this indirect block
    the maximum between number of sectors remaining in this
    indirect block and number of sectors left to add. */
    remaining_sectors = BLOCKS_IN_INDIRECT -
                               current_sectors % BLOCKS_IN_INDIRECT;
    sectors = sectors_to_add < remaining_sectors ?
                     sectors_to_add : remaining_sectors;

    /* Allocate indirect block, which will allocate sector block. */
    buffer[current_indirect] = indirect_block_allocate(
                               current_sectors % BLOCKS_IN_INDIRECT,
                               sectors, buffer[current_indirect]);
    sectors_to_add -= sectors;
    current_sectors += sectors;
  }

  /* Writes the upadted double_indirect_block to disk. */
  block_write (fs_device, double_indirect_block, buffer);
  return double_indirect_block;
}

/* Allocate sectors for file with disk_inode when the file is created
with a length > 0 or file is extending.

sectors_to_add - nunber of sectors to be allocated.
disk_inode - disk_inode of the file that will be extending.

Returns whether the allocation is successful.
*/
// Yige and Peijei Driving
bool
sector_allocate(size_t sectors_to_add, struct inode_disk *disk_inode) {
  /* Number of sectors already allocted for this file. */
  size_t current_sectors;
  size_t sectors;         /* Number of sectors to add to indirect block. */

  current_sectors = bytes_to_sectors (disk_inode->length);
  if (current_sectors + sectors_to_add > DIRECT_NUM + BLOCKS_IN_INDIRECT
      + BLOCKS_IN_INDIRECT * BLOCKS_IN_INDIRECT) {
    printf("Exceeds maximum file size\n");
    return false;
  }

  if (sectors_to_add == 0) {
    return true;
  }

  /* Allocate direct blocks if necessary. */
  while (sectors_to_add > 0 && current_sectors < DIRECT_NUM) {
    if (!free_map_allocate (1, &disk_inode->direct_blocks[current_sectors])) {
      return false;
    }
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write(fs_device, disk_inode->direct_blocks[current_sectors], zeros);
    sectors_to_add--;
    current_sectors++;
  }

  /* Allocate indirect block if necessary. */
  if (sectors_to_add > 0 &&
      current_sectors < DIRECT_NUM + BLOCKS_IN_INDIRECT) {
    // calculate how many sectors to allocate in indirect_block
    sectors = sectors_to_add > BLOCKS_IN_INDIRECT ?
    BLOCKS_IN_INDIRECT : sectors_to_add;

    /* Allocate sector blocks to indirect block. */
    if ((disk_inode->indirect_block =
      indirect_block_allocate(current_sectors - DIRECT_NUM, sectors,
      disk_inode->indirect_block)) == NULL) {
        return false;
      }
      current_sectors += sectors;
      sectors_to_add -= sectors;
    }

    /* Allocate double indirect block if necessary. */
    if (sectors_to_add > 0) {
      if ((disk_inode->double_indirect_block =
        double_indirect_block_allocate(current_sectors -
          DIRECT_NUM - BLOCKS_IN_INDIRECT, sectors_to_add,
          disk_inode->double_indirect_block)) == NULL) {
        return false;
      }
    }
  return true;
}

/* Initializes an inode with LENGTH bytes of data and
writes the new inode to sector SECTOR on the file system
device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
  one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    // Wei Po Driving
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = 0;
    disk_inode->magic = INODE_MAGIC;
    /* Initialize all sectors to 0. */
    disk_inode->indirect_block = 0;
    disk_inode->double_indirect_block = 0;
    disk_inode->isdir = isdir;

    /* Allocate sectors for file with length and write disk_inode to disk. */
    if (sector_allocate(sectors, disk_inode)) {
      disk_inode->length = length;
      disk_inode->eof = length;
      block_write(fs_device, sector, disk_inode);
      success = true;
    }
    free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector)
        {
          inode_reopen (inode);
          return inode;
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->inode_lock);
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk. (Does it?  Check code.)
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode)
{
  block_sector_t buffer[BLOCKS_IN_INDIRECT];  /* Old indirect block. */
  int i = 0;
  int j = 0;

  /* Ignore null pointer. */
  if (inode == NULL)
  return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      free_map_release (inode->sector, 1);

      /* Frees allocated sectors in direct blocks. */
      while (i < DIRECT_NUM && inode->data.direct_blocks[i] != 0) {
        free_map_release (inode->data.direct_blocks[i], 1);
        i++;
      }

      /* Frees allocated sectors in indirect blocks
      and the sector for indirect block. */
      if (inode->data.direct_blocks[i] != 0
          && inode->data.indirect_block != 0) {
        block_read (fs_device, inode->data.indirect_block, buffer);
        i = 0;
        while (i < BLOCKS_IN_INDIRECT && buffer[i] != 0) {
          free_map_release (buffer[i], 1);
          i++;
        }
        free_map_release(inode->data.indirect_block, 1);
      }

      /* Frees allocated indirect sectors in double indirect blocks
      and the sector for double indirect block. */
      if (buffer[i] != 0 && inode->data.double_indirect_block != 0) {
        block_read (fs_device, inode->data.double_indirect_block, buffer);
        i = 0;

        /* Frees allocated sectors in indirect blocks
        and the sector for indirect block. */
        while (i < BLOCKS_IN_INDIRECT && buffer[i] != 0) {
          block_sector_t buffer2[BLOCKS_IN_INDIRECT];
          block_read (fs_device, buffer[i], buffer2);

          j = 0;
          while (j < BLOCKS_IN_INDIRECT && buffer2[j] != 0) {
            free_map_release (buffer2[j], 1);
            j++;
          }
          free_map_release (buffer[i], 1);
          i++;
        }
        free_map_release(inode->data.double_indirect_block, 1);
      }
    }
    free (inode);
  }
}

bool
inode_is_removed (struct inode *inode)
{
  ASSERT (inode != NULL);
  return inode->removed;
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode)
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode->data.eof - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL)
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
  off_t offset)
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  off_t new_length;       /* Length till end of write. */
  size_t current_sectors;      /* Number of sector allocated for file. */
  size_t future_sectors;       /* Number of sectors till end of write. */
  size_t sectors;              /* Number of sectors to add. */

  if (inode->deny_write_cnt)
  return 0;

  // Yige and Pengdi Driving
  new_length = offset + size;

  /* Lock is already acquired for a directory. */
  if (!inode_isdir(inode)) {
    lock_acquire(&inode->inode_lock);
  }

  current_sectors = bytes_to_sectors (inode->data.length);
  future_sectors = bytes_to_sectors (new_length);

  struct inode_disk *inode_disk = malloc (BLOCK_SECTOR_SIZE);
  if (inode_disk == NULL) {
    printf("inode null\n");
  }
  block_read(fs_device, inode->sector, inode_disk);

  if (future_sectors > current_sectors) {
    /* Number of sectors needed increased, file extension. */
    sectors = future_sectors - current_sectors;
    sector_allocate(sectors, inode_disk);
  }

  /* Updates file length */
  if (new_length > inode_disk->length) {
    inode_disk->length = new_length;
    block_write (fs_device, inode->sector, inode_disk);
    block_read (fs_device, inode->sector, &inode->data);
  }

  /* Lock will be released by directory operation. */
  if (!inode_isdir(inode)) {
    lock_release(&inode->inode_lock);
  }

  while (size > 0)
  {
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0)
    break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Write full sector directly to disk. */
      block_write (fs_device, sector_idx, buffer + bytes_written);
    }
    else
    {
      /* We need a bounce buffer. */
      if (bounce == NULL)
      {
        bounce = malloc (BLOCK_SECTOR_SIZE);
        if (bounce == NULL)
        break;
      }

      /* If the sector contains data before or after the chunk
      we're writing, then we need to read in the sector
      first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
      block_read (fs_device, sector_idx, bounce);
      else
      memset (bounce, 0, BLOCK_SECTOR_SIZE);
      memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      block_write (fs_device, sector_idx, bounce);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  /* Updates file read length */
  if (new_length > inode_disk->eof) {
    inode_disk->eof = new_length;
    block_write (fs_device, inode->sector, inode_disk);
    block_read (fs_device, inode->sector, &inode->data);
  }

  free(inode_disk);

  free (bounce);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode)
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

// Peijie Driving
/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/* Returns the type of this inode - directory or file. */
bool
inode_isdir(struct inode *inode) {
  return inode->data.isdir;
}

/* Returns true if current directory is opened by another thread. */
bool
inode_is_open(struct inode *inode) {
  return inode->open_cnt > 1;
}

// Wei Po Driving
/* Acquire inod_lock of inode. */
void
inode_lock_acquire(struct inode *inode) {
  lock_acquire(&inode->inode_lock);
}

/* Release inod_lock of inode. */
void
inode_lock_release(struct inode *inode) {
  lock_release(&inode->inode_lock);
}
