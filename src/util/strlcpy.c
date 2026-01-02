#include <stddef.h>

/*
 * Standalone strlcpy implementation (BSD semantics).
 * Copies up to (dstsize - 1) bytes, NUL-terminates if dstsize > 0,
 * and returns the length of src.
 */
size_t strlcpy(char *dst, const char *src, size_t dstsize)
{
  const char *s = src;
  size_t nleft = dstsize;

  if (nleft != 0) {
    while (--nleft != 0) {
      if ((*dst++ = *s++) == '\0')
        return (size_t)(s - src - 1);
    }
    *dst = '\0';
  }

  while (*s++)
    ;

  return (size_t)(s - src - 1);
}
