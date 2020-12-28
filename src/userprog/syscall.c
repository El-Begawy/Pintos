#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <string.h>
#include <threads/malloc.h>
#include <filesys/file.h>
#include <devices/input.h>
#include "process.h"

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static int mem_read (void *src, void *dist, int size);
static uint32_t write_call (void *esp);
static uint32_t read_call (void *esp);
static void sys_exit (int status);
static bool create_file (void *esp);
static int open_file (void *esp);
static int filesize (int fd);
static void sys_seek (void *esp);
static unsigned sys_tell (int fd);
static struct file_descriptor *find_descriptor_by_fd (int fd);
static void add_file_desc_to_list (struct file_descriptor *file_desc);
static void sys_close (int fd);
static int sys_execute (void *esp);
static int sys_wait (tid_t pid);

struct lock file_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&file_lock);
}

static void
syscall_handler (struct intr_frame *f)
{
  int code;
  mem_read (f->esp, &code, sizeof (code));
  // printf("Code is:%d \n", *(int *)f->esp);
  switch (code)
    {
      case SYS_HALT:
        {
          shutdown_power_off ();
          break;
        }
      case SYS_EXIT:
        { // TODO EDIT THIS!! wake up parent.. do what needs to be done.. return status.
          int status;
          mem_read ((int *) f->esp + 1, &status, sizeof (status));
          sys_exit (status);
          break;
        }
      case SYS_EXEC:
        {
          f->eax = sys_execute (f->esp);
          break;
        }
      case SYS_WAIT:
        {
          tid_t pid;
          mem_read ((tid_t *) f->esp + 1, &pid, sizeof (pid));
          f->eax = sys_wait(pid);
          break;
        }
      case SYS_CREATE:
        {
          f->eax = create_file (f->esp);
          break;
        }
      case SYS_REMOVE:
        {

          break;
        }
      case SYS_OPEN:
        {
          f->eax = open_file (f->esp);
          break;
        }
      case SYS_FILESIZE:
        {
          int fd;
          mem_read ((int *) f->esp + 1, &fd, sizeof (fd));
          f->eax = filesize (fd);
          break;
        }
      case SYS_READ:
        {
          f->eax = read_call (f->esp);
          break;
        }
      case SYS_WRITE:
        {
          f->eax = write_call (f->esp);
          break;
        }
      case SYS_SEEK:
        {
          sys_seek (f->esp);
          break;
        }
      case SYS_TELL:
        {
          int fd;
          mem_read ((int *) f->esp + 1, &fd, sizeof (fd));
          f->eax = sys_tell (fd);
          break;
        }
      case SYS_CLOSE:
        {
          int fd;
          mem_read ((int *) f->esp + 1, &fd, sizeof (fd));
          sys_close (fd);
          break;
        }
    }
}

static int sys_execute (void *esp)
{
  const char *buffer;
  mem_read ((int *) esp + 1, &buffer, sizeof (buffer));
  if (buffer == NULL || get_user ((uint8_t *) buffer) == -1)
    {
      sys_exit (-1);
    }
  uint32_t len = strlen (buffer);
  if (len == 0)
    {
      return -1;
    }
  return process_execute (buffer);
}

/* closes a file with fd, and kills the process with -1 on invalid file/stdin/stdout */
static void sys_close (int fd)
{
  lock_acquire (&file_lock);
  struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
  if (descriptor == NULL)
    {
      lock_release (&file_lock);
      sys_exit (-1);
    }

  file_close (descriptor->file);
  list_remove (&descriptor->fd_elem);
  lock_release (&file_lock);
}

/* returns the position of next byte to be read in file*/
static unsigned sys_tell (int fd)
{
  struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
  if (descriptor == NULL) return -1;
  lock_acquire (&file_lock);
  unsigned sz = file_tell (descriptor->file);
  lock_release (&file_lock);
  return sz;
}

/* Seeks a specific offset in a file */
static void sys_seek (void *esp)
{
  int fd;
  unsigned pos;
  mem_read ((int *) esp + 1, &fd, sizeof (fd));
  mem_read ((int *) esp + 2, &pos, sizeof (pos));
  struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
  if (descriptor == NULL) return;
  lock_acquire (&file_lock);
  file_seek (descriptor->file, pos);
  lock_release (&file_lock);
}

/* Finds file size given the fd of the open file */
static int filesize (int fd)
{
  struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
  if (descriptor == NULL) return -1;
  lock_acquire (&file_lock);
  int sz = file_length (descriptor->file);
  lock_release (&file_lock);
  return sz;
}

