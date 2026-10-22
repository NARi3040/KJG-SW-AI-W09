# 구현 방식

[← README로 돌아가기](../README.md)

수정한 파일은 `pintos/threads/thread.c`, `pintos/threads/synch.c`, `pintos/include/threads/thread.h`, `pintos/devices/timer.c`입니다.

## 1. Alarm Clock (`devices/timer.c`)

- busy waiting을 하지 않고, `timer_sleep()`이 깨울 시각을 절대 tick으로 `wakeup_tick`에 저장한 뒤 BLOCKED 상태로 잠듭니다.
- 잠든 스레드는 `sleep_list`에 **`wakeup_tick` 오름차순**으로 삽입합니다(`wakeup_less()`).
- `timer_interrupt()`가 `wake_sleepers()`를 호출해 리스트 앞에서부터 깰 시각이 된 스레드를 `thread_unblock()`합니다. 정렬되어 있으므로 아직 깰 시각이 아닌 스레드를 만나면 순회를 멈춥니다.
- 깨운 뒤 `thread_test_preemption()`으로 선점 여부를 확인합니다.

## 2. Priority Scheduling (`threads/thread.c`)

핵심 설계는 **`ready_list`를 항상 priority 내림차순으로 유지**하는 것입니다.

- 삽입 지점은 `thread_unblock()`, `thread_yield()`, `thread_requeue()` 세 곳이고 모두 `list_insert_ordered()`와 `thread_priority_higher()`를 씁니다.
- 같은 priority 안에서는 먼저 들어온 스레드가 앞에 남습니다(FIFO, round robin).
- 정렬이 보장되므로 `next_thread_to_run()`은 `list_pop_front()`, `thread_test_preemption()`은 `list_front()`만 보면 됩니다. 이전에는 꺼낼 때마다 `list_max()`로 전체를 훑었습니다.
- `thread_test_preemption()`은 `ready_list` 맨 앞 스레드의 priority가 현재 스레드보다 높으면 양보합니다. 인터럽트 컨텍스트에서는 `thread_yield()`를 직접 호출할 수 없으므로 `intr_yield_on_return()`으로 예약합니다.
- 호출 지점은 `thread_create()`, `thread_set_priority()`, `sema_up()`, `wake_sleepers()`입니다.

**불변식(꼭 지켜 주세요)**: `ready_list` 안 스레드의 priority를 바꾸는 코드는 반드시 바꾼 직후 `thread_requeue()`를 호출해야 합니다. 현재 그런 경로는 `donate_priority()` 하나뿐입니다. 다른 곳에서 READY 스레드의 priority를 바꾸면 정렬이 깨져 `list_front()` 가정이 틀어집니다.

### 왜 이렇게 했는지 (벤치 결과)

| | 삽입 | 꺼내기 | donation 시 별도 처리 | 변경 범위 |
| --- | --- | --- | --- | --- |
| 이전 | O(n) 정렬 삽입 | O(n) `list_max` | 필요 없음 (`list_max`가 보정) | — |
| V1 | O(1) | O(n) `list_max` | 필요 없음 | 두 줄 |
| V2 (현재) | O(n) 정렬 삽입 | O(1) | `thread_requeue` 필요 | 함수 1개 + 수정 4곳 |

이전 방식은 정렬 삽입의 비용을 치르면서도 꺼낼 때 다시 전체를 훑어서 효과가 없었습니다. 64개 스레드 벤치에서 yield 한 번당 비교가 약 128회였습니다. 정렬 상태를 보장하는 대신 donation으로 priority가 바뀔 때 `thread_requeue()`로 위치를 다시 잡는 방식을 택했습니다.

## 3. Priority Donation (`threads/synch.c`, `thread.c`)

`struct thread`에 다음 필드를 추가했습니다(`include/threads/thread.h`).

| 필드 | 의미 |
| --- | --- |
| `init_priority` | donation을 받기 전 원래 priority. `thread_set_priority()`가 바꾸는 값 |
| `priority` | donation이 반영된 유효 priority. 스케줄러가 쓰는 값 |
| `wait_on_lock` | 지금 획득을 기다리는 lock |
| `donations` | 나에게 priority를 기부한 스레드 리스트 |
| `donation_elem` | 상대방 `donations`에 들어갈 때 쓰는 리스트 요소 |

priority 하나만 덮어쓰면 기부가 끝났을 때 원래 값을 복원할 수 없고, 여러 lock에서 동시에 기부받을 때 lock 하나를 풀어도 나머지 기부는 유지해야 하는 문제가 있어서 위와 같이 분리했습니다.

동작 순서는 다음과 같습니다.

1. `lock_acquire()`: holder가 있으면(MLFQS 제외) `wait_on_lock`을 설정하고 holder의 `donations`에 자신을 등록한 뒤 `donate_priority()`를 호출합니다.
2. `donate_priority()`: `wait_on_lock`의 holder를 따라 올라가며 priority를 전파합니다(nested donation, 깊이 8 제한). holder의 priority를 올릴 때마다 `thread_requeue()`를 호출합니다.
3. `lock_release()`: 이 lock 때문에 받은 기부만 `remove_donations_for()`로 제거하고, `refresh_priority()`로 `init_priority`와 남은 기부자 중 최댓값을 다시 계산합니다. 그다음 `sema_up()`으로 priority가 가장 높은 waiter를 깨웁니다.
4. `lock_acquire()`로 돌아와 획득에 성공하면, 이 lock을 계속 기다리는 스레드들을 새 holder의 `donations`에 **다시 등록**하고 `refresh_priority()`를 호출합니다. 이전 holder의 release가 그 기부를 모두 지웠기 때문입니다. 이 단계가 없으면 priority inversion이 생길 수 있습니다.

- `sema->waiters`와 `cond->waiters`는 정렬하지 않고 꺼낼 때 `list_max()`로 찾습니다. donation으로 waiter의 priority가 바뀌어도 안전합니다.
- `thread_set_priority()`는 `init_priority`만 바꾸고 `refresh_priority()`를 호출한 뒤 `thread_test_preemption()`을 부릅니다.
- `refresh_priority()`는 현재 실행 중인 스레드만 갱신하므로 `ready_list` 재정렬이 필요 없습니다.

### 인터럽트 처리 규칙

함수 끝에서 인터럽트를 무조건 켜지 말고 **호출 전 상태로 복원**해야 합니다.

```c
enum intr_level old_level = intr_disable ();
/* ... */
intr_set_level (old_level);
```

`lock_release()`는 인터럽트를 끈 채로 `sema_up()`을 호출합니다. 안쪽 함수가 인터럽트를 무조건 켜면 바깥 함수의 보호 구간이 끝나기 전에 타이머 인터럽트와 선점이 끼어들 수 있습니다.

## 알려진 한계

- `lock_acquire()`의 재등록 수정은 기존 테스트가 깨지지 않는 것만 확인했고, 그 시나리오(남은 waiter가 나중에 priority를 받는 경우)를 직접 재현하는 테스트는 없습니다.
- `lock_release()`가 `holder = NULL`로 만든 뒤 깨어난 스레드가 holder를 세팅하기 전까지의 짧은 구간에서는 donation 체인이 `holder == NULL`에서 끊깁니다.
