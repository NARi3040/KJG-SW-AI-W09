# 스레드 사용 가이드

[← README로 돌아가기](../README.md)

팀마다 스레드 구현 방식이 다르므로, 이 저장소의 스레드가 어떻게 동작하고 어떻게 쓰면 되는지만 정리합니다.

## 동작 방식

- **스케줄러**: priority 기반 선점형입니다. priority가 같으면 FIFO(round robin)이고, time slice는 4 tick(`TIME_SLICE`)입니다.
- **priority 범위**: `PRI_MIN`(0) ~ `PRI_MAX`(63), 기본값 `PRI_DEFAULT`(31)입니다. 숫자가 클수록 먼저 실행됩니다.
- **`ready_list`**: 항상 priority 내림차순입니다. 맨 앞이 다음에 실행될 스레드입니다.
- **선점**: 더 높은 priority의 스레드가 READY가 되면 현재 스레드가 바로 양보합니다. `thread_create()`, `thread_set_priority()`, `sema_up()`, `lock_release()`(내부의 `sema_up()`), 타이머 깨우기에서 확인합니다.
- **상태 전환**: 스레드는 `thread_create()`로 READY가 되고, 스스로 `thread_block()`으로 BLOCKED가 되며, 다른 쪽에서 `thread_unblock()`으로 READY로 되돌립니다. `thread_yield()`는 READY로 돌아가며 양보합니다.
- **잠들기**: `timer_sleep()`은 busy waiting 없이 BLOCKED로 잠들고, 타이머 인터럽트가 시각이 되면 깨웁니다.

## 쓰는 방법

| 하고 싶은 일 | 쓰는 함수 |
| --- | --- |
| 스레드 생성 | `thread_create (name, priority, function, aux)` |
| 현재 스레드 가져오기 | `thread_current ()` |
| 양보 | `thread_yield ()` |
| 종료 | `thread_exit ()` (userprog에서는 안에서 `process_exit()`가 호출됨) |
| priority 읽기/설정 | `thread_get_priority ()` (donation 반영값), `thread_set_priority (n)` (원래 값만 바꿈) |
| 지연 | `timer_sleep (ticks)` |
| 상호 배제 | `lock_acquire ()`, `lock_release ()` (priority donation 적용) |
| 신호/카운팅 | `sema_down ()`, `sema_up ()` |
| 조건 대기 | `cond_wait ()`, `cond_signal ()`, `cond_broadcast ()` (모니터 lock과 함께) |

- **donation은 lock에만 적용됩니다.** 세마포어로는 priority가 전달되지 않으므로, 자원 보호에는 lock을 쓰세요.
- `sema_up()`과 `cond_signal()`은 기다리는 스레드 중 priority가 가장 높은 것을 깨웁니다.
- 새 스레드는 `thread_create()`가 반환되기 전에 먼저 실행될 수 있습니다. 새 스레드가 쓸 값은 `aux`로 넘기거나, 생성 전에 준비해 두세요.

## 주의할 점

- **`ready_list` 안 스레드의 priority를 바꾸면 반드시 `thread_requeue(t)`를 호출해야 합니다.** 지금은 `donate_priority()`만 그렇게 하고 있고, 정렬이 깨지면 `list_front()` 가정이 틀어집니다.
- **`elem`은 `ready_list`, `sleep_list`, 세마포어 `waiters`가 공유합니다.** READY, BLOCKED 상태의 스레드는 이 중 한 리스트에만 있습니다. 부모-자식 목록처럼 새 리스트에 스레드를 넣을 때는 `elem`이나 `donation_elem`을 재사용하지 말고 새 `list_elem`을 `struct thread`에 추가하세요.
- **`struct thread`는 4KB 페이지 하나를 커널 스택과 나눠 씁니다.** 필드를 많이 늘리거나 큰 배열을 직접 넣으면 스택이 줄어 오버플로가 납니다.
- **인터럽트를 껐다 켤 때는 이전 상태로 복원하세요.** `old_level = intr_disable ()` 후 `intr_set_level (old_level)`을 씁니다. 무조건 켜면 바깥 함수의 보호 구간이 깨집니다.
- **인터럽트 핸들러 안에서는** `lock_acquire()`, `sema_down()`, `thread_yield()`를 쓸 수 없습니다. 양보가 필요하면 `intr_yield_on_return()`을 씁니다.
- **MLFQS는 미구현입니다.** `thread_set_nice()` 등은 스텁이고, `-mlfqs` 옵션을 주면 donation과 `thread_set_priority()`가 꺼집니다.
