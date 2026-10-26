/* code segment에 쓰기를 시도함.
   프로세스는 종료 코드 -1로 종료되어야 함. */

#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  *(int *) test_main = 0;
  fail ("writing the code segment succeeded");
}
