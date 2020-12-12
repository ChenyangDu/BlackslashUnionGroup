#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static struct list read_list;
static struct list wait_list;


static void syscall_handler (struct intr_frame *);
bool if_have_waited(tid_t parentd_id);//放在syscall的wait中

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  list_init(&read_list);
  list_init(&wait_list);
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
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


