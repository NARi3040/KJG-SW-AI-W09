/* 주소 0에서의 메모리 매핑이 허용되지 않음을 검증함. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap (NULL, 4096, 0, handle, 0) == MAP_FAILED, "try to mmap at address 0");
}

