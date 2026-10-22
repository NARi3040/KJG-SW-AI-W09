/* 이 파일은 Nachos 교육용 운영체제의 소스 코드에서 파생되었음.
   Nachos 저작권 고지는 아래에 전문 그대로 수록되어 있음. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/**
 * @brief 세마포어 SEMA를 VALUE로 초기화함.
 *
 * @details
 * 세마포어는 음이 아닌 정수와, 그 값이 양수가 되기를 기다리는 스레드들의
 * 리스트(waiters)로 구성됨. 이 함수는 value를 설정하고 waiters를 빈 리스트로 만듦.
 *
 * @param[out] sema 초기화할 세마포어
 * @param[in] value 세마포어의 초기값
 *
 * @see list_init()
 */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/**
 * @brief 세마포어에 대한 down 또는 "P" 연산
 *
 * @details
 * SEMA의 값이 양수가 될 때까지 기다린 뒤 atomic하게 1 감소시킴.
 * 값이 0이면 현재 스레드를 sema->waiters에 넣고 thread_block()으로 잠듦.
 * 깨어난 뒤에도 값을 다시 검사하므로(while), 다른 스레드가 먼저 값을 가져갔다면 다시 잠듦.
 *
 * @param[in,out] sema down할 세마포어
 *
 * @note 이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 됨
 * @note 인터럽트가 비활성화된 상태에서 호출할 수는 있지만, sleep하게 되면
 *       다음에 스케줄되는 스레드가 아마 인터럽트를 다시 켤 것임
 *
 * @see sema_up()
 * @see thread_block()
 */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_push_back(&sema->waiters, &thread_current()->elem);
		
		thread_block();
	}
	sema->value--;
	intr_set_level(old_level);
}

/**
 * @brief 세마포어에 대한 down 또는 "P" 연산 (non-blocking 버전)
 *
 * @details
 * 세마포어가 아직 0이 아닐 때만 값을 1 감소시킴. 값이 0이면 기다리지 않고 바로 반환함.
 *
 * @param[in,out] sema down을 시도할 세마포어
 * @return 세마포어가 감소되었으면 true, 아니면 false
 *
 * @note 이 함수는 sleep하지 않으므로 인터럽트 핸들러에서 호출해도 됨
 *
 * @see sema_down()
 */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/**
 * @brief 세마포어에 대한 up 또는 "V" 연산
 *
 * @details
 * SEMA의 값을 증가시키고, SEMA를 기다리는 스레드가 있다면 그중 priority가 가장 높은
 * 스레드를 list_max()(thread_priority_lower 기준)으로 찾아 waiters에서 제거한 뒤
 * thread_unblock()으로 깨움. 마지막에 thread_test_preemption()으로, 깨운 스레드의
 * priority가 더 높으면 즉시 CPU를 양보함.
 *
 * @param[in,out] sema up할 세마포어
 *
 * @note 이 함수는 인터럽트 핸들러에서 호출해도 됨 (핸들러 안에서는 intr_yield_on_return()으로 양보)
 * @note waiters는 정렬 상태로 유지되지 않으므로 매번 list_max()로 탐색함
 *
 * @see sema_down()
 * @see thread_unblock()
 * @see thread_test_preemption()
 */
void
sema_up (struct semaphore *sema) {
    enum intr_level old_level;

    ASSERT (sema != NULL);

    old_level = intr_disable ();
    if (!list_empty (&sema->waiters)) {
        /* list_max로 가장 높은 우선순위(lower 기준 최대값) 원소 검색 */
        struct list_elem *e = list_max(&sema->waiters, thread_priority_lower, NULL);
        
        /* 리스트에서 해당 노드 제거 */
        list_remove(e);
        
        /* 해당 스레드 unblock */
        thread_unblock(list_entry(e, struct thread, elem));
    }
    sema->value++;

    intr_set_level(old_level);

	thread_test_preemption();
}

static void sema_test_helper (void *sema_);

