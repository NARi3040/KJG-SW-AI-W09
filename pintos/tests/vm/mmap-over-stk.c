/* stack segment 위로의 매핑이 허용되지 않음을 검증함. */

#include <stdint.h>
#include <round.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  int handle;
  uintptr_t handle_page = ROUND_DOWN ((uintptr_t) &handle, 4096);
  
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  CHECK (mmap ((void *) handle_page, 4096, 0, handle, 0) == MAP_FAILED,
         "try to mmap over stack segment");
}

