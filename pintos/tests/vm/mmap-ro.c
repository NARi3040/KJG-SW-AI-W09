/* 매핑을 통해 파일에 쓰고 파일을 unmap한 뒤,
   read 시스템 콜로 파일의 데이터를 다시 읽어 검증함. */

#include <string.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

#define ACTUAL ((void *) 0x10000000)

void
test_main (void)
{
  int handle;
  void *map;
  char buf[1024];

  /* mmap을 통해 파일에 씀. */
  CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");
  CHECK ((map = mmap (ACTUAL, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"large.txt\" with writable=0");
  msg ("about to write into read-only mmap'd memory");
  *((int *)map) = 0;
  msg ("Error should have occured");
  munmap (map);
  close (handle);
}
