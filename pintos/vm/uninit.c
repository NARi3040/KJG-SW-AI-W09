/* uninit.c: 초기화되지 않은(uninitialized) 페이지의 구현.
 *
 * 모든 페이지는 uninit 페이지로 생성됨. 첫 page fault가 발생하면
 * 핸들러 체인이 uninit_initialize (page->operations.swap_in)를 호출함.
 * uninit_initialize 함수는 페이지 객체를 초기화하여 해당 페이지를 특정
 * 페이지 객체(anon, file, page_cache)로 변환하고, vm_alloc_page_with_initializer
 * 함수에서 전달받은 초기화 콜백을 호출함.
 * */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct (이 구조체는 수정하지 말 것) */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function (이 함수는 수정하지 말 것) */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* 아직 frame 없음 */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* 타입 변신. 실패해서 아직 uninit이면 aux를 비워 uninit_destroy가 같은 aux를 또 해제하지 않게 함. */
static bool
uninit_transform (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;
	bool succ = uninit->page_initializer (page, uninit->type, kva);
	if (!succ && VM_TYPE (page->operations->type) == VM_UNINIT)
		uninit->aux = NULL;
	return succ;
}

/* 첫 fault 시 페이지를 초기화함. */
static bool
uninit_initialize (struct page *page, void *kva) {
	vm_initializer *init = page->uninit.init; /* 변신이 union을 덮어쓰기 전에 꺼냄 */
	void *aux = page->uninit.aux;

	bool succ = uninit_transform (page, kva) && (init == NULL || init (page, aux));
	free (aux); /* 첫 접근 경로에서 aux를 해제하는 유일한 위치 */
	return succ;
}

/* uninit_page가 보유한 자원을 해제함. 대부분의 페이지는 다른 페이지 객체로
 * 변환되지만, 실행 중 한 번도 참조되지 않은 uninit 페이지가 프로세스
 * 종료 시점에 남아 있을 수 있음.
 * PAGE는 호출자가 해제함. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: 이 함수를 채울 것.
	 * TODO: 할 일이 없다면 그냥 return할 것. */
}