/* Opens the file whose name is stored in *esp + 1 after validating it */
static int open_file (void *esp)
{
  const char *buffer;
  mem_read ((int *) esp + 1, &buffer, sizeof (buffer));
  if (buffer == NULL || get_user ((uint8_t *) buffer) == -1)
    {
      sys_exit (-1);
    }
  uint32_t len = strlen (buffer);
  if (len == 0 || len >= 64)
    {
      return -1;
    }
  lock_acquire (&file_lock);
  struct file *file = filesys_open (buffer);
  if (file == NULL)
    {
      lock_release (&file_lock);
      return -1;
    }
  struct file_descriptor *file_desc = (struct file_descriptor *) malloc (sizeof (struct file_descriptor *));
  file_desc->file = file;
  add_file_desc_to_list (file_desc);
  lock_release (&file_lock);
  return file_desc->fd;
}
static void add_file_desc_to_list (struct file_descriptor *file_desc)
{
  struct thread *t = thread_current ();
  int len = (int) list_size (&t->files_owned);
  if (len == 0)
    {
      file_desc->fd = 2;
    }
  else
    file_desc->fd = list_entry(list_back (&t->files_owned), struct file_descriptor, fd_elem)->fd + 1;
  list_push_back (&t->files_owned, &(file_desc->fd_elem));
}
/* Creates the file whose name is stored in *esp + 1 after validating it */
static bool create_file (void *esp)
{
  const char *buffer;
  mem_read ((int *) esp + 1, &buffer, sizeof (buffer));
  int32_t size = (*((uintptr_t *) esp + 2));
  if (buffer == NULL || get_user ((uint8_t *) buffer) == -1)
    {
      sys_exit (-1);
    }
  uint32_t len = strlen (buffer);
  if (len == 0 || len >= 64)
    {
      return false;
    }
  lock_acquire (&file_lock);
  bool result = filesys_create (buffer, size);
  lock_release (&file_lock);
  return result;
}

/* Reads from the file whose fd is given, transfers SIZE bytes to buffer
 * where buffer and SIZE are args given in esp
 * returns size of bytes read
 */
uint32_t read_call (void *esp)
{
  int fd;
  void *buffer;
  unsigned size;
  mem_read ((int *) esp + 1, &fd, sizeof (fd));
  mem_read ((int *) esp + 2, &buffer, sizeof (buffer));
  mem_read ((int *) esp + 3, &size, sizeof (size));
  if (buffer == NULL || get_user ((uint8_t *) buffer) == -1)
    {
      sys_exit (-1);
    }
  if (fd == 0)
    {
      lock_acquire (&file_lock);
      for (int i = 0; i < size; i++)
        {
          if (!put_user (buffer, input_getc ()))
            {
              lock_release (&file_lock);
              sys_exit (-1);
            }
        }
      lock_release (&file_lock);
      return size;
    }
  else
    {
      lock_acquire (&file_lock);
      struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
      if (descriptor == NULL)
        {
          lock_release (&file_lock);
          return -1;
        }
      int res = file_read (descriptor->file, buffer, size);
      lock_release (&file_lock);
      return res;
    }
}

/* Writes to the file whose fd is given, transfers SIZE bytes from buffer
 * where buffer and SIZE are args given in esp
 * returns size of bytes written
 */
uint32_t write_call (void *esp)
{
  int fd;
  void *buffer;
  unsigned size;
  mem_read ((int *) esp + 1, &fd, sizeof (fd));
  mem_read ((int *) esp + 2, &buffer, sizeof (buffer));
  mem_read ((int *) esp + 3, &size, sizeof (size));
  if (buffer == NULL || get_user ((uint8_t *) buffer) == -1)
    {
      sys_exit (-1);
    }
  // printf("Size is: %d, dump location is: %x\n", size, (int *) esp + 1);
  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }
  else
    {
      lock_acquire (&file_lock);
      struct file_descriptor *descriptor = find_descriptor_by_fd (fd);
      if (descriptor == NULL)
        {
          lock_release (&file_lock);
          return 0;
        }
      int res = file_write (descriptor->file, buffer, size);
      lock_release (&file_lock);
      return res;
    }
}

void sys_exit (int status)
{
  struct thread *curr = thread_current ();
  curr->process_control->exit_code = status;
  sema_up(&curr->process_control->parent_waiting_sema);
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

int sys_wait (tid_t pid)
{
    return process_wait(pid);
}

/* Reads a byte at user virtual address UADDR.UADDR must be below PHYS_BASE.
 * Returns the byte value if successful, -1 if a segfault occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if (((void *) uaddr) >= PHYS_BASE)
    return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:": "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.UDST must be below PHYS_BASE.
 * Returns true if successful, false if a segfault occurred. */
bool put_user (uint8_t *udst, uint8_t byte)
{
  if (((void *) udst) > PHYS_BASE)
    return 0;
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:": "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* reads byte array from memory using get_user() */
int mem_read (void *src, void *dist, int size)
{
  //printf("Size is: %d", size);
  for (int i = 0; i < size; i++)
    {
      int value = get_user ((uint8_t *) src + i);
      if (value == -1)
        {
          sys_exit (-1);
        }
      *(int8_t *) (dist + i) = value & (0xff);
    }
  return size;
}

/* Given an fd, finds the file descriptor of the given fd */
struct file_descriptor *find_descriptor_by_fd (int fd)
{
  struct file_descriptor *descriptor = NULL;
  for (struct list_elem *iter = list_begin (&thread_current ()->files_owned);
       iter != list_end (&thread_current ()->files_owned); iter = list_next (iter))
    {
      if (list_entry(iter, struct file_descriptor, fd_elem)->fd == fd)
        descriptor = list_entry(iter, struct file_descriptor, fd_elem);
    }
  return descriptor;
}
