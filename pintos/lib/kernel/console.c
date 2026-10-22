#include <console.h>
#include <stdarg.h>
#include <stdio.h>
#include "devices/serial.h"
#include "devices/vga.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/synch.h"

static void vprintf_helper (char, void *);
static void putchar_have_lock (uint8_t c);

/* 콘솔 락(console lock).
   vga 계층과 serial 계층은 각자 락을 처리하므로 언제 호출해도
   안전함.
   하지만 이 락은 printf() 호출이 동시에 일어나 출력이 뒤섞이는
   것을 막는 데 유용하며, 뒤섞인 출력은 보기에 혼란스러움. */
static struct lock console_lock;

/* 평상시에는 true: 위에서 설명한 대로 스레드 간 출력이 섞이지
   않도록 콘솔 락을 사용하려는 것임.

   부팅 초기에 락이 동작하기 전이거나 콘솔 락이 초기화되기 전,
   또는 커널 패닉(panic) 이후에는 false.  앞의 경우 락을 잡으면
   assertion 실패가 일어나고, 그것이 다시 패닉을 일으켜 뒤의
   경우가 되어 버림.  뒤의 경우에서 패닉의 원인이 버그 있는
   lock_acquire() 구현이었다면 그냥 재귀(recurse)에 빠질
   가능성이 높음. */
static bool use_console_lock;

/* Pintos에 디버그 출력을 충분히 많이 추가하면, 한 스레드가
   console_lock을 재귀적으로 잡으려 하는 상황이 생길 수 있음.
   실제 예로, palloc_free()에 printf() 호출을 추가한 적이 있음.
   그 결과로 나온 실제 백트레이스(backtrace)는 다음과 같음.

   lock_console()
   vprintf()
   printf()             - palloc()이 락을 다시 잡으려 함
   palloc_free()        
   schedule_tail()      - 스레드를 전환하는 중에 다른 스레드가 죽음
   schedule()
   thread_yield()
   intr_handler()       - 타이머 인터럽트(timer interrupt)
   intr_set_level()
   serial_putc()
   putchar_have_lock()
   putbuf()
   sys_write()          - 프로세스 하나가 콘솔에 쓰는 중
   syscall_handler()
   intr_handler()

   이런 종류의 문제는 디버그하기 매우 어려우므로, 깊이 카운터
   (depth counter)로 재귀 락(recursive lock)을 흉내 내어 문제를
   피함. */
static int console_lock_depth;

/* 콘솔에 출력한 문자 수. */
static int64_t write_cnt;

/* 콘솔 락 사용을 켬. */
void
console_init (void) {
	lock_init (&console_lock);
	use_console_lock = true;
}

/* 커널 패닉(kernel panic)이 진행 중임을 콘솔에 알려,
   이제부터 콘솔 락을 잡으려 하지 않도록 경고함. */
void
console_panic (void) {
	use_console_lock = false;
}

/* 콘솔 통계(statistics)를 출력함. */
void
console_print_stats (void) {
	printf ("Console: %lld characters output\n", write_cnt);
}

/* 콘솔 락을 획득함. */
	static void
acquire_console (void) {
	if (!intr_context () && use_console_lock) {
		if (lock_held_by_current_thread (&console_lock)) 
			console_lock_depth++; 
		else
			lock_acquire (&console_lock); 
	}
}

/* 콘솔 락을 해제함. */
static void
release_console (void) {
	if (!intr_context () && use_console_lock) {
		if (console_lock_depth > 0)
			console_lock_depth--;
		else
			lock_release (&console_lock); 
	}
}

/* 현재 스레드가 콘솔 락을 가지고 있으면 true를,
   아니면 false를 반환함. */
static bool
console_locked_by_current_thread (void) {
	return (intr_context ()
			|| !use_console_lock
			|| lock_held_by_current_thread (&console_lock));
}

/* 표준 vprintf() 함수.
   printf()와 비슷하지만 va_list를 사용함.
   출력을 vga 디스플레이(display)와 시리얼 포트(serial port)
   양쪽에 씀. */
int
vprintf (const char *format, va_list args) {
	int char_cnt = 0;

	acquire_console ();
	__vprintf (format, args, vprintf_helper, &char_cnt);
	release_console ();

	return char_cnt;
}

/* 문자열 S를 콘솔에 쓰고 뒤에 개행 문자(new-line character)를
   붙임. */
int
puts (const char *s) {
	acquire_console ();
	while (*s != '\0')
		putchar_have_lock (*s++);
	putchar_have_lock ('\n');
	release_console ();

	return 0;
}

/* BUFFER의 N개 문자를 콘솔에 씀. */
void
putbuf (const char *buffer, size_t n) {
	acquire_console ();
	while (n-- > 0)
		putchar_have_lock (*buffer++);
	release_console ();
}

/* C를 vga 디스플레이(display)와 시리얼 포트(serial port)에 씀. */
int
putchar (int c) {
	acquire_console ();
	putchar_have_lock (c);
	release_console ();

	return c;
}

/* vprintf()용 도우미 함수(helper function). */
static void
vprintf_helper (char c, void *char_cnt_) {
	int *char_cnt = char_cnt_;
	(*char_cnt)++;
	putchar_have_lock (c);
}

/* C를 vga 디스플레이(display)와 시리얼 포트(serial port)에 씀.
   필요한 경우 호출자가 이미 콘솔 락을 획득한 상태임. */
static void
putchar_have_lock (uint8_t c) {
	ASSERT (console_locked_by_current_thread ());
	write_cnt++;
	serial_putc (c);
	vga_putc (c);
}
