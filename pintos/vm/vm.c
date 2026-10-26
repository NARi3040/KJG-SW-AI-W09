/* vm.c: 가상 메모리 객체를 위한 범용 인터페이스. */

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "vm/inspect.h"

/* Returns a hash value for page p. */
static unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}

/* Returns true if page a precedes page b. */
static bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->va < b->va;
}

/* 각 서브시스템의 초기화 코드를 호출하여 가상 메모리 서브시스템을
 * 초기화함. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* project 4용 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. (위 줄은 수정하지 말 것) */
	/* TODO: 여기에 코드를 작성할 것. */
}

/* 페이지의 타입을 가져옴. 이 함수는 페이지가 초기화된 뒤의 타입을 알고
 * 싶을 때 유용함.
 * 이 함수는 이미 완전히 구현되어 있음. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 초기화 함수(initializer)와 함께 대기 중(pending)인 페이지 객체를 생성함.
 * 페이지를 만들고 싶다면 직접 생성하지 말고 이 함수 또는
 * `vm_alloc_page`를 통해 만들 것. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* UPAGE가 이미 사용 중인지 확인함. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: 페이지를 생성하고, VM 타입에 따라 initializer를 가져온 뒤,
		 * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성할 것.
		 * TODO: uninit_new를 호출한 뒤에는 필드를 수정해야 함. */

		/* TODO: 페이지를 spt에 삽입할 것. */
	}
err:
	return false;
}

/* spt에서 VA를 찾아 페이지를 반환함. 오류 시 NULL을 반환함. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page p;
	struct hash_elem *e;

	p.va = pg_round_down(va);
	e = hash_find (&spt->pages, &p.hash_elem);
	return e != NULL ? hash_entry (e, struct page, hash_elem) : NULL;
}

/* 유효성 검사를 거쳐 PAGE를 spt에 삽입함. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	return hash_insert (&spt->pages, &page->hash_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
	return true;
}

/* 쫓겨날(evicted) struct frame을 가져옴. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: eviction 정책은 자유롭게 정할 것. */

	return victim;
}

/* 페이지 하나를 evict하고 해당 frame을 반환함.
 * 오류 시 NULL을 반환함. */
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: victim을 swap out하고 evict된 frame을 반환할 것. */

	return NULL;
}

/* palloc()으로 frame을 얻음. 사용 가능한 페이지가 없으면 페이지를
 * evict하고 그것을 반환함. 이 함수는 항상 유효한 주소를 반환함. 즉, user pool
 * 메모리가 가득 찬 경우 frame을 evict하여 사용 가능한 메모리 공간을
 * 확보함. */
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: 이 함수를 채울 것. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* 스택을 확장함. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* 쓰기 보호된(write_protected) 페이지에서 발생한 fault를 처리함. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 성공하면 true를 반환함. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: fault가 유효한지 검증할 것. */
	/* TODO: 여기에 코드를 작성할 것. */

	return vm_do_claim_page (page);
}

/* 페이지를 해제함.
 * DO NOT MODIFY THIS FUNCTION. (이 함수는 수정하지 말 것) */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 할당된 페이지를 claim함. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: 이 함수를 채울 것. */

	return vm_do_claim_page (page);
}

/* PAGE를 claim하고 mmu를 설정함. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* 링크를 설정함. */
	frame->page = page;
	page->frame = frame;

	/* TODO: 페이지의 VA를 frame의 PA에 매핑하는 page table entry를 삽입할 것. */

	return swap_in (page, frame->kva);
}

/* 새 supplemental page table을 초기화함. */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	if (hash_init(&spt->pages, page_hash, page_less, NULL) == false) {
		PANIC ("failed to init hash");
	}
}

/* supplemental page table을 SRC에서 DST로 복사함. */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* supplemental page table이 보유한 자원을 해제함. */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: 스레드가 보유한 supplemental_page_table을 모두 파괴하고,
	 * TODO: 수정된 내용을 모두 저장소에 writeback할 것. */
}
