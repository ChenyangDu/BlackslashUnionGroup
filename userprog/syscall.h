#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/synch.h"
#include "threads/thread.h"
#define THREAD_START 0
#define WAIT_THREAD 1

struct lock file_lock;

struct file_node{//文件标识符
  int fd;
  struct list_elem elem; 
  struct file* file; 
};

struct child_process{
  tid_t tid; // 进程标识符
  struct list_elem elem;
};

struct read_elem{//管道读元素
  tid_t tid;
  int op;
  struct list_elem elem;
  int ret_value;
};

struct wait_elem{//管道等待元素
  tid_t parent_tid;
  //等待只由父进程调用，防止出现二次调用父进程的情况
  tid_t child_tid;
  int op;
  struct list_elem elem;
  struct semaphore WaitSema;
};


void syscall_init (void);
void ExitStatus(int status); //与syscall调用的exit区分
void CloseFile(int fd); //与syscall调用的close区分
void pipe_write(tid_t id,int op,int ret);
int pipe_read(tid_t p_id,tid_t c_id,int op);

//struct lock file_lock;

#endif /* userprog/syscall.h */
