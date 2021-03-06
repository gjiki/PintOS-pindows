#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format)
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format)
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void)
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, bool is_dir)
{
  char file_name[strlen(path)+2], dir_name_buff[strlen(path)+2];
  char* dir_name;
  if(!path_get_dir_name(path, dir_name_buff))
    dir_name = "";
  else
    dir_name = dir_name_buff;
  
  path_get_file_name(path, file_name);

  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_by_path(dir_name);
  
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}
/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *path)
{

  char file_name[strlen(path)+2], dir_name_buff[strlen(path)+2];
  char* dir_name;
  if(!path_get_dir_name(path, dir_name_buff))
    dir_name = "";
  else
    dir_name = dir_name_buff;
  
  path_get_file_name(path, file_name);

  struct dir *dir = dir_open_by_path(dir_name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path)
{
  char file_name[strlen(path)+2], dir_name_buff[strlen(path)+2];
  char* dir_name;
  if(!path_get_dir_name(path, dir_name_buff))
    dir_name = "";
  else
    dir_name = dir_name_buff;
  
  path_get_file_name(path, file_name);
  struct dir *dir = dir_open_by_path (dir_name);
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir);

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

void 
path_get_file_name(char* name, char* file_name)
{   
    int i;
    int index = -1;
    for (i = strlen(name) - 1; i >= 0; i--)
    {
        char* symbol = name + i;
        if (*symbol == '/')
        {
            index = i;
            break;
        }
    }

    for (i = index + 1; i < strlen(name); i++)
    {
        char* symbol = name + i;
        file_name[i - index - 1] = *symbol;
    }
    file_name[strlen(name) - index - 1] = '\0';

    return file_name;
}

bool 
path_get_dir_name(char* name, char* dir_name)
{
    int i;
    int index = -1;
    for (i = strlen(name) - 1; i >= 0; i--)
    {
        char* symbol = name + i;
        if (*symbol == '/')
        {
            index = i;
            break;
        }
    }
    if(index == -1) return false;

    for (i = 0; i < index; i++)
    {
        char* symbol = name + i;
        dir_name[i] = *symbol;
    }
    dir_name[index] = '\0';

    return true;
}