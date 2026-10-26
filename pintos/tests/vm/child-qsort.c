/* 128 kB 파일을 stack으로 읽어 들이고, 여러 번의 패스를 거치는
   분할 정복 알고리즘인 퀵 정렬로 그 안의 바이트를 "정렬"함.
   정렬된 데이터는 같은 파일에 제자리(in-place)로 다시 기록됨. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

int
main (int argc UNUSED, char *argv[]) 
{
  test_name = "child-qsort";

  int handle;
  unsigned char buf[128 * 1024];
  size_t size;

  quiet = true;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  size = read (handle, buf, sizeof buf);
  qsort_bytes (buf, sizeof buf);
  seek (handle, 0);
  write (handle, buf, size);
  close (handle);
  
  return 72;
}
