#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>
#include "threads/synch.h"

struct file;

extern struct lock filesys_lock;

#define FD_MAX 64	/* fd 테이블 칸 수 */
#define STDIN_FD 0
#define STDOUT_FD 1
#define FD_FIRST 2	/* fd 테이블에 쓰는 첫 fd (0, 1은 stdin/stdout) */

void syscall_init (void);

/* fd 테이블 */
void fd_table_create (void);
int fd_add (struct file *file);
void fd_close (int fd);
void fd_table_close_all (void);
void sys_exit (int status) NO_RETURN;
#endif /* userprog/syscall.h */
