#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
struct file* fd2fp(int fd);

#endif /* userprog/syscall.h */