/**
 * @brief 세마포어 self-test
 *
 * @details
 * 한 쌍의 스레드 사이에서 제어가 "ping-pong"하도록 만들어 sema_down()/sema_up()이
 * 제대로 동작하는지 확인함.
 *
 * @note 무슨 일이 일어나는지 보려면 printf() 호출을 넣을 것
 *
 * @see sema_test_helper()
 */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/**
 * @brief sema_self_test()가 사용하는 스레드 함수
 *
 * @param[in,out] sema_ 세마포어 2개짜리 배열을 가리키는 포인터
 *
 * @see sema_self_test()
 */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/**
 * @brief LOCK을 초기화함
 *
 * @details
 * lock은 한 시점에 최대 한 스레드만 보유할 수 있음. 이 lock은 "recursive"하지 않음.
 * 즉, lock을 현재 보유한 스레드가 같은 lock을 다시 acquire하려 하면 오류임.
 *
 * lock은 초기값이 1인 세마포어의 특수화(specialization)임.
 * lock과 그런 세마포어의 차이는 두 가지임.
 * 첫째, 세마포어는 1보다 큰 값을 가질 수 있지만 lock은 한 시점에 한 스레드만 소유할 수 있음.
 * 둘째, 세마포어에는 owner가 없어서 한 스레드가 "down"하고 다른 스레드가 "up"할 수 있지만,
 * lock은 같은 스레드가 acquire와 release를 모두 해야 함.
 *
 * @param[out] lock 초기화할 lock
 *
 * @note 이런 제약이 부담스럽다면, lock 대신 세마포어를 써야 한다는 좋은 신호임
 *
 * @see sema_init()
 */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/**
 * @brief 현재 스레드의 priority를 lock holder 체인을 따라 전파함
 *
 * @details
 * 현재 스레드가 기다리는 lock의 holder가 더 낮은 priority면 올려줌.
 * holder가 다른 lock을 기다리고 있으면 그 holder로 계속 전파함 (nested donation).
 * holder의 priority를 올릴 때마다 thread_requeue()로 holder가 READY일 때 ready_list 정렬을 복구함.
 *
 * @see thread_requeue()
 *
 * @note 인터럽트가 꺼진 상태에서 호출해야 함
 * @note 체인 깊이는 8로 제한함
 */
static void
donate_priority (void) {
	struct thread *cur = thread_current();

	/* 기다리는 lock의 holder를 따라 체인을 올라가며 priority를 전파함. 연결 리스트 재귀 탐색 생각하면 편함 */
	for (int depth = 0; depth < 8 && cur->wait_on_lock != NULL; depth++) {
		struct thread *holder = cur->wait_on_lock->holder;

		/* lock을 지금 쥐고 있는 스레드(holder가 이미 나(cur)만큼 높거나 더 높은 경우 */
		if (holder == NULL || holder->priority >= cur->priority)
			break;
		holder->priority = cur->priority;
		thread_requeue (holder); /* holder가 READY면 ready_list 정렬 위치를 다시 잡음 */
		cur = holder;
	}
}

/**
 * @brief 현재 스레드의 donations에서 LOCK을 기다리던 스레드를 모두 제거함
 *
 * @param[in] lock release하는 lock
 */
static void
remove_donations_for (struct lock *lock) {
	struct thread *curr = thread_current();

	/*  donations 리스트 하나를 처음부터 끝까지 훑음 */
	for (struct list_elem *e = list_begin(&curr->donations); e != list_end(&curr->donations);) {
		struct thread *t = list_entry(e, struct thread, donation_elem);

		/* 스레드(t)가 지금 release하는 이 lock 때문에 기다리고 있었나? */
		if (t->wait_on_lock == lock)
			e = list_remove(e);
		else
			e = list_next(e);
	}
}

/**
 * @brief LOCK을 acquire함
 *
 * @details
 * 필요하면 lock이 사용 가능해질 때까지 sleep함. 획득에 성공하면 holder를 현재 스레드로 설정함.
 * holder가 있으면(MLFQS 제외) 기다리기 전에 wait_on_lock을 설정하고 holder의 donations에
 * 자신을 등록한 뒤 donate_priority()로 priority를 전파함.
 * 획득한 뒤에는 이 lock을 계속 기다리는 스레드들을 새 holder(현재 스레드)의 donations에
 * 다시 등록하고 refresh_priority()로 priority를 갱신함.
 * 이전 holder의 lock_release()가 이 lock에 대한 donation을 모두 지웠기 때문임.
 *
 * @param[in,out] lock acquire할 lock
 *
 * @note 현재 스레드가 이미 이 lock을 보유하고 있으면 안 됨
 * @note 이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 됨
 * @note 인터럽트가 비활성화된 상태에서 호출할 수 있지만, sleep해야 하면 인터럽트가 다시 켜짐
 *
 * @see lock_try_acquire()
 * @see lock_release()
 */
