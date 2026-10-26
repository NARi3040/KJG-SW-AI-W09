/* 길이가 0인 파일의 매핑을 시도함. 성공할 수도 있고 실패할 수도 있지만
   프로세스가 종료되거나 crash 나서는 안 됨.
   그 다음 매핑을 시도했던 주소를 역참조하며, 프로세스는
   종료 코드 -1로 종료되어야 함. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *data = (char *) 0x7f000000;
  int handle;

  CHECK (create ("empty", 0), "create empty file \"empty\"");
  CHECK ((handle = open ("empty")) > 1, "open \"empty\"");

  /* mmap() 호출은 성공할 수도 실패할 수도 있음. 어느 쪽이든 상관없음. */
  msg ("mmap \"empty\"");
  mmap (data, 0, 0, handle, 0);

  /* 호출이 성공했는지와 무관하게, *data 접근은
     프로세스를 종료시켜야 함. */
  fail ("unmapped memory is readable (%d)", *data);
}

