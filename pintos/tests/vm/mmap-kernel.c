/* kernel 영역 위로의 매핑이 허용되지 않음을 검증함. */

#include <stdint.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  
  void *kernel = (void *) 0x8004000000;
  CHECK (mmap (kernel, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap over kernel 0");

  kernel = (void *) 0xfffffffffffff000;
  CHECK (mmap (kernel, 0x2000, 0, handle, 0) == MAP_FAILED,
         "try to mmap over kernel 1");

  kernel = (void *) 0x8004000000 - 0x1000;
  CHECK (mmap (kernel, -0x8004000000 + 0x1000, 0, handle, 0) == MAP_FAILED,
         "try to mmap over kernel 2");

}
