#include "userprog/syscall.h"
// ++++++++++++
#include "threads/vaddr.h"

#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
// ++++++++++++++++++++++++
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "process.h"
#include <string.h>
#include "devices/shutdown.h"


// ++++++++++++++++
#define MAXCALL 21
#define MaxFiles 200
// #define	STDOUT_FILENO	1
#define stdin 1

struct file_node
{
  int fd;
  struct list_elem elem;
  struct file *f;
};


static void syscall_handler (struct intr_frame *);
// ++++++++
typedef void (*CALL_PROC)(struct intr_frame*);
CALL_PROC pfn[MAXCALL];

void IWrite(struct intr_frame* f);
void IExit(struct intr_frame* f);
void ExitStatus(int status);
void ICreate(struct intr_frame* f);
void IOpen(struct intr_frame* f);
void IClose(struct intr_frame* f);
void IRead(struct intr_frame* f);
void IFileSize(struct intr_frame* f);
void IExec(struct intr_frame* f);
void IWait(struct intr_frame* f);
void ISeek(struct intr_frame* f);
void IRemove(struct intr_frame* f);
void ITell(struct intr_frame* f);
void IHalt(struct intr_frame* f);
struct file_node *GetFile(struct thread *t, int fd);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  // ++++++++++++++
  int i;
  for ( i = 0; i < MAXCALL; i++)
  {
    pfn[i] = NULL;
  }
  
  pfn[SYS_WRITE] = IWrite;
  pfn[SYS_EXIT] = IExit;
  pfn[SYS_CREATE] = ICreate;
  pfn[SYS_OPEN] = IOpen;
  pfn[SYS_CLOSE] = IClose;
  pfn[SYS_READ] = IRead;
  pfn[SYS_FILESIZE] = IFileSize;
  pfn[SYS_EXEC] = IExec;
  pfn[SYS_WAIT] = IWait;
  pfn[SYS_SEEK] = ISeek;
  pfn[SYS_REMOVE] = IRemove;
  pfn[SYS_TELL] = ITell;
  pfn[SYS_HALT] = IHalt;

}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // 原代码
  // printf ("system call!\n");
  // thread_exit ();

  // +++++++++++++++++++++
  if(!is_user_vaddr(f->esp)){
    ExitStatus(-1);
  }
  int No = *((int*) (f->esp));
  if (No >= MAXCALL || MAXCALL < 0)
  {
    printf("We don't have this System Call!\n");
    ExitStatus(-1);
  }
  if (pfn[No] == NULL)
  {
    printf("this System Call %d not Implement!\n", No);
    ExitStatus(-1);
  }
  pfn[No](f);
}


// ++++++++++++++++++++++++++++++++++
void IWrite(struct intr_frame* f) // 三个参数
{
  int *esp = (int *)f->esp;
  if (!is_user_vaddr(esp+7)) // ??应该是虚拟地址空间的判断
  {
    ExitStatus(-1);
  }
  //可能会有问题
  int fd = *(esp+2); //文件句柄
  char *buffer = (char *)*(esp+6); // 要输出输入缓冲
  unsigned size = *(esp+3); // 输出内容大小

  if (fd == STDOUT_FILENO) // 标准输出设备
  {
    putbuf(buffer, size);
    f->eax = 0;
  } else
  {
    struct thread *cur = thread_current();
    struct file_node *fn = GetFile(cur, fd); // 获取文件指针
    if (fn == NULL)
    {
      f->eax = 0;
      return;
    }
    f->eax = file_write(fn->f, buffer, size); // 写文件
  }
  
  
}

void IExit(struct intr_frame* f) // 一个参数，正常退出时调用
{
  if (!is_user_vaddr(((int*)f->esp)+2))
  {
    ExitStatus(-1);
  }
  struct thread *cur = thread_current();
  cur->ret = *((int*)f->esp + 1);
  f->eax = 0;
  thread_exit();
}

void ExitStatus(int status) // 非正常退出
{
  struct thread *cur = thread_current();
  cur->ret = status;
  thread_exit();
}

void ICreate(struct intr_frame* f){

}

void IOpen(struct intr_frame* f){

}

void IClose(struct intr_frame* f){

}

void IRead(struct intr_frame* f){

}

void IFileSize(struct intr_frame* f){

}

void IExec(struct intr_frame* f){
  if (!is_user_vaddr(((int*)f->esp)+2))
  {
    ExitStatus(-1);
  }
  const char *file = (char*)*((int *)f->esp+1);
  tid_t tid = -1;
  if (file == NULL)
  {
    f->eax = -1;
    return;
  }
  char *newfile = (char*)malloc(sizeof(char)*(strlen(file)+1));

  memcpy(newfile, file, strlen(file) + 1);
  tid = process_execute(newfile);
  struct thread *t = GetThreadFromTid(tid);
  sema_down(&t->SemaWaitSuccess);
  f->eax = t->tid;
  t->father->sons++;
  free(newfile);
  sema_up(&t->SemaWaitSuccess);
}

void IWait(struct intr_frame* f){

}

void ISeek(struct intr_frame* f){

}

void IRemove(struct intr_frame* f){

}

void ITell(struct intr_frame* f){

}

void IHalt(struct intr_frame* f){

}

struct file_node *GetFile(struct thread *t, int fd){
  struct list_elem *e;
  for(e = list_begin(&t->file_list);e!=list_end(&t->file_list);e=list_next(e))
  {
    struct file_node *fn = list_entry (e, struct file_node, elem);
    if(fn -> fd == fd)
    {
      return fn;
    }
    return NULL;
    
  }

}
