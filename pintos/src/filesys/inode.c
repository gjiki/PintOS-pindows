#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "devices/timer.h"
#include "threads/synch.h"
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define BLOCK_CACHE_SIZE 64
#define BLOCK_COUNT_PER_SECTOR 128    
#define DIRECT_BLOCKS_PER_INODE 123
static const uint32_t BLOCKS_PER_SECTOR = BLOCK_SECTOR_SIZE / 4;


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {

    block_sector_t direct_blocks[DIRECT_BLOCKS_PER_INODE];
    block_sector_t indirect_blocks;
    block_sector_t doubly_indirect_blocks;

    bool is_dir;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

static inline size_t
min (size_t a, size_t b)
{
  if(a<b)
    return a;
  return b;
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

/*
  cache structure
 */
typedef struct cache{
  block_sector_t sector;
  uint8_t buffer[BLOCK_SECTOR_SIZE];
  bool is_fill;
  int64_t clock;
  bool not_writed;
};

typedef struct indirect_blocks_sector{
  block_sector_t block[BLOCK_COUNT_PER_SECTOR];
};

/*
 cache buffer
*/
static struct cache cache_blocks[BLOCK_CACHE_SIZE];
static uint32_t clock = 0;

/*
cash lock 
*/
static struct lock cache_lock;
bool inode_alloc(struct inode_disk* disk, size_t sector_count);
void deallocate_inode(struct inode* inode);

static block_sector_t
block_index_to_sector(const struct inode_disk *disk, uint32_t index)
{
  if(index < (uint32_t)DIRECT_BLOCKS_PER_INODE)
    return disk->direct_blocks[index];
  
  index -= (uint32_t)DIRECT_BLOCKS_PER_INODE;
  if(index < BLOCKS_PER_SECTOR)
  {
    struct indirect_blocks_sector ind_blocks;
    //block_read(fs_device,disk->indirect_blocks, &ind_blocks);
    read_cache(disk->indirect_blocks, &ind_blocks);
    return ind_blocks.block[index];
  }

  index -= BLOCKS_PER_SECTOR;
  if(index < BLOCKS_PER_SECTOR * BLOCKS_PER_SECTOR)
  {
    struct indirect_blocks_sector doubly_ind_blocks;
    //block_read(fs_device,disk->doubly_indirect_blocks, &doubly_ind_blocks);
    read_cache(disk->doubly_indirect_blocks, &doubly_ind_blocks);
    struct indirect_blocks_sector ind_blocks;

    //block_read(fs_device,doubly_ind_blocks.block[index / BLOCKS_PER_SECTOR], &ind_blocks);
    read_cache(doubly_ind_blocks.block[index / BLOCKS_PER_SECTOR], &ind_blocks);
    return ind_blocks.block[index % BLOCKS_PER_SECTOR];
  } 
  
  return -1;
}



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos)
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return block_index_to_sector(&inode->data, pos / BLOCK_SECTOR_SIZE);
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
  lock_init(&cache_lock);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
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
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->is_dir = is_dir;
      disk_inode->magic = INODE_MAGIC;
      if (inode_alloc(disk_inode,sectors))
        {
          //block_write (fs_device, sector, disk_inode);
          write_cache (sector, disk_inode);

          if (sectors > 0)
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;

              for (i = 0; i < sectors; i++)
                //block_write (fs_device, block_index_to_sector(disk_inode,i), zeros);
                write_cache (block_index_to_sector(disk_inode,i), zeros);

            }
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
  //block_read (fs_device, inode->sector, &inode->data);
  read_cache(inode->sector, &inode->data);
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
          deallocate_inode(inode);
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
          //block_read (fs_device, sector_idx, buffer + bytes_read);
          read_cache(sector_idx, buffer + bytes_read);
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
          //block_read (fs_device, sector_idx, bounce);
          read_cache(sector_idx, bounce);
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

  if(offset + size > inode->data.length)
  {
    if( bytes_to_sectors(offset + size) >bytes_to_sectors(inode->data.length))
    {

      if(!inode_alloc(&inode->data, bytes_to_sectors(offset + size)))
        return 0;
    }
    inode->data.length = offset + size;
    //block_write(fs_device, inode->sector, &inode->data);
    write_cache(inode->sector, &inode->data);

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
          //block_write (fs_device, sector_idx, buffer + bytes_written);
          write_cache (sector_idx, buffer + bytes_written);

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
          if (sector_ofs > 0 || chunk_size < sector_left){
            //block_read (fs_device, sector_idx, bounce);
            read_cache(sector_idx, bounce);
          }
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          //block_write (fs_device, sector_idx, bounce);
          write_cache(sector_idx, bounce);

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

struct cache * look_cache(block_sector_t sector)
{
  int i = 0;
  for (i = 0 ; i < BLOCK_CACHE_SIZE; ++ i)
  {
    
    if (cache_blocks[i].is_fill == 0)
      continue;
    
    if (cache_blocks[i].sector == sector)
    {
      return &cache_blocks[i];
    }
  }

  return NULL;
}


void 
flush(struct cache * cache_block)
{
    if (cache_block -> not_writed)
    {
      block_write(fs_device, cache_block -> sector, cache_block -> buffer);
      cache_block -> not_writed = 0;
    }
}

struct
cache * give_block_cache()
{
  int i = 0, j = 0;
  for (i = 0; i < BLOCK_CACHE_SIZE; ++ i)
  {
    if (cache_blocks[i].is_fill == 0)
    {
      j = i;
      break;
    }
    if (cache_blocks[i].clock > cache_blocks[j].clock)
    {
      j = i;
    }
  }

  if (cache_blocks[j].not_writed)
  {
    flush(&cache_blocks[j]);
  }

  return &cache_blocks[j];
}

void 
read_cache(block_sector_t sector_idx, void *buffer)
{
  lock_acquire(&cache_lock);
  ++ clock;
  struct cache * cache_block = look_cache(sector_idx);
  if (cache_block == NULL)
  {
    cache_block = give_block_cache();
    cache_block -> sector = sector_idx;
    cache_block -> not_writed = false;
    block_read(fs_device, sector_idx, cache_block -> buffer);
  }

  cache_block -> clock = clock;
  memcpy(buffer, cache_block -> buffer, BLOCK_SECTOR_SIZE);

  lock_release(&cache_lock);
}

void 
write_cache(block_sector_t sector_idx, void *buffer)
{
  lock_acquire(&cache_lock);
  ++ clock;
  struct cache * cache_block = look_cache(sector_idx);
  if (cache_block == NULL)
  {
    cache_block = give_block_cache();
    cache_block -> sector = sector_idx;
    cache_block -> not_writed = false;
    block_read(fs_device, sector_idx, cache_block -> buffer);
  }

  cache_block -> clock = clock;
  cache_block -> not_writed = true;
  memcpy(cache_block -> buffer, buffer, BLOCK_SECTOR_SIZE);

  lock_release(&cache_lock);
}


/* allocate sectors on disk */
bool 
inode_alloc(struct inode_disk* disk, size_t sector_count)
{

  size_t direct_count = min((size_t)DIRECT_BLOCKS_PER_INODE,sector_count);
  size_t i;
  for(i = 0; i < direct_count; i++)
    if(!allocate_direct_sector(&disk->direct_blocks[i]))
      return false;
  
  
  sector_count -= direct_count;
  if(sector_count==0) 
    return true;

  size_t indirect_count = min(BLOCKS_PER_SECTOR,sector_count);
  if(!allocate_indirect_sectors(&disk->indirect_blocks,indirect_count))
    return false;

  
  sector_count -= indirect_count;
  if(sector_count==0) 
    return true;

  size_t doubly_indirect_count = min(BLOCKS_PER_SECTOR*BLOCKS_PER_SECTOR,sector_count);
  if(!allocate_doubly_indirect_sectors(&disk->doubly_indirect_blocks, doubly_indirect_count))
    return false;
  
  sector_count -= doubly_indirect_count;
  if(sector_count==0) 
    return true;

  
  return false;
}

bool 
allocate_direct_sector(block_sector_t* sector)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  if(*sector == 0)
  {
    if(!free_map_allocate(1,sector))
        return false;
    //block_write(fs_device, *sector, zeros);
    write_cache(*sector, zeros);

  }
  return true;
}

bool 
allocate_indirect_sectors(block_sector_t* sector_ref, size_t sector_count)
{
  if(!allocate_direct_sector(sector_ref))
    return false;
  
  struct indirect_blocks_sector ind_block;

  //block_read(fs_device, *sector_ref, &ind_block);
  read_cache(*sector_ref, &ind_block);

  size_t i;
  for(i = 0; i < sector_count; i++)
    if(!allocate_direct_sector(&ind_block.block[i]))
      return false;

  //block_write(fs_device, *sector_ref, &ind_block);
  write_cache(*sector_ref, &ind_block);

  return true;
}

bool 
allocate_doubly_indirect_sectors(block_sector_t* sector_ref, size_t sector_count)
{
  if(!allocate_direct_sector(sector_ref))
    return false;
  
  struct indirect_blocks_sector ind_block;

  //block_read(fs_device, *sector_ref, &ind_block);
  read_cache(*sector_ref, &ind_block);
  size_t i;
  for(i = 0; i < sector_count / BLOCKS_PER_SECTOR; i++)
  {
    size_t cur_alloc_size = min(BLOCKS_PER_SECTOR, sector_count);
    if(!allocate_indirect_sectors(&ind_block.block[i], cur_alloc_size))
      return false;
    sector_count -= cur_alloc_size;
  }

  //block_write(fs_device, *sector_ref, &ind_block);
  write_cache(*sector_ref, &ind_block);

  return true;
}

void
deallocate_inode(struct inode* inode)
{
  size_t sector_count = bytes_to_sectors(inode->data.length);

  size_t direct_count = min((size_t)DIRECT_BLOCKS_PER_INODE,sector_count);
  size_t i;
  for(i = 0; i < direct_count; i++)
    deallocate_direct_sector(inode->data.direct_blocks[i]);
  
  
  sector_count -= direct_count;
  if(sector_count==0) 
    return;

  size_t indirect_count = min(BLOCKS_PER_SECTOR,sector_count);
  deallocate_indirect_sectors(inode->data.indirect_blocks,indirect_count);

  
  sector_count -= indirect_count;
  if(sector_count==0) 
    return;

  size_t doubly_indirect_count = min(BLOCKS_PER_SECTOR*BLOCKS_PER_SECTOR,sector_count);
  deallocate_doubly_indirect_sectors(inode->data.doubly_indirect_blocks, doubly_indirect_count);

  sector_count -= doubly_indirect_count;
  ASSERT(sector_count == 0)

}



bool 
deallocate_direct_sector(block_sector_t sector)
{
  free_map_release(sector,1);
}

bool 
deallocate_indirect_sectors(block_sector_t sector_ref, size_t sector_count)
{
  
  struct indirect_blocks_sector ind_block;

  //block_read(fs_device, sector_ref, &ind_block);
  read_cache(sector_ref,  &ind_block);
  size_t i;
  for(i = 0; i < sector_count; i++)
    deallocate_direct_sector(ind_block.block[i]);
  
  deallocate_direct_sector(sector_ref);
}

bool 
deallocate_doubly_indirect_sectors(block_sector_t sector_ref, size_t sector_count)
{
  struct indirect_blocks_sector ind_block;

  //block_read(fs_device, sector_ref, &ind_block);
  read_cache(sector_ref, &ind_block);
  size_t i;
  for(i = 0; i < sector_count / BLOCKS_PER_SECTOR; i++)
  {
    size_t cur_dealloc_size = min(BLOCKS_PER_SECTOR, sector_count);
    deallocate_indirect_sectors(ind_block.block[i], cur_dealloc_size);
    sector_count -= cur_dealloc_size;
  }

  deallocate_direct_sector(sector_ref);
}

bool
inode_is_dir(struct inode* inode)
{
  return inode->data.is_dir;
}
