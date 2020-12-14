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
struct file* FindFile(int fd);
int alloc_fd (void); //分配文件标识符
bool is_valid_ptr (void* esp,int cnt);
bool is_valid_string(void *str);

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

void IHalt(struct intr_frame* f); 
void IExit(struct intr_frame* f); 
void IExec(struct intr_frame* f); 
void IWait(struct intr_frame* f); 
void ICreate(struct intr_frame* f); 
void IRemove(struct intr_frame* f);
void IOpen(struct intr_frame* f); 
void IFilesize(struct intr_frame* f);
void IRead(struct intr_frame* f);  
void IWrite(struct intr_frame* f); 
void ISeek(struct intr_frame* f); 
void ITell(struct intr_frame* f); 
void IClose(struct intr_frame* f); 

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
  int syscall_num = *esp;
  switch (syscall_num)
  {
    case SYS_HALT:
      IHalt(f);
      break;
    case SYS_EXIT:
      IExit(f);
      break;
    case SYS_EXEC:
      IExec(f);
      break;
    case SYS_WAIT:
      IWait(f);
      break;
    case SYS_CREATE:
      ICreate(f);
      break;
    case SYS_REMOVE:
      IRemove(f);
      break;
    case SYS_OPEN:
      IOpen(f);
      break;
    case SYS_FILESIZE:
      IFilesize(f);
      break;
    case SYS_READ:
      IRead(f);
      break;
    case SYS_WRITE:
      IWrite(f);
      break;
    case SYS_SEEK:
      ISeek(f);
      break;
    case SYS_TELL:
      ITell(f);
      break;
    case SYS_CLOSE:
      IClose(f);
      break;
    default:
      ExitStatus(-1);
  }
}

void IHalt(struct intr_frame* f)
{
  halt();
  return;
} 

void IExit(struct intr_frame* f)
{
  if(!is_valid_ptr(f->esp+4,4)){
    ExitStatus(-1);
  }
  int status = *(int *)(f->esp +4);
  exit(status);
} 

void IExec(struct intr_frame* f)
{
  if(!is_valid_ptr(f->esp+4,4)||!is_valid_string(*(char **)(f->esp + 4))){
    ExitStatus(-1);
  }
  char *file_name = *(char **)(f->esp+4);
  lock_acquire(&file_lock);
  f->eax = exec(file_name);
  lock_release(&file_lock);
} 

void IWait(struct intr_frame* f)
{
  int pid;
  if(!is_valid_ptr(f->esp+4,4)){
    ExitStatus(-1);
  }
  pid = *((int*)f->esp+1);
  f->eax = wait(pid);
}

void ICreate(struct intr_frame* f)
{
  if(!is_valid_ptr(f->esp+4,8)){
    ExitStatus(-1);
  }
  char* file_name = *(char **)(f->esp+4);
  if(!is_valid_string(file_name)){
    ExitStatus(-1);
  }
  // if(!is_valid_ptr(f->esp+8,4)){
  //   ExitStatus(-1);
  // }
  unsigned size = *(int *)(f->esp+8);
  f->eax = create(file_name,size);
} 

void IRemove(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 4)){
    ExitStatus(-1);
  }
  char *file_name = *(char **)(f->esp+4);
  if(!is_valid_string(file_name)){
    ExitStatus(-1);
  }
  f->eax = remove(file_name);
}

void IOpen(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 4)){
    ExitStatus(-1);
  }
  char *file_name = *(char **)(f->esp+4);
  if (!is_valid_string(file_name){
    ExitStatus(-1);
  }
  lock_acquire(&file_lock);
  f->eax = open(file_name);
  lock_release(&file_lock);
}

void IFilesize(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 4)){
    ExitStatus(-1);
  }
  int fd = *(int *)(f->esp + 4);
  f->eax = filesize(fd);
}

void IRead(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp + 4, 12)){
    ExitStatus(-1);
  }
  int fd = *(int *)(f->esp + 4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_ptr(buffer, 1) || !is_valid_ptr(buffer + size,1)){
    ExitStatus(-1);
  }
  lock_acquire(&file_lock);
  f->eax = read(fd,buffer,size);
  lock_release(&file_lock);
}  

void IWrite(struct intr_frame* f)
{
  if(!is_valid_ptr(f->esp+4,12)){
    ExitStatus(-1);
  }
  int fd = *(int *)(f->esp +4);
  void *buffer = *(char**)(f->esp + 8);
  unsigned size = *(unsigned *)(f->esp + 12);
  if (!is_valid_ptr(buffer, 1) || !is_valid_ptr(buffer + size,1)){
    ExitStatus(-1);
  }
  lock_acquire(&file_lock);
  f->eax = write(fd,buffer,size);
  lock_release(&file_lock);
  return;
} 

void ISeek(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 8)){
    ExitStatus(-1);
  }
  int fd = *(int *)(f->esp + 4);
  unsigned pos = *(unsigned *)(f->esp + 8);
  seek(fd,pos);
} 

void ITell(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 4)){
    ExitStatus(-1);
  }
  int fd = *(int *)(f->esp + 4);
  f->eax = tell(fd);
} 

void IClose(struct intr_frame* f)
{
  if (!is_valid_ptr(f->esp +4, 4)){
    return ExitStatus(-1);
  }
  int fd = *(int *)(f->esp + 4);
  close(fd);
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
  if(if_have_waited(thread_current()->tid))
  {
    return -1;
    //已调用wait
  }
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

  struct file *f = FindFile(fd);
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
    struct file *f = FindFile(fd);
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
    struct file *f = FindFile(fd);
    if(f==NULL){
      ExitStatus(-1);
      //文件未打开或不存在
    }
    return (int) file_write(f,buffer,length);
  }
}

void seek (int fd, unsigned position)
{
  struct file *f = FindFile(fd);
  if(f == NULL){
    ExitStatus(-1);
    //文件未打开或不存在
  }
  file_seek(f,position);
}

unsigned tell (int fd){
  struct file *f = FindFile(fd);
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

struct file* FindFile(int fd)
{
  struct file_node *fdn;

  fdn = FindFileNode(fd);
  if (fdn == NULL)
    return NULL;
  return fdn->file;
}

bool is_valid_ptr(void* esp,int cnt){
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

//have a try尝试一下，不知道哪出问题了
bool is_valid_string(void *str){
  int ch=-1;
  while((ch=get_user((uint8_t*)str++))!='\0' && ch!=-1);
  if(ch=='\0')
    return true;
  else
    return false;
}

//魔改提供的get_user函数
static int
get_user (const uint8_t *uaddr)
{
  if(!is_user_vaddr((void *)uaddr)){
    return -1;
  }
  if(pagedir_get_page(thread_current()->pagedir,uaddr)==NULL){
    return -1;
  }
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}