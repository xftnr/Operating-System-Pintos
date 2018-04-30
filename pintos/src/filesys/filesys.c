#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

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

char *
get_name(const char *path) {
  char *s = NULL;             /* Temperary string to hold the path */
  char *prev_token = "";         /* Previous tokenized argument */
  char *token = NULL;         /* Tokenized argument */
  char *save_ptr = NULL;      /* Saved position in String */
  char *file_name = NULL;         /* Tokenized argument */

  s = malloc (strlen(path) + 1);
  strlcpy(s, path, strlen(path) + 1);

  for (token = strtok_r (s, "/", &save_ptr); token != NULL;
  token = strtok_r (NULL, "/", &save_ptr)) {
    prev_token = token;
  }

  file_name = malloc (strlen(prev_token) + 1);
  strlcpy(file_name, prev_token, strlen(prev_token) + 1);

  // printf("path %s\n\n", s);
  //
  // printf("name %s\n\n", file_name);

  free(s);

  return file_name;
}

bool
filesys_chdir (const char *path) {
  struct dir *dir = get_dir(path);
  if (dir == NULL) {
    return false;
  }
  dir_close(thread_current()->current_directory);
  thread_current()->current_directory = dir;
  return true;
}


/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *path, off_t initial_size, bool isdir)
{
  block_sector_t inode_sector = 0;
  char *file_name = get_name(path);
  char *dir_path = malloc(strlen(path) - strlen(file_name) + 1);
  strlcpy(dir_path, path, strlen(path) - strlen(file_name) + 1);
  struct dir *parent_dir = get_dir(dir_path);
  // struct dir *dir = dir_open_root ();
  bool success = (parent_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, isdir)
                  && dir_add (parent_dir, file_name, inode_sector));
  if (success && isdir) {
    struct inode *inode = inode_open (inode_sector);

    struct dir *child_dir = dir_open(inode);
    if (! dir_add(child_dir, ".", inode_sector)) {
      PANIC ("directory adding . failed");
    }
    if (! dir_add(child_dir, "..", inode_get_inumber(dir_get_inode(parent_dir)))) {
      PANIC ("directory adding .. failed");
    }
    dir_close (child_dir);
    inode_close (inode);
  }

  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);
  dir_close (parent_dir);

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
  char *file_name = get_name(path);
  char *dir_path = malloc(strlen(path) - strlen(file_name) + 1);
  strlcpy(dir_path, path, strlen(path) - strlen(file_name) + 1);
  struct dir *dir = get_dir(dir_path);

  if (dir == NULL) {
    return NULL;
  }

  struct inode *inode = NULL;
  if (strlen(file_name) == 0) {
    inode = dir_get_inode(dir);
  } else {
    dir_lookup (dir, file_name, &inode);
    dir_close (dir);
  }

  if (inode == NULL || inode_is_removed(inode)) {
    return NULL;
  }

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *path)
{
  char *file_name = get_name(path);
  char *dir_path = malloc(strlen(path) - strlen(file_name) + 1);
  strlcpy(dir_path, path, strlen(path) - strlen(file_name) + 1);
  struct dir *dir = get_dir(dir_path);

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

  // Add . and .. to root directory
  struct dir * dir = dir_open_root();
  if (! dir_add(dir, ".", ROOT_DIR_SECTOR)) {
    PANIC ("root directory adding . failed");
  }
  if (! dir_add(dir, "..", ROOT_DIR_SECTOR)) {
    PANIC ("root directory adding .. failed");
  }
  dir_close (dir);
  free_map_close ();
  printf ("done.\n");
}
