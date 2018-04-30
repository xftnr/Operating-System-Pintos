#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_NUM 10
#define BLOCKS_IN_INDIRECT (BLOCK_SECTOR_SIZE/sizeof(block_sector_t))

// #define INDIRECT_NUM 138
// #define DOUBLE_INDIRECT_NUM 16522

/* On-disk inode.
Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  /* A total of 12 entries with 10 direct blocks, 1 inderect block,
  and 1 double-indirect block. */
  block_sector_t direct_blocks[DIRECT_NUM];
  block_sector_t indirect_block;
  block_sector_t double_indirect_block;
  bool isdir;                         /* Directory or file?*/

  off_t length;                       /* File size in bytes. */
  unsigned magic;                     /* Magic number. */
  uint32_t unused[113];               /* Not used. */
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

};

/* Returns the block device sector that contains byte offset POS
within INODE.
Returns -1 if INODE does not contain data for a byte at offset
POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos <= inode->data.length) {

    size_t sectors = pos / BLOCK_SECTOR_SIZE;
    if (sectors < DIRECT_NUM) {
      return inode->data.direct_blocks[sectors];
    } else if (sectors < BLOCKS_IN_INDIRECT + DIRECT_NUM) {
      block_sector_t buffer[BLOCKS_IN_INDIRECT];
      block_read (fs_device, inode->data.indirect_block, buffer);
      return buffer[sectors - DIRECT_NUM];
    } else {
      block_sector_t buffer[BLOCKS_IN_INDIRECT];
      block_read (fs_device, inode->data.double_indirect_block, buffer);
      size_t current_indirect = (sectors - DIRECT_NUM - BLOCKS_IN_INDIRECT) / BLOCKS_IN_INDIRECT;
      block_sector_t buffer2[BLOCKS_IN_INDIRECT];
      block_read (fs_device, buffer[current_indirect], buffer2);
      return buffer2[(sectors - DIRECT_NUM - BLOCKS_IN_INDIRECT) % BLOCKS_IN_INDIRECT];
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

block_sector_t
indirect_block_allocate(size_t current_sectors, size_t sectors_to_add, block_sector_t indirect_block) {
  // block_sector_t indirect_block;
  block_sector_t buffer[BLOCKS_IN_INDIRECT];
  memset (buffer, 0, BLOCKS_IN_INDIRECT);

  if (indirect_block == 0) {
    if (!free_map_allocate (1, &indirect_block)) {
      return NULL;
    }
    // printf("allocate indirect\n");
  } else {
    block_read (fs_device, indirect_block, buffer);
  }

  // printf("indirect sectors to add %d\n\n", sectors_to_add);

  while (sectors_to_add > 0) {
    if (!free_map_allocate (1, &buffer[current_sectors])) {
      break;
    }
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write (fs_device, buffer[current_sectors], zeros);
    sectors_to_add--;
    current_sectors++;
    // printf("Finished block write\n\n");
  }

  block_write (fs_device, indirect_block, buffer);
  return indirect_block;
}


block_sector_t
double_indirect_block_allocate(size_t current_sectors, size_t sectors_to_add, block_sector_t double_indirect_block) {
//   block_sector_t double_indirect_block = 0;
  block_sector_t buffer[BLOCKS_IN_INDIRECT];
  memset (buffer, 0, BLOCKS_IN_INDIRECT);

  if (double_indirect_block == 0) {
    if (!free_map_allocate (1, &double_indirect_block)) {
      return NULL;
    }
  } else {
    block_read (fs_device, double_indirect_block, buffer);
  }

  while (sectors_to_add > 0) {

    size_t current_indirect = current_sectors / BLOCKS_IN_INDIRECT;
    size_t remaining_sectors = BLOCKS_IN_INDIRECT - current_sectors % BLOCKS_IN_INDIRECT;
    size_t sectors = sectors_to_add < remaining_sectors ? sectors_to_add : remaining_sectors;
    buffer[current_indirect] = indirect_block_allocate(current_sectors % BLOCKS_IN_INDIRECT,
      sectors, buffer[current_indirect]);
      sectors_to_add -= sectors;
      current_sectors += sectors;
    }

    block_write (fs_device, double_indirect_block, buffer);
    return double_indirect_block;
}

bool
sector_allocate(size_t sectors_to_add, struct inode_disk *disk_inode) {
  size_t current_sectors = bytes_to_sectors (disk_inode->length);

  size_t total_sectors = current_sectors + sectors_to_add;
  if (total_sectors > DIRECT_NUM + BLOCKS_IN_INDIRECT + BLOCKS_IN_INDIRECT * BLOCKS_IN_INDIRECT) {
    printf("Exceeds maximum file size\n");
    return false;
  }

  // printf("allocating\n");

  if (sectors_to_add == 0) {
    return true;
  }

  while (sectors_to_add > 0 && current_sectors < DIRECT_NUM) {
    if (!free_map_allocate (1, &disk_inode->direct_blocks[current_sectors])) {
      return false;
    }
    // printf("allocated sector %d \n\n", disk_inode->direct_blocks[current_sectors]);
    static char zeros[BLOCK_SECTOR_SIZE];
    block_write (fs_device, disk_inode->direct_blocks[current_sectors], zeros);
    sectors_to_add--;
    current_sectors++;
  }

  // printf("sectors to add %d\n\n", sectors);
  if (sectors_to_add > 0 && current_sectors < DIRECT_NUM + BLOCKS_IN_INDIRECT) {

    // calculate how many sectors to allocate in indirect_block
    size_t sectors = sectors_to_add > BLOCKS_IN_INDIRECT ?
    BLOCKS_IN_INDIRECT : sectors_to_add;

    if ((disk_inode->indirect_block =
      indirect_block_allocate(current_sectors - DIRECT_NUM, sectors, disk_inode->indirect_block))
      == NULL) {
        return false;
      }

      current_sectors += sectors;
      sectors_to_add -= sectors;
    }

    if (sectors_to_add > 0 ) {
      if ((disk_inode->double_indirect_block =
        double_indirect_block_allocate(current_sectors - DIRECT_NUM - BLOCKS_IN_INDIRECT,
        sectors_to_add, disk_inode->double_indirect_block)) == NULL) {
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
  size_t i = 0;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
  one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->length = 0;
    disk_inode->magic = INODE_MAGIC;
    // disk_inode->direct_blocks = {0};
    disk_inode->indirect_block = 0;
    disk_inode->double_indirect_block = 0;
    disk_inode->isdir = isdir;

    if (sector_allocate(sectors, disk_inode)) {

      disk_inode->length = length;

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

// printf("sector %d\n\n", sector);

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e))
    {

      inode = list_entry (e, struct inode, elem);
      // printf("in for %d\n\n", inode->sector);

      if (inode->sector == sector)
        {
          // printf("return inode %d\n\n", sector);

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
  // printf("assigning inode %d\n\n", sector);

  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);

  // printf("here\n\n");

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
      int i = 0;

      // Frees direct sectors
      while (i < DIRECT_NUM && inode->data.direct_blocks[i] != 0) {
        free_map_release (inode->data.direct_blocks[i], 1);
        i++;
      }

      block_sector_t buffer[BLOCKS_IN_INDIRECT];
      if (inode->data.direct_blocks[i] != 0 && inode->data.indirect_block != 0) {
        // Frees indirect block and its data blocks
        block_read (fs_device, inode->data.indirect_block, buffer);
        i = 0;
        while (i < BLOCKS_IN_INDIRECT && buffer[i] != 0) {
          free_map_release (buffer[i], 1);
          i++;
        }
        free_map_release(inode->data.indirect_block, 1);
      }

      if (buffer[i] != 0 && inode->data.double_indirect_block != 0) {
        // Frees indirect block and its data blocks
        block_read (fs_device, inode->data.double_indirect_block, buffer);
        i = 0;
        while (i < BLOCKS_IN_INDIRECT && buffer[i] != 0) {
          block_sector_t buffer2[BLOCKS_IN_INDIRECT];
          block_read (fs_device, buffer[i], buffer2);

          int j = 0;
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
      off_t inode_left = inode_length (inode) - offset;
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

    if (inode->deny_write_cnt)
    return 0;

    off_t new_length = offset + size;

    // printf("writing new_length %d cur length %d\n\n", new_length, inode->data.length);

    size_t current_sectors = bytes_to_sectors (inode->data.length);
    size_t future_sectors = bytes_to_sectors (new_length);

    // printf("writing current_sectors %d future_sectors  %d\n\n", current_sectors, future_sectors);

    struct inode_disk *inode_disk = malloc (BLOCK_SECTOR_SIZE);
    if (inode_disk == NULL) {
      printf("inode null\n");
    }
    block_read(fs_device, inode->sector, inode_disk);

    if (future_sectors > current_sectors) {
      // file extension
      size_t sectors = future_sectors - current_sectors;
      sector_allocate(sectors, inode_disk);
    }

  // update file length
    if (new_length > inode_disk->length) {
      inode_disk->length = new_length;
      // printf("LENGTH %d\n", inode_disk->length);
      block_write (fs_device, inode->sector, inode_disk);
      block_read (fs_device, inode->sector, &inode->data);
    }

    free(inode_disk);

    // printf("writing new_length %d cur length %d\n\n", new_length, inode->data.length);

    // printf("writing %d\n\n", size);
    while (size > 0)
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
// printf("sector_idx %d\n", sector_idx);
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

    free (bounce);
    // printf("finish writing %d\n\n", size);

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

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool
inode_isdir(struct inode *inode) {
return inode->data.isdir;
}
