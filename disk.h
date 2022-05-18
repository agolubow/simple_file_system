#ifndef _DISK_H_
#define _DISK_H_

/******************************************************************************/
#define DISK_BLOCKS  8192      /* number of blocks on the disk                */
#define BLOCK_SIZE   4096      /* block size on "disk"                       */
// Your library must support a maximum of 32 file descriptors that can be open simultaneously
#define MAX_FILE_DESC 32
// The maximum length for a file name is 15 characters
#define MAX_FILENAME_LEN 16 //extra character for null terminator
// Your file system does not have to store more than 64 files
#define MAX_FILE_LIMIT 64

#define MAX_DIR_LEN 1024

#define MAX_FAT_LEN 1280

/******************************************************************************/
int make_disk(char *name);     /* create an empty, virtual disk file          */
int open_disk(char *name);     /* open a virtual disk (file)                  */
int close_disk();              /* close a previously opened disk (file)       */

int block_write(int block, char *buf);
                               /* write a block of size BLOCK_SIZE to disk    */
int block_read(int block, char *buf);
                               /* read a block of size BLOCK_SIZE from disk   */
int convertToInt(char *array);
int make_fs(char *disk_name);
int initialize_fs();
int unmount_fs();
int mount_fs();
int find_file(char* name);
int fs_get_filesize(int fildes)
/******************************************************************************/

#endif
