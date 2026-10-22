#ifndef __LIB_SYSCALL_NR_H
#define __LIB_SYSCALL_NR_H

/* 시스템 콜(system call) 번호. */
enum {
	/* 프로젝트 2 이후. */
	SYS_HALT,                   /* 운영체제를 정지함. */
	SYS_EXIT,                   /* 현재 프로세스를 종료함. */
	SYS_FORK,                   /* 현재 프로세스를 복제함. */
	SYS_EXEC,                   /* 현재 프로세스를 다른 프로그램으로 전환함. */
	SYS_WAIT,                   /* 자식 프로세스가 종료될 때까지 기다림. */
	SYS_CREATE,                 /* 파일을 생성함. */
	SYS_REMOVE,                 /* 파일을 삭제함. */
	SYS_OPEN,                   /* 파일을 엶. */
	SYS_FILESIZE,               /* 파일의 크기를 얻음. */
	SYS_READ,                   /* 파일에서 읽음. */
	SYS_WRITE,                  /* 파일에 씀. */
	SYS_SEEK,                   /* 파일 내 위치를 변경함. */
	SYS_TELL,                   /* 파일 내 현재 위치를 알려줌. */
	SYS_CLOSE,                  /* 파일을 닫음. */

	/* 프로젝트 3, 그리고 선택적으로 프로젝트 4. */
	SYS_MMAP,                   /* 파일을 메모리에 매핑(map)함. */
	SYS_MUNMAP,                 /* 메모리 매핑을 제거함. */

	/* 프로젝트 4 전용. */
	SYS_CHDIR,                  /* 현재 디렉터리를 변경함. */
	SYS_MKDIR,                  /* 디렉터리를 생성함. */
	SYS_READDIR,                /* 디렉터리 엔트리를 읽음. */
	SYS_ISDIR,                  /* 파일 디스크립터(fd)가 디렉터리를 나타내는지 검사함. */
	SYS_INUMBER,                /* 파일 디스크립터(fd)의 inode 번호를 반환함. */
	SYS_SYMLINK,                /* 파일 디스크립터(fd)의 inode 번호를 반환함. */

	/* 프로젝트 2 추가분 */
	SYS_DUP2,                   /* 파일 디스크립터(file descriptor)를 복제함 */

	SYS_MOUNT,
	SYS_UMOUNT,
};

#endif /* lib/syscall-nr.h */
