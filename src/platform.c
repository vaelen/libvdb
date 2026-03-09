/*
 * VDB Platform Abstractions
 *
 * File truncation and epoch conversion for portability
 * across POSIX, DOS, and Classic Mac OS.
 */

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  #define _POSIX_C_SOURCE 200112L
#endif

#include "internal.h"

/* File truncation */

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__) || \
    defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
  /* POSIX */
  #include <sys/types.h>
  #include <unistd.h>
  int ftruncate_(FILE *fp, long size) {
      int fd;
      fflush(fp);
      fd = fileno(fp);
      return (ftruncate(fd, (off_t)size) == 0) ? 1 : 0;
  }

#elif defined(__MSDOS__) || defined(__TURBOC__) || defined(__BORLANDC__) || \
      defined(__WATCOMC__) || defined(_DOS)
  /* DOS */
  #include <io.h>
  int ftruncate_(FILE *fp, long size) {
      int fd;
      fflush(fp);
      fd = fileno(fp);
      return (chsize(fd, size) == 0) ? 1 : 0;
  }

#elif defined(THINK_C) || defined(__MWERKS__)
  /* Classic Mac OS */
  #include <Files.h>
  int ftruncate_(FILE *fp, long size) {
      short refNum;
      fflush(fp);
      refNum = (short)fileno(fp);
      return (SetEOF(refNum, size) == noErr) ? 1 : 0;
  }

#else
  /* Fallback: close, truncate via rewrite, reopen */
  int ftruncate_(FILE *fp, long size) {
      /* Best-effort: seek and write nothing past the desired end. */
      /* This is a no-op fallback for unknown platforms. */
      (void)fp;
      (void)size;
      return 0;
  }
#endif

/* Epoch conversion */

int32 UnixToMacEpoch(int32 unix_time) {
    return unix_time + (int32)EPOCH_DELTA;
}

int32 MacToUnixEpoch(int32 mac_time) {
    return mac_time - (int32)EPOCH_DELTA;
}
