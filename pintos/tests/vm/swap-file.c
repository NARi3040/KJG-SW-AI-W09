/* 파일 매핑 page가 swap out 및 swap in 되는 과정이 올바른지 확인함
 * 이 테스트에서 Pintos 메모리 크기는 128MB임
 * 먼저 메모리를 anonymous page로 가득 채운 뒤,
 * page에 파일을 매핑하려고 시도함 */

#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include <stdint.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/large.inc"

void
test_main (void) 
{
    size_t handle;
    char *actual = (char *) 0x10000000;
    void *map;
    size_t i;

    /* page를 파일에 매핑함. */
    CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");
    CHECK ((map = mmap (actual, sizeof(large), 0, handle, 0)) != MAP_FAILED, "mmap \"large.txt\"");

    /* 데이터가 올바른지 확인함. */
    if (memcmp (actual, large, strlen (large)))
        fail ("read of mmap'd file reported bad data");

    /* 데이터 뒤가 0으로 채워져 있는지 검증함. */
    size_t len = strlen(large);
    size_t page_end;
    for(page_end = 0; page_end < len; page_end+=4096);

    for (i = len+1; i < page_end; i++) 
    {
        if (actual[i] != 0) {
            fail ("byte %zu of mmap'd region has value %02hhx (should be 0)", i, actual[i]);
        }
    }

    /* 열어 둔 파일을 unmap하고 닫음. */ 
    munmap (map);
    close (handle);
}

