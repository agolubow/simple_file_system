#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "disk.h"

int main(){

  //make_disk("test_disk");
  //open_disk("test_dis");
  //char * file_system = malloc(36503552);

  int i;

  make_fs("test_disk");
  close_disk();

  printf("opened array\n");

  return 0;
}
