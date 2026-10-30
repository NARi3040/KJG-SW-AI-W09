/* uninit.c: 초기화되지 않은(uninitialized) 페이지의 구현.
 *
 * 모든 페이지는 uninit 페이지로 생성됨. 첫 page fault가 발생하면
 * 핸들러 체인이 uninit_initialize (page->operations.swap_in)를 호출함.
 * uninit_initialize 함수는 페이지 객체를 초기화하여 해당 페이지를 특정
 * 페이지 객체(anon, file, page_cache)로 변환하고, vm_alloc_page_with_initializer
 * 함수에서 전달받은 초기화 콜백을 호출함.
 * */

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

/* 첫 fault 시 페이지를 초기화함. */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* 먼저 가져올 것. page_initialize가 값을 덮어쓸 수 있음. */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: 이 함수를 수정해야 할 수도 있음. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* uninit_page가 보유한 자원을 해제함. 대부분의 페이지는 다른 페이지 객체로
 * 변환되지만, 실행 중 한 번도 참조되지 않은 uninit 페이지가 프로세스
 * 종료 시점에 남아 있을 수 있음.
 * PAGE는 호출자가 해제함. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit = &page->uninit;
	free (uninit->aux);
	uninit->aux = NULL;
}
