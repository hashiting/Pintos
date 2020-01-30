#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

//123 + 1 + 1 = 125, fill up the unused space in inode_disk
#define DIRECT_BLOCK_COUNT 123
#define INDIRECT_BLOCK_POINTERS_PER_SECTOR 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    block_sector_t direct_blocks[DIRECT_BLOCK_COUNT];
    block_sector_t indirect_block;
    block_sector_t doubly_indirect_block;
    
    bool in_dir;//if in dir
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
//    uint32_t unused[125];               /* Not used. */
  };

struct inode_indirect_block{
    block_sector_t blocks[INDIRECT_BLOCK_POINTERS_PER_SECTOR];
};

static bool inode_allocate(struct inode_disk* disk_inode, size_t length);
static bool inode_deallocate(struct inode* inode);

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

static block_sector_t
pos_to_sector(struct inode_disk* disk_inode, off_t pos)
{
  //in a direct block
  if(pos < DIRECT_BLOCK_COUNT * BLOCK_SECTOR_SIZE){
    return disk_inode->direct_blocks[pos / BLOCK_SECTOR_SIZE];
  }
  //in a indirect block
  if(pos < (DIRECT_BLOCK_COUNT + INDIRECT_BLOCK_POINTERS_PER_SECTOR) * BLOCK_SECTOR_SIZE){
    struct inode_indirect_block* indirect_block = calloc(1, sizeof(struct inode_indirect_block));
    filesys_cache_read(disk_inode->indirect_block, indirect_block);//read the indirect block from cache, overwrite indirect_block
    block_sector_t ret = indirect_block->blocks[pos / BLOCK_SECTOR_SIZE - DIRECT_BLOCK_COUNT];//return a block pointed to by one of the pointers in the indirect block
    free(indirect_block);
    return ret;
  }
  //in a doubly indirect block
  if(pos < (DIRECT_BLOCK_COUNT + INDIRECT_BLOCK_POINTERS_PER_SECTOR + INDIRECT_BLOCK_POINTERS_PER_SECTOR * INDIRECT_BLOCK_POINTERS_PER_SECTOR) * BLOCK_SECTOR_SIZE){
    off_t doubly_indirect_block_index = (pos / BLOCK_SECTOR_SIZE - (DIRECT_BLOCK_COUNT + INDIRECT_BLOCK_POINTERS_PER_SECTOR)) / INDIRECT_BLOCK_POINTERS_PER_SECTOR;
    off_t doubly_indirect_block_offset = (pos / BLOCK_SECTOR_SIZE - (DIRECT_BLOCK_COUNT + INDIRECT_BLOCK_POINTERS_PER_SECTOR)) % INDIRECT_BLOCK_POINTERS_PER_SECTOR;
    struct inode_indirect_block* indirect_block = calloc(1, sizeof(struct inode_indirect_block));
    filesys_cache_read(disk_inode->doubly_indirect_block, indirect_block);//read the doubly indirect block from cache, overwrite indirect_block
    filesys_cache_read(indirect_block->blocks[doubly_indirect_block_index], indirect_block);//read the block(1. level) pointed to by one of the pointers in the doubly indirect block from cache, overwrite indirect_block
    block_sector_t ret = indirect_block->blocks[doubly_indirect_block_offset];//return the block(2. level) pointed to by one of the pointers in the block(1. level) pointed to by one of the pointers in the doubly indirect block
    free(indirect_block);
    return ret;
  }

  PANIC("file size larger than the max size of a index-structured inode");
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    return pos_to_sector(&inode->data, pos);
  }
  else
    return -1;
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

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool in_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */

  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->in_dir = in_dir;
      if (inode_allocate(disk_inode, disk_inode->length))
        {
          filesys_cache_write(sector, disk_inode);
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
  filesys_cache_read(inode->sector, &inode->data);
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

/* Closes INODE and writes it to disk.
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
          inode_deallocate(inode);
        }

      free (inode); 
    }
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
          filesys_cache_read(sector_idx, buffer + bytes_read);
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
          filesys_cache_read(sector_idx, bounce);
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

  if(byte_to_sector(inode, offset + size - 1) == -1){//need to extend inode
    bool success = inode_allocate(&inode->data, offset + size);
    ASSERT(success == true);//can it fail?

    inode->data.length = offset + size;
    filesys_cache_write(inode->sector, &inode->data);
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
          filesys_cache_write(sector_idx, buffer + bytes_written);
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
            filesys_cache_read(sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          filesys_cache_write(sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
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

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

bool in_dir(struct inode* inode){
  return inode->data.in_dir;
}

bool is_remove(struct inode* i){
  return i->removed;
}

static bool inode_indirect_allocate(block_sector_t* sector, size_t sectors_count, int level){
  static char dummy[BLOCK_SECTOR_SIZE];

  if(*sector == 0){
    if(!free_map_allocate(1, sector))
      return false;
    filesys_cache_write(*sector, dummy);
  }

  if(level == 0)
    return true;

  struct inode_indirect_block temp_indirect_block;
  filesys_cache_read(*sector, &temp_indirect_block);//read from the cache just written

  size_t to_alloc_on_this_level;
  if(level == 1){
    to_alloc_on_this_level = sectors_count;//sectors in 1 block
  }else{
    to_alloc_on_this_level = DIV_ROUND_UP(sectors_count, INDIRECT_BLOCK_POINTERS_PER_SECTOR); //# of level 2 blocks
  }

  int i;
  for(i = 0; i < to_alloc_on_this_level; i++){//recursively allocate children
    size_t allocating;
    if(level == 1)
      allocating = sectors_count < 1 ? sectors_count : 1;
    else
      allocating = sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR ? sectors_count : INDIRECT_BLOCK_POINTERS_PER_SECTOR;
    if(!inode_indirect_allocate(&temp_indirect_block.blocks[i], allocating, level - 1))
      return false;
    sectors_count -= allocating;
  }

  ASSERT(sectors_count == 0);
  filesys_cache_write(*sector, &temp_indirect_block);//write newly allocated blocks to the cache
  return true;
}

static bool inode_allocate(struct inode_disk* disk_inode, size_t length){
  if(length < 0)
    return false;
  size_t sectors_count = bytes_to_sectors(length);
  size_t allocating = 0;
  //direct blocks
  if(sectors_count < DIRECT_BLOCK_COUNT){
    allocating = sectors_count;
  }else{
    allocating = DIRECT_BLOCK_COUNT;
  }
  int i;
  for(i = 0; i < allocating; i++){
    if(disk_inode->direct_blocks[i] == 0)
      if(!inode_indirect_allocate(&disk_inode->direct_blocks[i],1,0))
        return false;
  }
  sectors_count -= allocating;
  if(sectors_count == 0)
    return true;

  //indirect blocks
  if(sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR){//remaining sectors
    allocating = sectors_count;
  }else{
    allocating = INDIRECT_BLOCK_POINTERS_PER_SECTOR;
  }
  if(!inode_indirect_allocate(&disk_inode->indirect_block, allocating, 1))
    return false;
  sectors_count -= allocating;
  if(sectors_count == 0)
    return true;

  //doubly indirect blocks
  if(sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR * INDIRECT_BLOCK_POINTERS_PER_SECTOR){
    allocating = sectors_count;
  } else{
    allocating = INDIRECT_BLOCK_POINTERS_PER_SECTOR * INDIRECT_BLOCK_POINTERS_PER_SECTOR;
  }
  if(!inode_indirect_allocate(&disk_inode->doubly_indirect_block, allocating, 2))
    return false;
  sectors_count -= allocating;
  if(sectors_count == 0)
    return true;

  ASSERT(sectors_count == 0);
  return false;
}

static void inode_indirect_deallocate(block_sector_t *sector, size_t sectors_count, int level){
  if(level == 0){
    free_map_release(*sector, 1);
    return;
  }

  struct inode_indirect_block temp_indirect_block;
  filesys_cache_read(*sector, &temp_indirect_block);

  size_t to_dealloc_on_this_level;
  if(level == 1){
    to_dealloc_on_this_level = sectors_count;//sectors in 1 block
  }else{
    to_dealloc_on_this_level = DIV_ROUND_UP(sectors_count, INDIRECT_BLOCK_POINTERS_PER_SECTOR); //# of level 2 blocks
  }

  int i;
  for(i = 0; i < to_dealloc_on_this_level; i++){//recursively deallocate children
    size_t deallocating;
    if(level == 1)
      deallocating = sectors_count < 1 ? sectors_count : 1;
    else
      deallocating = sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR ? sectors_count : INDIRECT_BLOCK_POINTERS_PER_SECTOR;
    inode_indirect_deallocate(&temp_indirect_block.blocks[i], deallocating, level - 1);
    sectors_count -= deallocating;
  }

  ASSERT(sectors_count == 0);
  free_map_release(*sector, 1);//deallocate self
}

static bool inode_deallocate(struct inode* inode){
  if(inode->data.length < 0)
    return false;
  size_t sectors_count = bytes_to_sectors(inode->data.length);
  size_t deallocating;
  //direct blocks
  if(sectors_count < DIRECT_BLOCK_COUNT){
    deallocating = sectors_count;
  }else{
    deallocating = DIRECT_BLOCK_COUNT;
  }
  int i;
  for(i = 0; i < deallocating; i++){
    free_map_release(inode->data.direct_blocks[i], 1);
  }
  sectors_count -= deallocating;
  if(sectors_count == 0)
    return true;

  //indirect blocks
  if(sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR){//remaining sectors
    deallocating = sectors_count;
  }else{
    deallocating = INDIRECT_BLOCK_POINTERS_PER_SECTOR;
  }
  inode_indirect_deallocate(&inode->data.indirect_block, deallocating, 1);
  sectors_count -= deallocating;
  if(sectors_count == 0)
    return true;

  //doubly indirect blocks
  if(sectors_count < INDIRECT_BLOCK_POINTERS_PER_SECTOR * INDIRECT_BLOCK_POINTERS_PER_SECTOR){
    deallocating = sectors_count;
  } else{
    deallocating = INDIRECT_BLOCK_POINTERS_PER_SECTOR * INDIRECT_BLOCK_POINTERS_PER_SECTOR;
  }
  inode_indirect_deallocate(&inode->data.doubly_indirect_block, deallocating, 2);
  sectors_count -= deallocating;
  if(sectors_count == 0)
    return true;

  ASSERT(sectors_count == 0);
  return false;
}