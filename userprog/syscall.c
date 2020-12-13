#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct list read_list;
static struct list wait_list;


static void syscall_handler (struct intr_frame *);
bool if_have_waited(tid_t parentd_id);//放在syscall的wait中
struct file_node* FindFileNode(int fd);
int alloc_fd (void); //分配文件标识符
bool is_valid_ptr (const void *usr_ptr);

void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
tid_t exec (const char *file);
int wait (tid_t);//pid和tid是一样的
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&read_list);
  list_init(&wait_list);
  lock_init(&file_lock);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t *esp;
  esp = f->esp;

  if (!is_valid_ptr (esp) || !is_valid_ptr (esp + 1) ||
      !is_valid_ptr (esp + 2) || !is_valid_ptr (esp + 3))
  {
    ExitStatus (-1);
  }
  else
  {
    int syscall_num = *esp;
    switch (syscall_num)
    {
      case SYS_HALT:
        halt ();
        break;
      case SYS_EXIT:
        if(!is_valid_pointer(esp+4,4)){
            ExitStatus(-1);
        }
        int status = *(int *)(esp +4);
        exit(status);
        break;
      case SYS_EXEC:
        if(!is_valid_pointer(esp+4,4)){
          ExitStatus(-1);
        }
        char *file_name = *(char **)(esp+4);
        lock_acquire(&file_lock);
        f->eax = exec (file_name);
        lock_release(&file_lock);
        break;
      case SYS_WAIT:
        tid_t pid;
        if(!is_valid_pointer(esp+4,4)){
          ExitStatus(-1);
        }
        pid = *((int*)esp+1);
        f->eax = wait (pid);
        break;
      case SYS_CREATE:
        if(!is_valid_pointer(esp+4,4)){
          ExitStatus(-1);
        }
        char* file_name = *(char **)(esp+4);
        unsigned size = *(int *)(esp+8);
        f->eax = create (file_name,size);
        break;
      case SYS_REMOVE:
        if (!is_valid_pointer(esp +4, 4)){
          ExitStatus(-1);
        }
        char *file_name = *(char **)(esp+4);
        f->eax = remove (file_name);
        break;
      case SYS_OPEN:
      if (!is_valid_pointer(esp +4, 4)){
          ExitStatus(-1);
        }
        char *file_name = *(char **)(esp+4);
        lock_acquire(&file_lock);
        f->eax = open (file_name);
        lock_release(&file_lock);
        break;
      case SYS_FILESIZE:
        if (!is_valid_pointer(esp +4, 4)){
          ExitStatus(-1);
        }
        int fd = *(int *)(esp + 4);
	      f->eax = filesize ();
	      break;
      case SYS_READ:
        lock_acquire(&file_lock);
        f->eax = read (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        lock_release(&file_lock);
        break;
      case SYS_WRITE:
        lock_acquire(&file_lock);
        f->eax = write (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        lock_release(&file_lock);
        break;
      case SYS_SEEK:
        seek (*(esp + 1), *(esp + 2));
        break;
      case SYS_TELL:
        f->eax = tell (*(esp + 1));
        break;
      case SYS_CLOSE:
        close (*(esp + 1));
        break;
      default:
        ExitStatus(-1);
    }
  }
}

void halt(void)
{
  shutdown_power_off();
}

void exit (int status) 
{
  ExitStatus(status);
}

tid_t exec (const char *file)
{
  return process_execute(file);
}


int wait (tid_t pid){
  // if(if_have_waited(thread_current()->tid))
  // {
  //   return -1;
  //   //已调用wait
  // }
  return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
    return filesys_create(file,initial_size);
}

bool remove (const char *file){
  return filesys_remove(file);
}

int open (const char *file){
    struct file* f = filesys_open(file);
    if(f == NULL){
      return -1;
    }

    struct file_node *f_node = (struct file_node *)malloc(sizeof(struct file_node));                                   
    if(f_node == NULL){
      file_close(f);
      return -1; //不太可能
    }
    struct thread *cur = thread_current();
    f_node->fd = alloc_fd();
    f_node->file = f;
    list_push_back(&cur->file_list,&f_node->elem);
    return f_node->fd;
}

int filesize (int fd){

  struct file *f = FindFileNode(fd);
  if(f == NULL){
    ExitStatus(-1);
    //文件未打开或不存在
  }
  return file_length(f);

}


int read (int fd, void *buffer, unsigned length){
  if(fd==STDIN_FILENO){
    for(unsigned int i=0;i<length;i++){
      *((char **)buffer)[i] = input_getc();
      //copy
    }
    return length;
  }
  else{
    struct file *f = FindFileNode(fd);
    if(f == NULL){
      return -1;
    }
    return file_read(f,buffer,length);
  }
}

int write (int fd, const void *buffer, unsigned length){
  if(fd==STDOUT_FILENO){ // stdout
      putbuf((char *) buffer,(size_t)length);
      return (int)length;
  }
  else{
    struct file *f = FindFileNode(fd);
    if(f==NULL){
      ExitStatus(-1);
      //文件未打开或不存在
    }
    return (int) file_write(f,buffer,length);
  }
}

void seek (int fd, unsigned position)
{
  struct file *f = FindFileNode(fd);
  if(f == NULL){
    ExitStatus(-1);
    //文件未打开或不存在
  }
  file_seek(f,position);
}

unsigned tell (int fd){
  struct file *f = FindFileNode(fd);
  if(f == NULL){
    ExitStatus(-1);
  }
  return file_tell(f);
  //文件未打开或不存在
}

void close (int fd)
{
  CloseFile(fd);
}


int alloc_fd (void)
{
  static int fd = 2;
  return fd++;
}


void ExitStatus(int status)
{
  struct thread *t;
  struct list_elem *l;

  t = thread_current ();
  while (!list_empty (&t->file_list))
  {
    l = list_begin (&t->file_list);
    CloseFile (list_entry (l, struct file_node, elem)->fd);
  }

  t->ret = status;
  thread_exit ();
}

void CloseFile(int fd)
{
  struct file_node *f = FindFileNode(fd);

  // close more than once will fail
  if(f == NULL){
    return;
  }
  file_close (f->file);
  list_remove (&f->elem);
  free (f);
}


void pipe_write(tid_t id,int op,int ret){
  enum intr_level old_level = intr_disable (); // 中断
  struct read_elem* read = malloc(sizeof(struct read_elem));
  read->tid = id;
  read->op = op;
  read->ret_value = ret;
  list_push_back(&read_list,&read->elem);

  //如果有一个线程正在等待此进程结束，则对相应的信号量进行V操作
  struct list_elem *e;
  for(e=list_begin(&wait_list);e!=list_end(&wait_list);e=list_next(e)){
    struct wait_elem *wait = list_entry(e,struct wait_elem,elem);
    if(wait->child_tid == id && wait->op == op)
      sema_up(&wait->WaitSema);
  }
  intr_set_level(old_level);
}


int pipe_read(tid_t p_id,tid_t c_id,int op){
  enum intr_level old_level;
  struct list_elem *e;
  while(true){
    old_level = intr_disable ();
    for(e = list_begin(&read_list); e != list_end(&read_list); e = list_next(e) ){
      struct read_elem *read = list_entry(e,struct read_elem, elem);
      if(read->tid == c_id && read->op == op)
      {
        list_remove(e);
        int value = read->ret_value;
        free(read);
        return value;
      }
      
    }
    //相应进程并未返回
    struct wait_elem *wait = malloc(sizeof(struct wait_elem));
    sema_init(&wait->WaitSema,0);
    wait->child_tid = c_id;
    wait->parent_tid = p_id;
    wait->op = op;
    list_push_back(&wait_list,&wait->elem);
    intr_set_level(old_level);//结束原子操作，允许中断
    sema_down(&wait->WaitSema);

    //在信号量操作结束后，删除wait元素
    list_remove(&wait->elem);
    free(wait);
  }
}

//检查此进程是否已在等待
bool if_have_waited(tid_t parentd_id){
  enum intr_level old_level = intr_disable (); // 中断
  struct list_elem *e;
  for(e=list_begin(&wait_list);e!=list_end(&wait_list);e=list_next(e)){
    struct wait_elem *wait = list_entry(e,struct wait_elem,elem);
    if(wait->parent_tid == parentd_id)
      intr_set_level(old_level);
      return true;
  }
  intr_set_level(old_level);
  return false;
}

struct file_node* FindFileNode(int fd)
{
  struct file_node *retfile;
  struct list_elem *l;
  struct thread *t;

  t = thread_current ();

  for (l = list_begin (&t->file_list); l != list_end (&t->file_list); l = list_next (l))
    {
      retfile = list_entry (l, struct file_node, elem);
      if (retfile->fd == fd)
        return retfile;
    }

  return NULL;
}

bool is_valid_ptr (const void *usr_ptr)
{
  struct thread *cur = thread_current ();
  if (usr_ptr != NULL && is_user_vaddr (usr_ptr))
    {
      return (pagedir_get_page (cur->pagedir, usr_ptr)) != NULL;
    }
  return false;
}

bool is_valid_pointer(void* esp,int cnt){
  //为bad-ptr新加
  int i = 0;
  for (; i < cnt; ++i)
  {
    if((!is_user_vaddr(esp))||(pagedir_get_page(thread_current()->pagedir,esp)==NULL))
    {
      return false;
    }
  }
  return true;
}