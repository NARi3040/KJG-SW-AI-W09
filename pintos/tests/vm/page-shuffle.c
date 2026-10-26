/* 128 kB 데이터 버퍼를 10번 섞으며,
   매번 섞은 뒤에 checksum을 출력함. */

#include <stdbool.h>
#include "tests/arc4.h"
#include "tests/cksum.h"
#include "tests/lib.h"
#include "tests/main.h"

#define SIZE (128 * 1024)

static char buf[SIZE];

void
test_main (void)
{
  size_t i;

  /* 초기화함. */
  for (i = 0; i < sizeof buf; i++)
    buf[i] = i * 257;
  msg ("init: cksum=%lu", cksum (buf, sizeof buf));
    
  /* 반복적으로 섞음. */
  for (i = 0; i < 10; i++)
    {
      shuffle (buf, sizeof buf, 1);
      msg ("shuffle %zu: cksum=%lu", i, cksum (buf, sizeof buf));
    }
}
