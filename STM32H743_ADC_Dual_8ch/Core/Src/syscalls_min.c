/**
 * @file    syscalls_min.c
 * @brief   Makefile 빌드용 최소 newlib 시스템 콜(printf → __io_putchar).
 *          CubeIDE 프로젝트에는 syscalls.c/sysmem.c가 이미 있으므로 이 파일은 넣지 마세요.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

extern int __io_putchar(int ch);

int _write(int file, char *ptr, int len)
{
  (void)file;
  for (int i = 0; i < len; i++)
  {
    __io_putchar(ptr[i]);
  }
  return len;
}

int _read(int file, char *ptr, int len)   { (void)file; (void)ptr; (void)len; return 0; }
int _close(int file)                      { (void)file; return -1; }
int _fstat(int file, struct stat *st)     { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file)                     { (void)file; return 1; }
int _lseek(int file, int ptr, int dir)    { (void)file; (void)ptr; (void)dir; return 0; }
int _getpid(void)                         { return 1; }
int _kill(int pid, int sig)               { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                    { (void)status; while (1) {} }

/* 힙: 링커 스크립트의 _end ~ _estack - _Min_Stack_Size */
extern uint8_t _end;
extern uint8_t _estack;
extern uint32_t _Min_Stack_Size;

void *_sbrk(ptrdiff_t incr)
{
  static uint8_t *heap_end = NULL;
  const uint8_t *limit = &_estack - (uint32_t)&_Min_Stack_Size;
  uint8_t *prev;

  if (heap_end == NULL)
  {
    heap_end = &_end;
  }
  if (heap_end + incr > limit)
  {
    errno = ENOMEM;
    return (void *)-1;
  }
  prev = heap_end;
  heap_end += incr;
  return (void *)prev;
}
