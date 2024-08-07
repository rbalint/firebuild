/*
 * Copyright (c) 2022 Firebuild Inc.
 * All rights reserved.
 *
 * Free for personal use and commercial trial.
 * Non-trial commercial use requires licenses available from https://firebuild.com.
 * Modification and redistribution are permitted, but commercial use of derivative
 * works is subject to the same requirements of this license
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef COMMON_PLATFORM_H_
#define COMMON_PLATFORM_H_

#include <assert.h>
#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#endif
#include <errno.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/kcmp.h>
#endif
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Convenience macro because #if defined (__GLIBC_PREREQ) && __GLIBC_PREREQ (x, y) does not work. */
#ifdef __GLIBC_PREREQ
#define FB_GLIBC_PREREQ __GLIBC_PREREQ
#else
#define FB_GLIBC_PREREQ(major, minor) 0
#endif

#ifdef __has_include
#if __has_include(<linux/close_range.h>)
#include <linux/close_range.h>
#endif
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC (1U << 2)
#endif

#ifndef CLONE_PIDFD
#define CLONE_PIDFD 0x00001000
#endif

#ifndef __linux__
#define loff_t off_t
#endif

#ifdef __APPLE__
#define off64_t off_t
#endif

#ifdef __linux__
#ifndef STATX_TYPE
#define STATX_TYPE 0x0001U
#define STATX_MODE 0x0002U
#define STATX_SIZE 0x0200U

struct statx_timestamp {
  __s64 tv_sec;
  __u32 tv_nsec;
};

struct statx {
  __u32 stx_mask;
  __u32 stx_blksize;
  __u64 stx_attributes;
  __u32 stx_nlink;
  __u32 stx_uid;
  __u32 stx_gid;
  __u16 stx_mode;
  __u64 stx_ino;
  __u64 stx_size;
  __u64 stx_blocks;
  __u64 stx_attributes_mask;
  struct statx_timestamp stx_atime;
  struct statx_timestamp stx_btime;
  struct statx_timestamp stx_ctime;
  struct statx_timestamp stx_mtime;
  __u32 stx_rdev_major;
  __u32 stx_rdev_minor;
  __u32 stx_dev_major;
  __u32 stx_dev_minor;
};
#endif  // STATX_TYPE
#endif  // __linux__

#ifdef __APPLE__
#define stat64 stat
#define lstat64 lstat
#define fstat64 fstat
#define st_mtim st_mtimespec
#endif

#ifdef __APPLE__
#define PRImode "uh"
#else
#define PRImode "u"
#endif

#if (defined(SIZE_WIDTH) && (SIZE_WIDTH == 64)) \
  || (defined(__SIZE_WIDTH__) && (__SIZE_WIDTH__ == 64))
#define PRIsize "lu"
#define PRIssize "ld"
#else
#define PRIsize "u"
#define PRIssize "d"
#endif

#if defined(__APPLE__) || (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64) && \
                           (__BITS_PER_LONG == 32))
#define PRIoff "lld"
#define SCNoff "lld"
#else
#define PRIoff "ld"
#define SCNoff "ld"
#endif

#if __WORDSIZE == 64
#define PRIoff64 "ld"
#else
#define PRIoff64 "lld"
#endif

#ifdef __APPLE__
#define PRIloff "lld"
#else
#define PRIloff PRIoff64
#endif

#if defined(__USE_TIME_BITS64) && defined(__BITS_PER_LONG)
#if __BITS_PER_LONG == 32
#define PRItime "lld"
#else
#define PRItime "ld"
#endif
#else
#define PRItime "ld"
#endif

#ifdef __APPLE__
#define sighandler_t sig_t
#endif

static inline bool path_is_absolute(const char * p) {
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
static inline int fdcmp(int fd1, int fd2) {
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

inline int fb_pipe2(int pipefd[2], int flags) {
#ifdef __APPLE__
  if (pipe(pipefd) == -1) {
    return -1;
  } else if (fcntl(pipefd[0], F_SETFL, flags) == -1 || fcntl(pipefd[1], F_SETFL, flags) == -1) {
    int saved_errno = errno;
    close(pipefd[0]);
    close(pipefd[1]);
    errno = saved_errno;
    return -1;
  } else {
    return 0;
  }
#else
  return pipe2(pipefd, flags);
#endif
}

#endif  // COMMON_PLATFORM_H_
