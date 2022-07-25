/* Copyright (c) 2014 Balint Reczey <balint@balintreczey.hu> */
/* This file is an unpublished work. All rights reserved. */

#ifndef COMMON_PLATFORM_H_
#define COMMON_PLATFORM_H_

#include <assert.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/kcmp.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif
#include <stdlib.h>
#include <unistd.h>


#ifdef __has_include
#if __has_include(<linux/close_range.h>)
#include <linux/close_range.h>
#endif
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC (1U << 2)
#endif


inline bool path_is_absolute(const char * p) {
#ifdef _WIN32
  return !PathIsRelative(p);
#else
  if (p[0] == '/') {
    return true;
  } else {
    return false;
  }
#endif
}

/**
 * Check if fd1 and fd2 point to the same place.
 * kcmp() is not universally available, so in its absence do a back-n-forth fcntl() on one and see
 * if it drags the other with it.
 * See https://unix.stackexchange.com/questions/191967.
 * @return 0 if they point to the same place, -1 or 1 if fd1 sorts lower or higher than fd2 in an
 * arbitrary ordering to help using fdcmp for sorting
 */
inline int fdcmp(int fd1, int fd2) {
#ifdef __linux__
  pid_t pid = getpid();
  switch (syscall(SYS_kcmp, pid, pid, KCMP_FILE, fd1, fd2)) {
    case 0: return 0;
    case 1: return -1;
    case 2: return 1;
    case 3: return fd1 < fd2 ? -1 : 1;
    case -1: {
#endif
      /* TODO(rbalint) this may not be safe for shim, but I have no better idea */
      int flags1 = fcntl(fd1, F_GETFL);
      int flags2a = fcntl(fd2, F_GETFL);
      fcntl(fd1, F_SETFL, flags1 ^ O_NONBLOCK);
      int flags2b = fcntl(fd2, F_GETFL);
      fcntl(fd1, F_SETFL, flags1);
      return (flags2a != flags2b) ? 0 : (fd1 < fd2 ? -1 : 1);
#ifdef __linux__
    }
    default:
      assert(0 && "not reached");
      abort();
  }
#endif
}

/** Makes a directory hierarchy, like the mkdirhier(1) command */
static inline int mkdirhier(const char *pathname, const mode_t mode) {
  if (mkdir(pathname, mode) == 0) {
    return 0;
  } else {
    switch (errno) {
      case EEXIST:
        return 0;
      case ENOENT: {
        const char *last_slash = strrchr(pathname, '/');
        if (last_slash) {
          ssize_t len = last_slash - pathname;
          char* parent = (char*)alloca(len + 1);
          memcpy(parent, pathname, len);
          parent[len] = '\0';
          if (mkdirhier(parent, mode) == 0) {
            return mkdir(pathname, mode);
          } else {
            return -1;
          }
        } else {
          return -1;
        }
      }
      default:
        return -1;
    }
  }
}

#endif  // COMMON_PLATFORM_H_
