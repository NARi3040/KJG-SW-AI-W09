#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "threads/init.h"
#include "vm/vm.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* 시스템 콜(System call).
 *
 * 예전에는 시스템 콜 서비스를 인터럽트 핸들러(interrupt handler)가 처리했음
 * (예: linux의 int 0x80). 하지만 x86-64에서는 제조사가 시스템 콜을 요청하는
 * 효율적인 경로인 `syscall` 명령어를 제공함.
 *
 * syscall 명령어는 모델 특정 레지스터(Model Specific Register, MSR)의
 * 값을 읽어서 동작함. 자세한 내용은 매뉴얼을 참고할 것. */

#define MSR_STAR 0xc0000081         /* 세그먼트 셀렉터(Segment selector) msr */
#define MSR_LSTAR 0xc0000082        /* 롱 모드(Long mode) SYSCALL 대상 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

#define IO_CHUNK 512                /* read/write가 커널 버퍼를 거치는 단위 (디스크 섹터 크기) */
#define NAME_BUF 128                /* 파일 이름을 커널로 복사해 두는 버퍼 크기 */
struct lock filesys_lock;           /* 전역 파일 시스템 락 */

/* ---- 헬퍼 함수 ---- */

/* 시스템 콜 핸들러 타입과 번호별 핸들러 선언. */
typedef void syscall_fn(struct intr_frame *f);

static syscall_fn handle_halt, handle_exit, handle_fork, handle_exec, handle_wait,
	handle_create, handle_remove, handle_open, handle_filesize, handle_read,
	handle_write, handle_seek, handle_tell, handle_close,
	handle_dup2, handle_mmap, handle_munmap;

/* 디스패치, 종료 기록, 인자 복사, read/write 청크 처리용 도우미. */
static void record_exit_status(struct thread *t, int status);
static char *copy_user_cmdline(const char *cmd);
static bool copy_user_path(const char *user_path, char kname[NAME_BUF]);
static int fd_add_or_close(struct file *file);
static unsigned read_stdin(uint8_t *buffer, unsigned size);
static int read_file_to_user(struct file *file, void *buffer, unsigned size);
static int write_user_to_file(struct file *file, const void *buffer, unsigned size);
static unsigned chunk_len(unsigned left);

/* 유저 포인터 검증. */
static void check_user_buffer(const void *buf, size_t size, bool write);
static void check_user_page(const void *addr, bool write);
static void check_user_addr(const void *addr);
static bool is_user_page_valid(const void *addr);
static bool is_user_page_writable(const void *addr);
static size_t user_strnlen_checked(const char *s, size_t max);

/* fd 테이블 조회. */
static struct file *fd_lookup(int fd);
static bool is_file_fd(int fd);

/* filesys_lock을 잡고 파일 시스템 함수를 부르는 래퍼. */
static bool fs_create(const char *name, unsigned initial_size);
static bool fs_remove(const char *name);
static struct file *fs_open(const char *name);
static void fs_close(struct file *file);
static int fs_length(struct file *file);
static int fs_tell(struct file *file);
static void fs_seek(struct file *file, unsigned position);
static int fs_read(struct file *file, void *kbuf, unsigned n);
static int fs_write(struct file *file, const void *kbuf, unsigned n);

