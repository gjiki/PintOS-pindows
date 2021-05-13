#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *path, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *path);
bool filesys_remove (const char *path);

void path_get_file_name(char* name, char* file_name);
bool path_get_dir_name(char* name, char* dir_name);

#endif /* filesys/filesys.h */
