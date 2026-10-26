/* mmap된 영역은 메모리에서 데이터가 실제로 수정된 경우에만
   munmap 시점에 파일로 다시 기록되는지 검증함. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  static const char overwrite[] = "Now is the time for all good...";
  static char buffer[sizeof sample - 1];
  char *actual = (char *) 0x54321000;
  int handle;
  void *map;

  /* 파일을 열고, 매핑하고, 데이터를 검증함. */
  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");
 
	CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");
  if (memcmp (actual, sample, strlen (sample)))
    fail ("read of mmap'd file reported bad data");

  /* 파일을 수정함. */
  CHECK (write (handle, overwrite, strlen (overwrite))
         == (int) strlen (overwrite),
         "write \"sample.txt\"");

  /* 매핑을 닫음. 매핑을 통해 수정하지 않았으므로
     데이터가 다시 기록되어서는 안 됨. */
  msg ("munmap \"sample.txt\"");
  munmap (map);

  /* 파일을 다시 읽음. */
  msg ("seek \"sample.txt\"");
  seek (handle, 0);
  CHECK (read (handle, buffer, sizeof buffer) == sizeof buffer,
         "read \"sample.txt\"");

  /* 파일 덮어쓰기가 제대로 되었는지 검증함. */
  if (memcmp (buffer, overwrite, strlen (overwrite))
      || memcmp (buffer + strlen (overwrite), sample + strlen (overwrite),
                 strlen (sample) - strlen (overwrite))) 
    {
      if (!memcmp (buffer, sample, strlen (sample)))
        fail ("munmap wrote back clean page");
      else
        fail ("read surprising data from file"); 
    }
  else
    msg ("file change was retained after munmap");
}
