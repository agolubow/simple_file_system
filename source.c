#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "disk.h"

/******************************************************************************/
static int active = 0;  /* is the virtual disk open (active) */
static int handle;      /* file handle to virtual disk       */
static char * meta_info = NULL;
typedef struct metaInfo
{
    char superBlockInfo[26];
    int directoryOffset;
    int FATOffset;
    int dataBlockOffset;
    int numOfFiles;
    int directoryBlock;
    int FATBlock;
    int dataBlock;
    char virtualFat[1280];
    char virtualDirectory[1024];
    int fileDescriptors[64];
} metaInfo;

static metaInfo * fs_metaInfo = NULL;
/******************************************************************************/
int make_disk(char *name)
{
  int f, cnt;
  char buf[BLOCK_SIZE];

  if (!name) {
    fprintf(stderr, "make_disk: invalid file name\n");
    return -1;
  }

  if ((f = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
    perror("make_disk: cannot open file");
    return -1;
  }

  memset(buf, 0, BLOCK_SIZE);
  for (cnt = 0; cnt < DISK_BLOCKS; ++cnt)
    write(f, buf, BLOCK_SIZE);

  close(f);

  return 0;
}

int open_disk(char *name)
{
  int f;

  if (!name) {
    fprintf(stderr, "open_disk: invalid file name\n");
    return -1;
  }

  if (active) {
    fprintf(stderr, "open_disk: disk is already open\n");
    return -1;
  }

  if ((f = open(name, O_RDWR, 0644)) < 0) {
    perror("open_disk: cannot open file");
    return -1;
  }

  handle = f;
  active = 1;

  return 0;
}

int close_disk()
{
  if (!active) {
    fprintf(stderr, "close_disk: no open disk\n");
    return -1;
  }

  close(handle);

  active = handle = 0;

  return 0;
}

int block_write(int block, char *buf)
{
  if (!active) {
    fprintf(stderr, "block_write: disk not active\n");
    return -1;
  }

  if ((block < 0) || (block >= DISK_BLOCKS)) {
    fprintf(stderr, "block_write: block index out of bounds\n");
    return -1;
  }

  if (lseek(handle, block * BLOCK_SIZE, SEEK_SET) < 0) {
    perror("block_write: failed to lseek");
    return -1;
  }

  if (write(handle, buf, BLOCK_SIZE) < 0) {
    perror("block_write: failed to write");
    return -1;
  }

  return 0;
}

int block_read(int block, char *buf)
{
  if (!active) {
    fprintf(stderr, "block_read: disk not active\n");
    return -1;
  }

  if ((block < 0) || (block >= DISK_BLOCKS)) {
    fprintf(stderr, "block_read: block index out of bounds\n");
    return -1;
  }

  if (lseek(handle, block * BLOCK_SIZE, SEEK_SET) < 0) {
    perror("block_read: failed to lseek");
    return -1;
  }

  if (read(handle, buf, BLOCK_SIZE) < 0) {
    perror("block_read: failed to read");
    return -1;
  }

  return 0;
}

int convertToInt(char *array) {
  int number = 0;
  int mult = 1;
  int n = sizeof(array);
  /* for each character in array */
  while (n--)
  {
      /* if not digit or '-', check if number > 0, break or continue */
      if (array[n] < '0' || array[n] > '9') {
          if (number){
              break;
          } else {
              continue;
          }
      }                   /* convert digit to numeric value   */
      number += (array[n] - '0') * mult;
      mult *= 10;
  }

  return number;
}

int initialize_fs()
{
    if( active != 1 ){
      perror("initialize_fs: failed to initialize. No disk available");
      return -1;
    }

    /*
        The first 7 bytes are a signature to show that the filesystem has been
        initialized. second 4 bytes are the directory offset. it is 4096, meaning
        it starts at block 2. third 4 bytes is the File Allocation Table offset.
        it is 8192, meaning it starts at block 3. the next 8 bytes are the
        data blocks offset. it starts at 16777216, which is the 4096th block.
        the remaining two bytes are the number of files. max files = 64.
    */
    char init_buf[] = "CPSC351409681921677721600\n";
    if( write(handle, init_buf, 25) < 0 ) {
      perror("initialize_fs: failed to initialize");
      return -1;
    }

    return 0;
}

/* create file system */
int make_fs(char *disk_name)
{

  /* create disk */
  if( make_disk(disk_name) == 0 ){

    /* open disk */
    if( open_disk(disk_name) == 0 ){

      //initialize filesystem here.
      initialize_fs();
      close_disk();

    } else {
      perror("make_fs: failed to create file system. open file failure");
      return -1;
    }

  } else {
    perror("make_fs: failed to create file system due to disk creation failure");
    return -1;
  }
  return 0;
}

int mount_fs(char *disk_name)
{
    open_disk(disk_name);
    fs_metaInfo = malloc(sizeof(metaInfo));

    if (read(handle, fs_metaInfo->superBlockInfo, 25) < 0) {
      perror("mount_fs: failed to mount file system");
      return -1;
    }

    char fs_init_signature[7];
    int i;
    int j;
    for( i = 0; i < 7; i++ ){
      fs_init_signature[i] = fs_metaInfo->superBlockInfo[i];
    }

    if(strncmp(fs_init_signature, "CPSC351", 7) != 0){
  		perror("mount_fs: file system was not initialized.");
  		return -1;
  	} else {

      char directoryOffset[5];
      char FATOffset[5];
      char dataBlockOffset[9];
      char numOfFiles[3];

      j = 0;
      while( i < 11 ){
        directoryOffset[j] = fs_metaInfo->superBlockInfo[i];
        ++i;
        ++j;
      }
      directoryOffset[j] = '\0';
      j = 0;
      while( i < 15 ){
        FATOffset[j] = fs_metaInfo->superBlockInfo[i];
        ++i;
        ++j;
      }
      FATOffset[j] = '\0';
      j = 0;
      while( i < 23 ){
        dataBlockOffset[j] = fs_metaInfo->superBlockInfo[i];
        ++i;
        ++j;
      }
      dataBlockOffset[j] = '\0';
      j = 0;
      while( i < 25 ){
        numOfFiles[j] = fs_metaInfo->superBlockInfo[i];
        ++i;
        ++j;
      }
      numOfFiles[j] = '\0';

      /* convert offsets to actual integers to be used for indexing array */
      fs_metaInfo->directoryOffset = atoi(directoryOffset);
      fs_metaInfo->FATOffset = atoi(FATOffset);
      fs_metaInfo->dataBlockOffset = atoi(dataBlockOffset);
      fs_metaInfo->numOfFiles = atoi(numOfFiles);

      /* get block number to pass to block_write */
      fs_metaInfo->directoryBlock = fs_metaInfo->directoryOffset / BLOCK_SIZE;
      fs_metaInfo->FATBlock = fs_metaInfo->FATOffset / BLOCK_SIZE;
      fs_metaInfo->dataBlock = fs_metaInfo->dataBlockOffset / BLOCK_SIZE;

      /* initialize file descriptor array */
      for(i = 0; i < 64; i++ ){
        fs_metaInfo->fileDescriptors[i] = -1;
      }

      printf("\nInteger: %d", fs_metaInfo->directoryOffset);
      printf("\nInteger: %d", fs_metaInfo->FATOffset);
      printf("\nInteger: %d", fs_metaInfo->dataBlockOffset);
      printf("\nInteger: %d", fs_metaInfo->numOfFiles);

      printf("\nblock number: %d", fs_metaInfo->directoryBlock);
      printf("\nblock number: %d", fs_metaInfo->FATBlock);
      printf("\nblock number: %d\n", fs_metaInfo->dataBlock);

    }
    return 0;
}

int unmount_fs(char *disk_name)
{
  if( lseek(handle, 0, SEEK_SET) < 0 ) {
    perror("unmount_fs: failed to lseek. failed to save metadata");
    return -1;
  }
  if( write(handle, fs_metaInfo->superBlockInfo, 25) < 0 ) {
    perror("unmount_fs: failed to save metadata");
    return -1;
  }
  close_disk();
  free(fs_metaInfo);
  return 0;
}
