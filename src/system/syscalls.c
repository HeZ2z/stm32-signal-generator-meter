#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

extern char _end;

/* newlib 通过这组桩函数适配裸机环境；当前项目只保留最小可链接实现。 */
static char *heap_end;

int _close(int file) {
  (void)file;
  return -1;
}

/* 将文件状态伪装成字符设备，满足 libc 对 stdout/stderr 的基本假设。 */
int _fstat(int file, struct stat *st) {
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _getpid(void) {
  return 1;
}

/* 裸机环境下默认把所有文件句柄都视为 tty。 */
int _isatty(int file) {
  (void)file;
  return 1;
}

int _kill(int pid, int sig) {
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

/* 不支持真实文件定位，返回 0 作为占位实现。 */
int _lseek(int file, int ptr, int dir) {
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

/* 当前工程没有通过 libc 路径接收标准输入，直接返回 0。 */
int _read(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  (void)len;
  return 0;
}

/* 为 malloc/new 提供最小堆增长实现，堆起点来自链接脚本中的 _end。 */
void *_sbrk(ptrdiff_t increment) {
  char *previous;

  if (heap_end == 0) {
    heap_end = &_end;
  }

  previous = heap_end;
  heap_end += increment;
  return previous;
}

/* 当前 printf 走项目自定义串口输出，因此 libc 写调用直接吞掉即可。 */
int _write(int file, char *ptr, int len) {
  (void)file;
  (void)ptr;
  return len;
}

/* exit 在裸机上无法返回宿主，直接停机。 */
void _exit(int status) {
  (void)status;
  while (1) {
  }
}