void
lock_acquire (struct lock *lock) {
	struct thread *curr = thread_current ();

	ASSERT (lock != NULL);
	ASSERT (!intr_context());
	ASSERT (!lock_held_by_current_thread(lock));

	/* holder 확인과 donations 등록 사이에 holder가 release하지 못하도록 인터럽트를 끔. */
	enum intr_level old_level = intr_disable();
	if (!thread_mlfqs && lock->holder != NULL) {
		curr->wait_on_lock = lock;
		/* lock holder의 donations 리스트 끝에 현재 스레드(curr)를 등록 */
		list_push_back(&lock->holder->donations, &curr->donation_elem);
		donate_priority();
	}
	intr_set_level (old_level);

	sema_down(&lock->semaphore);

	/* 이전 holder의 release가 남은 waiter들의 donation을 지웠으므로, 새 holder(curr)에게 다시 등록함 */
	old_level = intr_disable();
	curr->wait_on_lock = NULL;
	lock->holder = curr;
	if (!thread_mlfqs) {
		for (struct list_elem *e = list_begin(&lock->semaphore.waiters);
				e != list_end(&lock->semaphore.waiters); e = list_next(e)) {
			struct thread *t = list_entry(e, struct thread, elem);
			t->wait_on_lock = lock;
			list_push_back(&curr->donations, &t->donation_elem);
		}
		refresh_priority();
	}
	intr_set_level(old_level);
}

/**
 * @brief LOCK을 acquire하려 시도함 (non-blocking 버전)
 *
 * @param[in,out] lock acquire를 시도할 lock
 * @return 성공하면 true, 실패하면 false
 *
 * @note 현재 스레드가 이미 이 lock을 보유하고 있으면 안 됨
 * @note 이 함수는 sleep하지 않으므로 인터럽트 핸들러 안에서 호출해도 됨
 *
 * @see lock_acquire()
 * @see sema_try_down()
 */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down(&lock->semaphore);
	if (success)
		lock->holder = thread_current();
	return success;
}

/**
 * @brief 현재 스레드가 소유한 LOCK을 release함
 *
 * @details
 * (MLFQS 제외) 이 lock 때문에 받은 donation을 remove_donations_for()로 지우고
 * refresh_priority()로 priority를 다시 계산함. 이후 holder를 비우고 sema_up()으로
 * 가장 priority가 높은 waiter를 깨움.
 *
 * @param[in,out] lock release할 lock
 *
 * @note 호출 스레드가 이 lock의 holder여야 함
 * @note 인터럽트 핸들러는 lock을 acquire할 수 없으므로, 인터럽트 핸들러 안에서
 *       lock을 release하려는 것은 의미가 없음
 *
 * @see lock_acquire()
 */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread(lock));

	enum intr_level old_level = intr_disable ();
	if (!thread_mlfqs) {
		remove_donations_for(lock);
		refresh_priority();
	}
	lock->holder = NULL;
	sema_up(&lock->semaphore);
	intr_set_level(old_level);
}

/**
 * @brief 현재 스레드가 LOCK을 보유하고 있는지 검사함
 *
 * @param[in] lock 검사할 lock
 * @return 현재 스레드가 보유하고 있으면 true, 아니면 false
 *
 * @note 다른 스레드가 lock을 보유하고 있는지 검사하는 것은 race가 생길 수 있으니 주의
 */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/**
 * @brief 조건 변수의 waiters 리스트에 들어가는 세마포어 하나
 *
 * @details cond_wait()를 호출한 스레드마다 하나씩 스택에 만들어져 cond->waiters에 연결됨.
 */
struct semaphore_elem {
	struct list_elem elem;              /* 리스트 요소. */
	struct semaphore semaphore;         /* 이 세마포어. */
};

