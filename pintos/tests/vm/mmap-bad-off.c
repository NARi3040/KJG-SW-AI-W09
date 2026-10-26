/* 유효하지 않은 offset으로 mmap을 시도함.
   조용히 실패하거나 프로세스를 종료 코드 -1로 종료해야 함. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");

  CHECK (mmap ((void *) 0x10000000, 4096, 0, handle, 0x1234) == MAP_FAILED,
         "try to mmap invalid offset");
}
