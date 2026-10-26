/* 모든 종류의 메모리가 swap out 및 swap in 되는 과정이 올바른지 확인함
 * 이 테스트에서 Pintos memory size는 128MB임 */ 

#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/large.inc"


#define PAGE_SHIFT 12
#define PAGE_SIZE (1 << PAGE_SHIFT)
#define ONE_MB (1 << 20) // 1MB
#define CHUNK_SIZE (20*ONE_MB)
#define PAGE_COUNT (CHUNK_SIZE / PAGE_SIZE)

static char big_chunks[CHUNK_SIZE];

void
test_main (void)
{
    size_t i, handle;
    char *actual = (char *) 0x10000000;
    void *map;

    for (i = 0 ; i < PAGE_COUNT ; i++) {
        if ((i & 0x1ff) == 0)
            msg ("write sparsely over page %zu", i);
        big_chunks[i*PAGE_SIZE] = (char) i;
    }

    CHECK ((handle = open ("large.txt")) > 1, "open \"large.txt\"");
    CHECK ((map = mmap (actual, sizeof(large), 0, handle, 0)) != MAP_FAILED, "mmap \"large.txt\"");

    /* 파일 매핑 page를 읽음. */
    if (memcmp (actual, large, strlen (large)))
        fail ("read of mmap'd file reported bad data");


    /* anonymous page를 읽음. */
    for (i = 0; i < PAGE_COUNT; i++) {
        if (big_chunks[i*PAGE_SIZE] != (char) i)
            fail ("data is inconsistent");
        if ((i & 0x1ff) == 0)
            msg ("check consistency in page %zu", i);
    }

    /* 파일 매핑 page를 다시 확인함. */
    if (memcmp (actual, large, strlen (large)))
        fail ("read of mmap'd file reported bad data");

    /* 열어 둔 파일을 unmap하고 닫음. */
    munmap (map);
    close (handle);
}