/**
 * @brief 두 semaphore_elem을 기다리는 스레드의 priority를 비교하는 list_less_func
 *
 * @details
 * 각 semaphore_elem의 세마포어에서 기다리는 스레드(waiters 맨 앞)의 priority를
 * thread_priority_lower()로 비교함. 아직 sema_down()으로 블록되지 않아
 * waiters가 비어 있는 원소는 가장 낮은 priority로 취급함.
 *
 * @param[in] a 비교할 첫 번째 semaphore_elem의 list_elem
 * @param[in] b 비교할 두 번째 semaphore_elem의 list_elem
 * @param[in] aux 사용하지 않음
 *
 * @return a를 기다리는 스레드의 priority가 b보다 낮으면 true, 아니면 false
 *
 * @see cond_signal()
 */
static bool
sema_priority_lower (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
	struct semaphore *sa = &list_entry (a, struct semaphore_elem, elem)->semaphore;
	struct semaphore *sb = &list_entry (b, struct semaphore_elem, elem)->semaphore;

	if (list_empty (&sa->waiters))
		return !list_empty (&sb->waiters);
	if (list_empty (&sb->waiters))
		return false;
	return thread_priority_lower (list_front (&sa->waiters), list_front (&sb->waiters), NULL);
}

/**
 * @brief 조건 변수 COND를 초기화함
 *
 * @details
 * 조건 변수는 한 코드 조각이 어떤 조건을 signal하고, 협력하는 코드가 그 signal을
 * 받아 대응할 수 있게 해 줌.
 *
 * @param[out] cond 초기화할 조건 변수
 */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/**
 * @brief LOCK을 atomic하게 release하고, 다른 코드가 COND를 signal할 때까지 기다림
 *
 * @details
 * COND가 signal된 뒤 반환하기 전에 LOCK을 다시 acquire함.
 *
 * 이 함수가 구현하는 monitor는 "Hoare" 방식이 아니라 "Mesa" 방식임.
 * 즉, signal을 보내고 받는 것이 atomic한 연산이 아님.
 * 따라서 보통 호출자는 wait가 끝난 뒤 조건을 다시 확인해야 하며, 필요하면 다시 기다려야 함.
 *
 * 하나의 조건 변수는 단 하나의 lock과만 연결되지만, 하나의 lock은 임의 개수의
 * 조건 변수와 연결될 수 있음. 즉, lock에서 조건 변수로의 일대다(one-to-many) 매핑이 존재함.
 *
 * @param[in,out] cond 기다릴 조건 변수
 * @param[in,out] lock cond와 연결된 lock
 *
 * @note 이 함수를 호출하기 전에 LOCK을 보유하고 있어야 함
 * @note 이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 됨
 * @note 인터럽트가 비활성화된 상태에서 호출할 수 있지만, sleep해야 하면 인터럽트가 다시 켜짐
 *
 * @see cond_signal()
 */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}


/**
 * @brief COND를 기다리는 스레드 하나를 깨움
 *
 * @details
 * COND를 기다리는 스레드가 있다면(LOCK으로 보호됨), 그중 priority가 가장 높은
 * 스레드를 list_max()(sema_priority_lower 기준)으로 찾아 signal해서
 * wait에서 깨어나게 함.
 *
 * @param[in,out] cond signal할 조건 변수
 * @param[in] lock cond와 연결된 lock
 *
 * @note 이 함수를 호출하기 전에 LOCK을 보유하고 있어야 함
 * @note 인터럽트 핸들러는 lock을 acquire할 수 없으므로, 인터럽트 핸들러 안에서
 *       조건 변수를 signal하려는 것은 의미가 없음
 *
 * @see cond_wait()
 * @see cond_broadcast()
 */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)) {
		struct list_elem *e = list_max (&cond->waiters, sema_priority_lower, NULL);
		list_remove (e);
		sema_up (&list_entry (e, struct semaphore_elem, elem)->semaphore);
	}
}

/**
 * @brief COND를 기다리는 모든 스레드를 깨움
 *
 * @details COND를 기다리는 스레드가 있다면(LOCK으로 보호됨), 모두 깨움.
 *
 * @param[in,out] cond signal할 조건 변수
 * @param[in] lock cond와 연결된 lock
 *
 * @note 이 함수를 호출하기 전에 LOCK을 보유하고 있어야 함
 * @note 인터럽트 핸들러는 lock을 acquire할 수 없으므로, 인터럽트 핸들러 안에서
 *       조건 변수를 signal하려는 것은 의미가 없음
 *
 * @see cond_signal()
 */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
