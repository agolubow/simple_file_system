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

typedef enum { false, true } bool;

typedef struct
{
    bool is_used;
    int fileIndex;
    int offset;
} fileDescriptor;

typedef struct
{
    int dirOffset;
    int beginBlock;
    int endBlock;
    int filesize;
    char filename[MAX_FILENAME_LEN]; //extra character for null byte.
} fatStruct;

typedef struct
{
    bool is_used;
    int fileIndex;
    int offset;
    fatStruct fileInfo;
} fileDescriptor;

typedef struct
{
    char superBlockInfo[30];
    int directoryOffset;
    int FATOffset;
    int dataBlockOffset;
    int numOfFiles;
    int directoryBlock;
    int FATBlock;
    int dataBlock;
    int numFreeBlocks;
    char virtualFat[MAX_FAT_LEN];
    char virtualDirectory[MAX_DIR_LEN];
    fileDescriptor fileDescriptors[MAX_FILE_DESC];
    fatStruct fatStucts[MAX_FILE_LIMIT];
} metaInfo;


static metaInfo * fs_metaInfo = NULL;
/******************************************************************************/

/*
  FILE ALLOCATION TABLE STRUCTURE

  flat array. each file created requires 20 bytes of info to be recorded in The
  array. first 8 bytes are filesize. next 4 bytes is the beginning block number.
  next 4 bytes are the ending block number. the remaining 4 bytes is the the
  filename offset in the directory.
*/
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
  int multiplier = 1;
  int n = sizeof(array);
  /* for each character in array */
  while (n--)
  {
      /* check if number > 0, break or continue */
      if (array[n] < '0' || array[n] > '9') {
          if (number){
              break;
          } else {
              continue;
          }
      }
      /* convert digit to numeric value   */
      number += (array[n] - '0') * multiplier;
      multiplier *= 10;
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
        the next two bytes are the number of files. max files = 64. the remaining
        4 bytes is the number of free blocks.
    */
    char init_buf[] = "CPSC3514096819216777216004096\n";
    if( write(handle, init_buf, 29) < 0 ) {
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

    if (read(handle, fs_metaInfo->superBlockInfo, 29) < 0) {
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
      char numOfFreeBlocks[4];

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
      j = 0;
      while( i < 29 ){
        numOfFreeBlocks[j] = fs_metaInfo->superBlockInfo[i];
        ++i;
        ++j;
      }
      numOfFiles[j] = '\0';

      /* convert offsets to actual integers to be used for indexing array */
      fs_metaInfo->directoryOffset = atoi(directoryOffset);
      fs_metaInfo->FATOffset = atoi(FATOffset);
      fs_metaInfo->dataBlockOffset = atoi(dataBlockOffset);
      fs_metaInfo->numOfFiles = atoi(numOfFiles);
      fs_metaInfo->numFreeBlocks = atoi(numOfFreeBlocks);

      /* get block number to pass to block_write */
      fs_metaInfo->directoryBlock = fs_metaInfo->directoryOffset / BLOCK_SIZE;
      fs_metaInfo->FATBlock = fs_metaInfo->FATOffset / BLOCK_SIZE;
      fs_metaInfo->dataBlock = fs_metaInfo->dataBlockOffset / BLOCK_SIZE;

      /* initialize file descriptor array */
      for(i = 0; i < MAX_FILE_DESC; i++ ){
        fs_metaInfo->fileDescriptors[i].is_used = false;
      }

      /* get directory information */
      if(lseek(handle, fs_metaInfo->directoryOffset, SEEK_SET) < 0 ){
        perror("mount_fs: failed directory lseek");
        return -1;
      }

      if( read(handle, fs_metaInfo->virtualDirectory, MAX_DIR_LEN < 0) ){
        perror("mount_fs: failed to read directory data");
        return -1;
      }

      /* get FAT information */
      if( lseek(handle, fs_metaInfo->FATOffset, SEEK_SET) < 0 ){
        perror("mount_fs: failed FAT lseek");
        return -1;
      }

      if( read(handle, fs_metaInfo->virtualFat, MAX_FAT_LEN) < 0 ){
        perror("mount_fs: failed to read FAT data");
        return -1;
      }

      int j;
      int k;
      char filesizeBuf[8];
      char blockBeginBuf[4];
      char blockEndBuf[4];
      char dirOffsetBuf[4];
      int fileCount = 0;
      int tempOffset;
      for( i = 0; i < MAX_FAT_LEN; i+=20 ){
          if( fs_metaInfo->virtualFat[i] == '\0' ){
              fs_metaInfo->fatStucts[fileCount].dirOffset = -1;
          } else {
              for( j = 0; j < 20; j++ ){
                  if( j < 8 ){
                      filesizeBuf[j] = fs_metaInfo->virtualFat[ i + j ];
                  } else if( j < 12 ){
                      blockBeginBuf[ j - 8 ] = fs_metaInfo->virtualFat[ i + j ];
                  } else if( j < 16 ){
                      blockEndBuf[ j - 12 ] = fs_metaInfo->virtualFat[ i + j ];
                  } else {
                      dirOffsetBuf[ j - 16 ] = fs_metaInfo->virtualFat[ i + j ];
                  }
              }
              fs_metaInfo->fatStucts[fileCount].dirOffset = atoi(dirOffsetBuf);
              fs_metaInfo->fatStucts[fileCount].beginBlock = atoi(blockBeginBuf);
              fs_metaInfo->fatStucts[fileCount].endBlock = atoi(blockEndBuf);
              fs_metaInfo->fatStucts[fileCount].filesize = atoi(filesizeBuf);
              tempOffset = fs_metaInfo->fatStucts[fileCount].dirOffset;
              for( k = 0; k < MAX_FILENAME_LEN; k++ ){
                  fs_metaInfo->fatStucts[fileCount].filename[k] = fs_metaInfo->virtualDirectory[ tempOffset + k ];
              }
          }
          fileCount++;
      }

      printf("\nInteger: %d", fs_metaInfo->directoryOffset);
      printf("\nInteger: %d", fs_metaInfo->FATOffset);
      printf("\nInteger: %d", fs_metaInfo->dataBlockOffset);
      printf("\nInteger: %d", fs_metaInfo->numOfFiles);
      printf("\nInteger: %d", fs_metaInfo->numFreeBlocks);

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
  if( write(handle, fs_metaInfo->superBlockInfo, 29) < 0 ) {
    perror("unmount_fs: failed to save metadata");
    return -1;
  }

  if(lseek(handle, fs_metaInfo->directoryOffset, SEEK_SET) < 0 ){
    perror("unmount_fs: failed directory lseek");
    return -1;
  }

  if( write(handle, fs_metaInfo->virtualDirectory, MAX_DIR_LEN < 0) ){
    perror("unmount_fs: failed to write directory data");
    return -1;
  }

  /* get FAT information */
  if( lseek(handle, fs_metaInfo->FATOffset, SEEK_SET) < 0 ){
    perror("unmount_fs: failed FAT lseek");
    return -1;
  }

  if( write(handle, fs_metaInfo->virtualFat, MAX_FAT_LEN) < 0 ){
    perror("unmount_fs: failed to write FAT data");
    return -1;
  }
  close_disk();
  free(fs_metaInfo);
  return 0;
}

int fs_get_filesize(int fildes){
    if(!fs_metaInfo->fileDescriptors[fildes].is_used){
        fprintf(stderr, "error: Invalid file descriptor.\n");
        return -1;
    }
    return fs_metaInfo->fileDescriptors[fildes].fileInfo.filesize;
}



int find_file(char* name)
{
    int i;
    for(i = 0; i < MAX_FILE_LIMIT; i++ ) {

        if( fs_metaInfo->fatStucts[i].dirOffset == -1 ){
            continue;
        }
        if( strcmp(fs_metaInfo->fatStucts[i].filename, name) == 0){
            return i;  // return the FAT array offset
        }
    }

    return -1;         // file not found
}

/* find free directory spot */
int findFreeDirectoryOffset(){
    int i;
    for( i = 0; i < MAX_DIR_LEN; i += 16 ){
        if( fs_metaInfo->virtualDirectory[i] == '\0' ){
            return i;
        }
    }
    return -1;
}

/* find free block to create file */
int findFreeBlock(){
    int bufferSize = (DISK_BLOCKS/2) * BLOCK_SIZE;
    char dataBlocks = malloc((DISK_BLOCKS/2) * BLOCK_SIZE );

    if( lseek(handle, (DISK_BLOCKS/2), SEEK_SET) < 0 ) {
      perror("find_block: failed to lseek.");
      return -1;
    }

    if( read(handle, dataBlocks, bufferSize) < 0 ) {
      perror("unmount_fs: failed to read data blocks");
      return -1;
    }

    for( i = 0; i < bufferSize; i += BLOCK_SIZE ){
        if( dataBlocks[i] == '\0' ){
            return i;
        }
    }
    return -1;
}

int fs_create(char *name)
{
    if( sizeof name > 15 ){
      perror("fs_create: filename must not exceed 15 characters.\n");
      return -1;
    }

    int dirOffset = find_file(name);

    if( dirOffset < 0 ){  // Create file
        int i;
        for(i = 0; i < MAX_FILE_LIMIT; i++) {
            if( fs_metaInfo->fatStucts[i].dirOffset == -1 ) {
                fs_metaInfo->numOfFiles++;
                /* Initialize file information */
                fs_metaInfo->fatStucts[i].dirOffset = findFreeDirectoryOffset();
                fs_metaInfo->fatStucts[i].beginBlock = findFreeBlock();
                fs_metaInfo->fatStucts[i].endBlock = fs_metaInfo->fatStucts[i].beginBlock;
                strcpy(fs_metaInfo->fatStucts[i].filename, name);
                fs_metaInfo->numOfFiles++;
                fs_metaInfo->numFreeBlocks--;
                int j;
                for( j = 0; j < 15; j++){
                    fs_metaInfo->virtualDirectory[ fs_metaInfo->fatStucts[i].dirOffset + j ] = name[j];
                }
                fs_metaInfo->virtualDirectory[]
                printf("fs_create: success. file [%s] created.\n", name);
                return 0;
            }
        }
        perror("fs_create: maximum number of files has been reached.\n");
        return -1;
    } else {              // File already exists
        perror("fs_create: file already exists.\n");
        return -1;
    }
}

int fs_close(int fildes){
    if( fs_metaInfo->fileDescriptors[fildes].is_used == false ){
        return -1;
    }
    fs_metaInfo->fileDescriptors[fildes].is_used = false;
    fs_metaInfo->fileDescriptors[fildes].fileIndex = -1;
    fs_metaInfo->fileDescriptors[fildes].offset = -1;
    return 0;
}