/* syscall MSR과 전역 filesys_lock을 초기화함. */
void
syscall_init (void) {
	lock_init(&filesys_lock);
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* 인터럽트 서비스 루틴(interrupt service routine)은 syscall_entry가
	 * 유저랜드 스택을 커널 모드 스택으로 바꿀 때까지 어떤 인터럽트도
	 * 처리하면 안 됨. 따라서 FLAG_FL을 마스크함. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* rax 번호로 핸들러를 호출함. 모르는 번호는 -1. */
// 예쁘당
void
syscall_handler (struct intr_frame *f) {
	thread_current()->user_rsp = f->rsp;
	
	switch (f->R.rax) {
	case SYS_HALT:     handle_halt(f);     break;
	case SYS_EXIT:     handle_exit(f);     break;
	case SYS_FORK:     handle_fork(f);     break;
	case SYS_EXEC:     handle_exec(f);     break;
	case SYS_WAIT:     handle_wait(f);     break;
	case SYS_CREATE:   handle_create(f);   break;
	case SYS_REMOVE:   handle_remove(f);   break;
	case SYS_OPEN:     handle_open(f);     break;
	case SYS_FILESIZE: handle_filesize(f); break;
	case SYS_READ:     handle_read(f);     break;
	case SYS_WRITE:    handle_write(f);    break;
	case SYS_SEEK:     handle_seek(f);     break;
	case SYS_TELL:     handle_tell(f);     break;
	case SYS_CLOSE:    handle_close(f);    break;
	case SYS_DUP2:     handle_dup2(f);     break;
	case SYS_MMAP:     handle_mmap(f);     break;
	case SYS_MUNMAP:   handle_munmap(f);   break;
	default:           f->R.rax = -1;      break;
	}
}

/* ---- 프로세스 종료 ---- */

/* 종료 상태를 기록하고 프로세스를 끝냄(exit, 잘못된 포인터, page fault 공통). */
void sys_exit (int status) {
	record_exit_status (thread_current (), status);
	thread_exit ();
}

/* exit_info malloc 실패에 대비해 self_info에도 종료 상태를 씀. */
static void record_exit_status(struct thread *t, int status) {
	if (t->self_info != NULL)
		t->self_info->exit_status = status;
	if (t->exit_info == NULL)
		t->exit_info = malloc (sizeof *t->exit_info);
	if (t->exit_info != NULL)
		t->exit_info->status = status;
}

/* ---- 프로세스 시스템 콜 ---- */

static void handle_halt(struct intr_frame *f UNUSED) {
	power_off();
}

static void handle_exit(struct intr_frame *f) {
	sys_exit((int)f->R.rdi);
}

static void handle_fork(struct intr_frame *f) {
	f->R.rax = process_fork((const char *)f->R.rdi, f);
}

static void handle_wait(struct intr_frame *f) {
	f->R.rax = process_wait(f->R.rdi);
}

static void handle_exec(struct intr_frame *f) {
	char *cmdline = copy_user_cmdline((const char *)f->R.rdi);

	if (cmdline == NULL) {
		sys_exit(-1);
	}
	process_exec(cmdline);	// 성공하면 돌아오지 않음
	sys_exit(-1);
}

/* 검사 후 페이지에 명령줄을 복사함(검사 중 exit 시 누수 방지). 실패하면 NULL. */
static char *copy_user_cmdline(const char *cmd) {
	size_t len = user_strnlen_checked(cmd, PGSIZE);
	if (len == PGSIZE) {
		return NULL;
	}

	char *k = palloc_get_page(0);
	if (k == NULL) {
		return NULL;
	}
	memcpy(k, cmd, len);
	k[len] = '\0';
	return k;
}

/* ---- 경로 이름을 받는 파일 시스템 콜 ---- */

/* 파일 시스템 콜은 모두 같은 순서를 지킴.
 * 1. 유저 포인터 검증 (check_user_addr, check_user_buffer)
 * 2. 유저 데이터를 커널 버퍼로 복사 (copy_user_path, IO_CHUNK 버퍼)
 * 3. filesys_lock을 잡고 파일 시스템 함수 호출, 바로 해제 (fs_* 함수)
 * 4. 결과를 rax에 씀
 * 락을 쥔 채 유저 메모리를 건드리지 않는 것이 핵심 규칙임. 프로젝트 3에서 page fault 처리가
 * 같은 락을 잡으면 재귀 획득으로 패닉이 나기 때문임(락은 재귀 불가). */

static void handle_create(struct intr_frame *f) {
	unsigned initial_size = (unsigned)f->R.rsi;
	char kname[NAME_BUF];

	bool ok = copy_user_path((const char *)f->R.rdi, kname);
	f->R.rax = ok && fs_create(kname, initial_size);
}

static void handle_remove(struct intr_frame *f) {
	char kname[NAME_BUF];

	bool ok = copy_user_path((const char *)f->R.rdi, kname);
	f->R.rax = ok && fs_remove(kname);
}

static void handle_open(struct intr_frame *f) {
	char kname[NAME_BUF];

	if (!copy_user_path((const char *)f->R.rdi, kname)) {
		f->R.rax = -1;
		return;
	}
	struct file *file = fs_open(kname);
	f->R.rax = file == NULL ? -1 : fd_add_or_close(file);
}

/* 유저 경로를 검증해 커널 버퍼로 복사함. 너무 길면 false. */
static bool copy_user_path(const char *user_path, char kname[NAME_BUF]) {
	size_t len = user_strnlen_checked(user_path, NAME_BUF);
	if (len == NAME_BUF) {
		return false;
	}
	memcpy(kname, user_path, len);
	kname[len] = '\0';
	return true;
}

/* fd 테이블이 꽉 차면 열어 둔 파일을 닫고 -1. */
static int fd_add_or_close(struct file *file) {
	int fd = fd_add(file);
	if (fd == -1) {
		fs_close(file);
	}
	return fd;
}

/* ---- fd를 받는 파일 시스템 콜 ---- */

static void handle_filesize(struct intr_frame *f) {
	struct file *file = fd_lookup(f->R.rdi);
	f->R.rax = file == NULL ? -1 : fs_length(file);
}

static void handle_tell(struct intr_frame *f) {
	struct file *file = fd_lookup(f->R.rdi);
	f->R.rax = file == NULL ? -1 : fs_tell(file);
}

static void handle_seek(struct intr_frame *f) {
	struct file *file = fd_lookup(f->R.rdi);
	unsigned position = (unsigned)f->R.rsi;

	// off_t(int32)로 바꾸면 음수가 되어 file_seek의 ASSERT에서 커널 패닉이 나므로 무시
	if (file != NULL && position <= INT32_MAX) {
		fs_seek(file, position);
	}
}

static void handle_read(struct intr_frame *f) {
	int fd = f->R.rdi;
	void *buffer = (void *)f->R.rsi;
	unsigned size = (unsigned)f->R.rdx;

	check_user_buffer(buffer, size, true);

	if (fd == STDIN_FD) {
		f->R.rax = read_stdin(buffer, size);
	} else {
		struct file *file = fd_lookup(fd);
		f->R.rax = file == NULL ? -1 : read_file_to_user(file, buffer, size);
	}
}

static void handle_write(struct intr_frame *f) {
	int fd = f->R.rdi;
	const void *buffer = (const void *)f->R.rsi;
	unsigned size = (unsigned)f->R.rdx;

	check_user_buffer(buffer, size, false);

	if (fd == STDOUT_FD) {
		putbuf(buffer, size);
		f->R.rax = size;
	} else {
		struct file *file = fd_lookup(fd);
		f->R.rax = file == NULL ? -1 : write_user_to_file(file, buffer, size);
	}
}

static void handle_close(struct intr_frame *f) {
	fd_close(f->R.rdi);
}

static unsigned read_stdin(uint8_t *buffer, unsigned size) {
	for (unsigned i = 0; i < size; i++) {
		buffer[i] = input_getc();
	}
	return size;
}

/* IO_CHUNK 커널 버퍼를 거쳐 읽음(락을 쥔 채 유저 메모리 fault 시 락 재획득 패닉 방지). */
static int read_file_to_user(struct file *file, void *buffer, unsigned size) {
	char kbuf[IO_CHUNK];
	unsigned done = 0;
	while (done < size) {
		unsigned n = chunk_len(size - done);
		int got = fs_read(file, kbuf, n);
		if (got <= 0) {
			break;
		}
		memcpy((char *)buffer + done, kbuf, got);
		done += got;
		if (got < (int)n) {
			break;
		}
	}
	return done;
}

static int write_user_to_file(struct file *file, const void *buffer, unsigned size) {
	char kbuf[IO_CHUNK];
	unsigned done = 0;
	while (done < size) {
		unsigned n = chunk_len(size - done);
		memcpy(kbuf, (const char *)buffer + done, n);
		int put = fs_write(file, kbuf, n);
		done += put;
		if (put < (int)n) {
			break;
		}
	}
	return done;
}

static unsigned chunk_len(unsigned left) {
	return left < IO_CHUNK ? left : IO_CHUNK;
}

/* ---- 미구현 시스템 콜 (형식만 둠, 항상 실패값 반환) ---- */
static void handle_dup2(struct intr_frame *f) {
	f->R.rax = -1;
}

static void handle_mmap(struct intr_frame *f) {
	f->R.rax = 0;	// 실패 시 NULL
}

static void handle_munmap(struct intr_frame *f UNUSED) {
}

/* ---- 유저 메모리 검사 ---- */

/* 버퍼가 걸친 페이지를 모두 검사함(CR0.WP가 꺼져 있어 WRITE면 권한도 직접 봄). */
static void check_user_buffer(const void *buf, size_t size, bool write) {
	check_user_page(buf, write);
	if (size == 0) {
		return;
	}
	const char *end = (const char *)buf + size - 1;
	for (const char *p = (const char *)pg_round_down(buf) + PGSIZE; p <= end; p += PGSIZE) {
		check_user_page(p, write);
	}
}

/* 페이지 하나가 접근 가능한지, WRITE면 쓰기 권한까지 검사함. */
static void check_user_page(const void *addr, bool write) {
	check_user_addr(addr);
	if (write && !is_user_page_writable(addr)) {
		sys_exit(-1);
	}
}

/* ADDR의 유저 페이지가 접근 불가면 sys_exit(-1). */
static void check_user_addr(const void *addr) {
	if (addr == NULL || !is_user_vaddr(addr) || !is_user_page_valid(addr)) {
		sys_exit(-1);
	}
}

/* VM 빌드의 lazy 페이지는 첫 접근 전까지 pml4에 매핑이 없으므로 SPT 등록 여부로 판단함. */
static bool is_user_page_valid(const void *addr) {
#ifdef VM
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = spt_find_page (spt, (void *)addr);
	if (page == NULL) {
		uintptr_t rsp = thread_current ()->user_rsp;
		page = vm_try_grow_stack((void *)addr, rsp); //이 스택으로 등록된 page 포인터 반환 
	}
	return page != NULL;
#else
	return pml4_get_page(thread_current()->pml4, addr) != NULL;
#endif
}

/* VM 빌드는 SPT의 writable을 봄(lazy 페이지는 PTE가 없어 pml4e_walk()가 NULL). */
static bool is_user_page_writable(const void *addr) {
#ifdef VM
	struct page *page = spt_find_page(&thread_current()->spt, (void *)addr);
	return page != NULL && page->writable;
#else
	return is_writable(pml4e_walk(thread_current()->pml4, (uint64_t)addr, false));
#endif
}

/* 페이지마다 검사하며 유저 문자열 길이를 셈. MAX 안에 NUL이 없으면 MAX. */
static size_t user_strnlen_checked(const char *s, size_t max) {
	for (size_t len = 0; len < max; len++) {
		if (len == 0 || pg_ofs(s + len) == 0) {
			check_user_addr(s + len);
		}
		if (s[len] == '\0') {
			return len;
		}
	}
	return max;
}

/* ---- fd 테이블 ---- */

/* fd 테이블을 할당함. 실패하면 NULL로 남아 fd_add()가 -1. */
void fd_table_create(void) {
	thread_current()->fd_table = calloc(FD_MAX, sizeof(struct file *));
}

/* FILE을 가장 작은 빈 fd에 등록함. 실패하면 -1, FILE은 닫지 않음. */
int fd_add(struct file *file) {
	struct file **table = thread_current()->fd_table;

	if (table == NULL) {
		return -1;
	}
	for (int fd = FD_FIRST; fd < (int)FD_MAX; fd++) {
		if (table[fd] == NULL) {
			table[fd] = file;
			return fd;
		}
	}
	return -1;
}

/* fd 칸을 먼저 비우고 파일을 닫음. 잘못된 fd는 무시함. */
void fd_close(int fd) {
	struct file *file = fd_lookup(fd);

	if (file == NULL) {
		return;
	}
	thread_current()->fd_table[fd] = NULL;
	fs_close(file);
}

/* 열린 파일을 모두 닫고 fd 테이블을 해제함(재호출해도 안전). */
void fd_table_close_all(void) {
	struct thread *cur = thread_current();

	if (cur->fd_table == NULL) {
		return;
	}
	for (int fd = FD_FIRST; fd < (int)FD_MAX; fd++) {
		fd_close(fd);
	}
	free(cur->fd_table);
	cur->fd_table = NULL;
}

/* fd의 열린 파일을 반환함. stdin/stdout이나 잘못된 fd는 NULL. */
static struct file *fd_lookup(int fd) {
	struct file **table = thread_current()->fd_table;

	if (table == NULL || !is_file_fd(fd)) {
		return NULL;
	}
	return table[fd];
}

static bool is_file_fd(int fd) {
	return fd >= FD_FIRST && fd < (int)FD_MAX;
}

/* ---- filesys_lock 아래에서 부르는 파일 시스템 함수 ---- */

static bool fs_create(const char *name, unsigned initial_size) {
	lock_acquire(&filesys_lock);
	bool ok = filesys_create(name, initial_size);
	lock_release(&filesys_lock);
	return ok;
}

static bool fs_remove(const char *name) {
	lock_acquire(&filesys_lock);
	bool ok = filesys_remove(name);
	lock_release(&filesys_lock);
	return ok;
}

static struct file *fs_open(const char *name) {
	lock_acquire(&filesys_lock);
	struct file *file = filesys_open(name);
	lock_release(&filesys_lock);
	return file;
}

static void fs_close(struct file *file) {
	lock_acquire(&filesys_lock);
	file_close(file);
	lock_release(&filesys_lock);
}

static int fs_length(struct file *file) {
	lock_acquire(&filesys_lock);
	int len = file_length(file);
	lock_release(&filesys_lock);
	return len;
}

static int fs_tell(struct file *file) {
	lock_acquire(&filesys_lock);
	int pos = file_tell(file);
	lock_release(&filesys_lock);
	return pos;
}

static void fs_seek(struct file *file, unsigned position) {
	lock_acquire(&filesys_lock);
	file_seek(file, position);
	lock_release(&filesys_lock);
}

static int fs_read(struct file *file, void *kbuf, unsigned n) {
	lock_acquire(&filesys_lock);
	int got = file_read(file, kbuf, n);
	lock_release(&filesys_lock);
	return got;
}

static int fs_write(struct file *file, const void *kbuf, unsigned n) {
	lock_acquire(&filesys_lock);
	int put = file_write(file, kbuf, n);
	lock_release(&filesys_lock);
	return put;
}
