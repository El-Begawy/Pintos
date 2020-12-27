#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <devices/shutdown.h>
#include <filesys/filesys.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static int mem_read (void *src, void *dist, int size);
uint32_t write_call (void *esp);
void sys_exit (int status);
static bool create_file (void *esp);
static int open_file (void *esp);
void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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

          break;
        }
      case SYS_WAIT:
        {

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

          break;
        }
      case SYS_READ:
        {

          break;
        }
      case SYS_WRITE:
        {
          f->eax = write_call (f->esp);
          break;
        }
      case SYS_SEEK:
        {

          break;
        }
      case SYS_TELL:
        {

          break;
        }
      case SYS_CLOSE:
        {

          break;
        }
    }
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
  struct file *file = filesys_open (buffer);
  return (file == NULL ? -1 : 2);
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
  return filesys_create (buffer, size);
}

uint32_t write_call (void *esp)
{
  int fd;
  void *buffer;
  unsigned size;
  mem_read ((int *) esp + 1, &fd, sizeof (fd));
  mem_read ((int *) esp + 2, &buffer, sizeof (buffer));
  mem_read ((int *) esp + 3, &size, sizeof (size));
  // printf("Size is: %d, dump location is: %x\n", size, (int *) esp + 1);
  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }

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
void sys_exit (int status)
{
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}
