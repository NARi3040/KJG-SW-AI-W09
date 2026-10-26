/* 콘솔 입력의 파일 디스크립터인 fd 0으로 mmap을 시도함.
   mmap은 조용히 실패하거나 프로세스를 종료 코드 -1로 종료해야 함. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  CHECK (mmap ((void *) 0x10000000, 4096, 0, 0, 0) == MAP_FAILED,
         "try to mmap stdin");
}

