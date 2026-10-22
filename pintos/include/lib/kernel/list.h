#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

/* 이중 연결 리스트(Doubly linked list).
 *
 * 이 이중 연결 리스트 구현은 동적 할당 메모리를 사용할 필요가
 * 없음.  대신 리스트 원소가 될 수 있는 각 구조체는 struct list_elem
 * 멤버를 내장해야 함.  모든 리스트 함수는 이 `struct
 * list_elem`들에 대해 동작함.  list_entry 매크로를 쓰면 struct
 * list_elem 에서 그것을 포함하는 구조체 객체로 되돌리는 변환을
 * 할 수 있음.

 * 예를 들어 `struct foo'의 리스트가 필요하다고 하자.
 * `struct foo'는 다음과 같이 `struct list_elem' 멤버를
 * 포함해야 함.

 * struct foo {
 *   struct list_elem elem;
 *   int bar;
 *   ...다른 멤버들...
 * };

 * 그러면 `struct foo'의 리스트를 다음과 같이 선언하고
 * 초기화할 수 있음.

 * struct list foo_list;

 * list_init (&foo_list);

 * 순회(iteration)는 struct list_elem 에서 그것을 감싸는 구조체로
 * 되돌려야 하는 전형적인 상황임.  foo_list 를 사용한 예시는
 * 다음과 같음.

 * struct list_elem *e;

 * for (e = list_begin (&foo_list); e != list_end (&foo_list);
 * e = list_next (e)) {
 *   struct foo *f = list_entry (e, struct foo, elem);
 *   ...f 로 무언가를 함...
 * }

 * 리스트 사용의 실제 예시는 소스 곳곳에서 찾을 수 있음.
 * 예를 들어 threads 디렉터리의 malloc.c, palloc.c, thread.c 는
 * 모두 리스트를 사용함.

 * 이 리스트의 인터페이스는 C++ STL 의 list<> 템플릿에서
 * 착안했음.  list<> 에 익숙하다면 쉽게 쓸 수 있을 것임.
 * 다만 이 리스트는 타입 검사를 *전혀* 하지 않으며 그 밖의
 * 정합성 검사도 거의 할 수 없다는 점을 강조해야 함.
 * 실수하면 그 대가를 치르게 됨.

 * 리스트 용어 정리(Glossary).

 * - "front": 리스트의 첫 번째 원소.  빈 리스트에서는 정의되지
 * 않음.  list_front() 가 반환함.

 * - "back": 리스트의 마지막 원소.  빈 리스트에서는 정의되지
 * 않음.  list_back() 이 반환함.

 * - "tail": 리스트의 마지막 원소 바로 뒤에 있다고 가정한
 * 원소.  빈 리스트에서도 잘 정의됨.  list_end() 가 반환함.
 * 앞에서 뒤로 순회할 때 끝을 나타내는 센티널(sentinel)로
 * 쓰임.

 * - "beginning": 비어 있지 않은 리스트에서는 front 임.  빈
 * 리스트에서는 tail 임.  list_begin() 이 반환함.  앞에서 뒤로
 * 순회할 때 시작점으로 쓰임.

 * - "head": 리스트의 첫 번째 원소 바로 앞에 있다고 가정한
 * 원소.  빈 리스트에서도 잘 정의됨.  list_rend() 가 반환함.
 * 뒤에서 앞으로 순회할 때 끝을 나타내는 센티널로
 * 쓰임.

 * - "reverse beginning": 비어 있지 않은 리스트에서는 back 임.
 * 빈 리스트에서는 head 임.  list_rbegin() 이 반환함.  뒤에서
 * 앞으로 순회할 때 시작점으로 쓰임.
 *
 * - "interior element": head 도 tail 도 아닌 원소, 즉 실제
 * 리스트 원소.  빈 리스트에는 interior element 가 하나도
 * 없음.*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 이중 연결 리스트의 노드임.
 *
 * 데이터는 없고 prev/next 포인터만 가짐. 리스트에 넣을 구조체 안에
 * 멤버로 심어서 쓰는 intrusive 방식임. (예: struct thread의 elem)
 * 노드 포인터에서 원래 구조체를 복원하려면 list_entry()를 사용함.
 *
 * @see list_entry
 */
struct list_elem {
	struct list_elem *prev;     /**< 이전 노드. */
	struct list_elem *next;     /**< 다음 노드. */
};

/* 리스트(List). */
struct list {
	struct list_elem head;      /* 리스트 head. */
	struct list_elem tail;      /* 리스트 tail. */
};

/**
 * @brief list_elem 포인터로부터 그 노드를 멤버로 가진 바깥 구조체의 포인터를 복원함.
 *
 * @details
 * &LIST_ELEM->next(노드 시작 주소 + 8바이트)에서 offsetof(STRUCT, MEMBER.next)를
 * 빼서 구조체의 시작 주소를 구함. MEMBER.next는 구조체 안에서 elem 멤버의
 * next 필드까지의 오프셋임. 둘 다 next 기준이라 elem 기준으로 계산한 것과 결과가 같음.
 *
 * @verbatim
 * struct thread (메모리)
 * +------------------+ <- t        (우리가 원하는 주소)
 * | tid, name, ...   |
 * | ...              |
 * +------------------+ <- &t->elem (= e, 리스트가 가진 주소)
 * | elem (prev,next) |
 * +------------------+
 * | wakeup_tick ...  |
 * +------------------+
 * @endverbatim
 *
 * @param LIST_ELEM 구조체에 심어 둔 struct list_elem을 가리키는 포인터.
 * @param STRUCT    바깥 구조체의 타입 이름. (예: struct thread)
 * @param MEMBER    STRUCT 안에서 list_elem 멤버의 이름. (예: elem)
 *
 * @return STRUCT * 타입의 포인터.
 *
 * @note LIST_ELEM이 실제로 STRUCT의 MEMBER 멤버를 가리킬 때만 유효함.
 *       다른 곳을 가리키면 잘못된 주소가 나오고 검사도 없음.
 * @note 사용 예: list_entry (e, struct thread, elem)
 * @note 이딴게 왜 있지
 *
 * @see struct list_elem
 */
#define list_entry(LIST_ELEM, STRUCT, MEMBER)           \
	((STRUCT *) ((uint8_t *) &(LIST_ELEM)->next     \
		- offsetof (STRUCT, MEMBER.next)))

void list_init (struct list *);

/* 리스트 순회(List traversal). */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* 리스트 삽입(List insertion). */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before, struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* 리스트 제거(List removal). */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* 리스트 원소(List elements). */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* 리스트 속성(List properties). */
size_t list_size (struct list *);
bool list_empty (struct list *); // ?

/* 기타(Miscellaneous). */
void list_reverse (struct list *);

/* 두 리스트 원소 A와 B의 값을 보조 데이터 AUX를 사용해 비교함.
   A가 B보다 작으면 true를, A가 B보다 크거나 같으면
   false를 반환함. */
typedef bool list_less_func (const struct list_elem *a,
                             const struct list_elem *b,
                             void *aux);

/* 정렬된 원소를 가진 리스트에 대한 연산(Operations on lists with ordered elements). */
void list_sort (struct list *,
                list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *,
                          list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates,
                  list_less_func *, void *aux);

/* 최댓값과 최솟값(Max and min). */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

#endif /* lib/kernel/list.h */
