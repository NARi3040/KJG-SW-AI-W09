/* 시스템 콜을 사용하여 code segment에 쓰기를 시도함.
   프로세스는 종료 코드 -1로 종료되어야 함. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  int handle;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
  read (handle, (void *) test_main, 1);
  fail ("survived reading data into code segment");
}
