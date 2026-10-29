/* vm.c: 가상 메모리 객체를 위한 범용 인터페이스. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include <string.h>

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
static bool duplicate_page(struct supplemental_page_table *dst, struct page *src_page);
static struct page *claim_empty_child_page(struct supplemental_page_table *dst, struct page *src_page);
static bool is_uninit_page(const struct page *page);
static bool fill_from_initializer(struct page *dst_page, struct page *src_page);
static bool fill_from_frame(struct page *dst_page, struct page *src_page);
static void page_destroy_func(struct hash_elem *e, void *aux);

/* 초기화 함수(initializer)와 함께 대기 중(pending)인 페이지 객체를 생성함.
 * 페이지를 만들고 싶다면 직접 생성하지 말고 이 함수 또는
 * `vm_alloc_page`를 통해 만들 것. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	// UPAGE가 이미 사용 중인지 확인함
	if (spt_find_page (spt, upage) != NULL) { goto err; }

	// 마커를 뗀 기본 타입으로 변신용 initializer를 고름
	bool (*initializer) (struct page *, enum vm_type, void *);
	switch (VM_TYPE (type)) {
		case VM_ANON: { initializer = anon_initializer; break; }
		case VM_FILE: { initializer = file_backed_initializer; break; }
		default: goto err;
	}

	struct page *page = malloc (sizeof *page);
	if (page == NULL)
		goto err;

	uninit_new (page, upage, init, type, aux, initializer);
	page->writable = writable; // uninit_new가 구조체를 통째로 덮어쓰므로 그 뒤에 설정함 

	if (!spt_insert_page (spt, page)) { free (page); goto err; }
	return true;
err:
	return false;
}

/* spt에서 VA를 찾아 페이지를 반환함. 오류 시 NULL을 반환함. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;

	struct page tmp;
	tmp.va = pg_round_down (va); 
	struct hash_elem *e = hash_find(&spt->pages, &tmp.hash_elem); //구조체는 넘길때 &해야 주소로 넘어감, 배열과 다름!
	if(e==NULL) return NULL;
	page = hash_entry(e, struct page, hash_elem);

	return page;
}

/* 유효성 검사를 거쳐 PAGE를 spt에 삽입함. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {
	int succ = false;
	/* TODO: Fill this function. */
	succ = hash_insert(&spt->pages, &page->hash_elem) == NULL;

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	vm_dealloc_page (page);
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
	void *kva = palloc_get_page (PAL_USER | PAL_ZERO); // 새 프레임 노출 방지, evict 재사용분은 anon_initializer가 따로 채움
	if (kva == NULL)
		PANIC ("todo"); // shortcut: 사용자 풀 고갈 시 패닉, swap 구현 시 vm_evict_frame으로 교체

	struct frame *frame = malloc (sizeof *frame);
	if (frame == NULL) {
		palloc_free_page (kva);
		return NULL;
	}
	frame->kva = kva;
	frame->page = NULL;
	return frame;
}

/* 양방향 연결을 끊고 프레임 기록과 물리 페이지를 반납함. (claim 실패 경로, 타입별 destroy) */
void
vm_free_frame (struct page *page, struct frame *frame) {
	page->frame = NULL;
	palloc_free_page (frame->kva);
	free (frame);
}

/* 스택을 확장함. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* 쓰기 보호된(write_protected) 페이지에서 발생한 fault를 처리함. */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* 유저 주소의 "페이지 없음" 폴트만 lazy loading 후보임. user 값은 안 봄 (시스템 콜 중 커널 모드 폴트도 처리) */
static bool
is_lazy_fault_candidate (void *addr, bool not_present) {
	return addr != NULL && is_user_vaddr (addr) && not_present;
}

/* 폴트 주소의 SPT 페이지. 없거나 읽기 전용에 쓰려 하면 NULL (진짜 폴트). */
static struct page *
find_accessible_page (void *addr, bool write) {
	struct page *page = spt_find_page (&thread_current ()->spt, addr);
	if (page == NULL || (write && !page->writable))
		return NULL;
	return page;
}

/* 성공하면 true를 반환함. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr,
		bool user UNUSED, bool write, bool not_present) {
	if (!is_lazy_fault_candidate (addr, not_present))
		return false;

	struct page *page = find_accessible_page (addr, write);
	return page != NULL && vm_do_claim_page (page);
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
vm_claim_page (void *va) {
	if (va == NULL || !is_user_vaddr (va))
		return false;
	/* 경계 내림은 spt_find_page가 함 */
	struct page *page = spt_find_page (&thread_current ()->spt, va);
	if (page == NULL)
		return false;
	return vm_do_claim_page (page);
}

