#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "disk.h"

int main(){

  make_fs("test_disk");

  mount_fs("test_disk");

  unmount_fs("test_disk");

  return 0;
}
