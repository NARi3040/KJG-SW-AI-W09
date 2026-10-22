#include "devices/input.h"
#include <debug.h>
#include "devices/intq.h"
#include "devices/serial.h"

/* 키보드(keyboard)와 시리얼 포트(serial port)에서 들어온 키를 저장함. */
static struct intq buffer;

/* 입력 버퍼(input buffer)를 초기화함. */
void
input_init (void) {
	intq_init (&buffer);
}

/* 입력 버퍼에 키를 추가함.
   인터럽트(interrupt)가 꺼져 있어야 하고 버퍼가 가득 차 있으면 안 됨. */
void
input_putc (uint8_t key) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (!intq_full (&buffer));

	intq_putc (&buffer, key);
	serial_notify ();
}

/* 입력 버퍼에서 키를 하나 꺼냄.
   버퍼가 비어 있으면 키가 눌릴 때까지 기다림. */
uint8_t
input_getc (void) {
	enum intr_level old_level;
	uint8_t key;

	old_level = intr_disable ();
	key = intq_getc (&buffer);
	serial_notify ();
	intr_set_level (old_level);

	return key;
}

/* 입력 버퍼가 가득 찼으면 true를,
   아니면 false를 반환함.
   인터럽트가 꺼져 있어야 함. */
bool
input_full (void) {
	ASSERT (intr_get_level () == INTR_OFF);
	return intq_full (&buffer);
}
