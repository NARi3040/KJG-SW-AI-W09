/* child-mm-wrt를 실행하고, 발생했어야 할 쓰기가
   실제로 발생했는지 검증함. */

#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  pid_t child;

  /* 자식이 파일에 쓰게 함. */
  quiet = true;
	child = fork("child-mm-wrt");
	if (child == 0) {
		CHECK ((child = exec ("child-mm-wrt")) != -1, "exec \"child-mm-wrt\"");
	} else {
		CHECK (wait (child) == 0, "wait for child (should return 0)");
		quiet = false;
		
		/* 파일 내용을 확인함. */
		check_file ("sample.txt", sample, sizeof sample);
	} 
}