/* PAGE를 claim하고 mmu를 설정함. */
static bool
vm_do_claim_page (struct page *page) {
	if (page == NULL)
		return false;
	/* 이미 프레임이 붙은 페이지는 다시 할당하지 않음 */
	if (page->frame != NULL)
		return true;

	uint64_t *pml4 = thread_current ()->pml4;
	/* pml4_set_page는 기존 매핑을 덮어쓰므로 상태 불일치를 여기서 막음 */
	if (pml4_get_page (pml4, page->va) != NULL)
		return false;

	struct frame *frame = vm_get_frame ();
	if (frame == NULL)
		return false;

	/* 링크를 설정함. */
	frame->page = page;
	page->frame = frame;

	if (!pml4_set_page (pml4, page->va, frame->kva, page->writable)) {
		vm_free_frame (page, frame);
		return false;
	}
	if (!swap_in (page, frame->kva)) {
		pml4_clear_page (pml4, page->va);
		vm_free_frame (page, frame);
		return false;
	}
	return true;
}

static uint64_t page_hash_func (const struct hash_elem *e, void *aux UNUSED){
	struct page *p = hash_entry(e, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}
static bool page_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){
	struct page *a_page = hash_entry(a, struct page, hash_elem);
	struct page *b_page = hash_entry(b, struct page, hash_elem);
	return a_page->va < b_page->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	//hash_init (struct hash *h, hash_hash_func *hash, hash_less_func *less, void *aux) {
	if(!hash_init(&spt->pages, page_hash_func, page_less_func, NULL)){
		PANIC ("supplemental_page_table_init FAIL");
	}
}

/* supplemental page table을 SRC에서 DST로 복사함. 
 * 부모의 SPT(src)에 있는 페이지를 하나씩 자식의 SPT(dst)로 복제함.
 */
bool
supplemental_page_table_copy(struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_iterator i;

	hash_first(&i, &src->pages);
	while (hash_next(&i)) { // 부모 SPT 순회
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		if (!duplicate_page(dst, src_page)) // 복제에 실패했다면
			return false;
	}
	return true;
}

/* SRC_PAGE(부모)를 DST(자식)에 복제하고 곧바로 claim해 내용까지 채움.
 * 미초기화 페이지도 미루지 않음.
 */
static bool
duplicate_page(struct supplemental_page_table *dst, struct page *src_page) {
	struct page *dst_page = claim_empty_child_page(dst, src_page);
	if (dst_page == NULL) // 자식 페이지가 없다면
		return false;
	return is_uninit_page(src_page)
		? fill_from_initializer(dst_page, src_page)
		: fill_from_frame(dst_page, src_page);
}

/* SRC_PAGE(부모)와 같은 주소, 권한, 타입으로 내용 채우기 함수 없이 예약하고 claim함. */
static struct page *
claim_empty_child_page(struct supplemental_page_table *dst, struct page *src_page) {
	/* 미초기화 페이지는 마커를 포함한 원래 타입을 그대로 물려줌.*/
	enum vm_type type = is_uninit_page(src_page)
		? src_page->uninit.type
		: page_get_type(src_page);
	void *va = src_page->va;

	// 자식 SPT에 등록 + 즉시 클레임 
	if (!vm_alloc_page(type, va, src_page->writable) || !vm_claim_page(va))
		return NULL;
	return spt_find_page(dst, va);
}

// 페이지가 아직 한 번도 초기화되지 않은 상태(lazy loading 대기 중)인가?
static bool
is_uninit_page(const struct page *page) {
	return VM_TYPE(page->operations->type) == VM_UNINIT;
}

/* SRC_PAGE(부모)의 init을 SRC_PAGE(부모) aux로 직접 부름. */
static bool
fill_from_initializer(struct page *dst_page, struct page *src_page) {
	vm_initializer *init = src_page->uninit.init;
	return init == NULL || init(dst_page, src_page->uninit.aux);
}
/* SRC(부모) 프레임의 내용을 DST(자식) 프레임에 복사함. SRC(부모)가 메모리에 없으면 false. */
static bool
fill_from_frame(struct page *dst_page, struct page *src_page) {
	if (src_page->frame == NULL) // 부모 페이지는 있는데 메모리에 올라와 있지 않다면
		return false;
	memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	return true;
}

/* supplemental page table이 보유한 자원을 해제함. */
void
supplemental_page_table_kill(struct supplemental_page_table *spt) {
	hash_destroy(&spt->pages, page_destroy_func);
}

/* hash_destroy가 원소마다 부르는 해제 함수. */
static void
page_destroy_func(struct hash_elem *e, void *aux UNUSED) {
	vm_dealloc_page(hash_entry(e, struct page, hash_elem));
}
