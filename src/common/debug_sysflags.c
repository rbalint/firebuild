/* Copyright (c) 2022 Interri Kft. */
/* This file is an unpublished work. All rights reserved. */

#include "common/debug_sysflags.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convenience macros to debug-print a bitfield. */

#define DEBUG_BITMAP_START(f, var) \
  { \
    const char *sep = "";

#define DEBUG_BITMAP_FLAG(f, var, flag) \
    if (var & flag) { \
      fprintf(f, "%s%s", sep, #flag); \
      var &= ~flag; \
      sep = "|"; \
    }

#define DEBUG_BITMAP_END_OCT(f, flags) \
    if (flags) { \
      /* Remaining unrecognized flags */ \
      fprintf(f, "%s0%o", sep, flags); \
    } \
  }

#define DEBUG_BITMAP_END_HEX(f, flags) \
    if (flags) { \
      /* Remaining unrecognized flags */ \
      fprintf(f, "%s0x%X", sep, flags); \
    } \
  }

/* Convenience macros to debug-print a variable that is supposed to have one of several values. */

#define DEBUG_VALUE_START(f, var) \
  switch (var) { \
    /* NOLINT(whitespace/blank_line) */

#define DEBUG_VALUE_VALUE(f, var, value) \
    case value: \
      fprintf(f, #value); \
      break;

#define DEBUG_VALUE_END_OCT(f, var) \
    default: \
      fprintf(f, "0%o", var); \
  }

#define DEBUG_VALUE_END_DEC(f, var) \
    default: \
      fprintf(f, "%d", var); \
  }

#define DEBUG_VALUE_END_HEX(f, var) \
    default: \
      fprintf(f, "0x%X", var); \
  }

/**
 * Debug-print O_* flags, as usually seen in the 'flags' parameter of dup3(), open(), pipe2(),
 * posix_spawn_file_actions_addopen() etc. calls.
 */
void debug_open_flags(FILE *f, int flags) {
  DEBUG_BITMAP_START(f, flags)

  int accmode = flags & O_ACCMODE;
  DEBUG_VALUE_START(f, accmode)
  DEBUG_VALUE_VALUE(f, accmode, O_RDONLY);
  DEBUG_VALUE_VALUE(f, accmode, O_WRONLY);
  DEBUG_VALUE_VALUE(f, accmode, O_RDWR);
  DEBUG_VALUE_END_OCT(f, accmode)

  flags &= ~O_ACCMODE;
  sep = "|";

  DEBUG_BITMAP_FLAG(f, flags, O_APPEND)
  DEBUG_BITMAP_FLAG(f, flags, O_ASYNC)
  DEBUG_BITMAP_FLAG(f, flags, O_CLOEXEC)
  DEBUG_BITMAP_FLAG(f, flags, O_CREAT)
  DEBUG_BITMAP_FLAG(f, flags, O_DIRECT)
  DEBUG_BITMAP_FLAG(f, flags, O_DIRECTORY)
  DEBUG_BITMAP_FLAG(f, flags, O_DSYNC)
  DEBUG_BITMAP_FLAG(f, flags, O_EXCL)
  DEBUG_BITMAP_FLAG(f, flags, O_LARGEFILE)
  DEBUG_BITMAP_FLAG(f, flags, O_NOATIME)
  DEBUG_BITMAP_FLAG(f, flags, O_NOCTTY)
  DEBUG_BITMAP_FLAG(f, flags, O_NOFOLLOW)
  DEBUG_BITMAP_FLAG(f, flags, O_NONBLOCK)
  DEBUG_BITMAP_FLAG(f, flags, O_PATH)
  DEBUG_BITMAP_FLAG(f, flags, O_SYNC)
  DEBUG_BITMAP_FLAG(f, flags, O_TMPFILE)
  DEBUG_BITMAP_FLAG(f, flags, O_TRUNC)
  DEBUG_BITMAP_END_HEX(f, flags)
}

/**
 * Debug-print AT_* flags, as usually seen in the 'flags' parameter of execveat(), faccessat(),
 * fchmodat(), fchownat(), fstatat(), linkat(), statx(), unlinkat(), utimensat() etc. calls.
 */
void debug_at_flags(FILE *f, int flags) {
  DEBUG_BITMAP_START(f, flags)
  DEBUG_BITMAP_FLAG(f, flags, AT_EMPTY_PATH)
  DEBUG_BITMAP_FLAG(f, flags, AT_NO_AUTOMOUNT)
  DEBUG_BITMAP_FLAG(f, flags, AT_RECURSIVE)
  DEBUG_BITMAP_FLAG(f, flags, AT_REMOVEDIR)
  DEBUG_BITMAP_FLAG(f, flags, AT_STATX_DONT_SYNC)
  DEBUG_BITMAP_FLAG(f, flags, AT_STATX_FORCE_SYNC)
  DEBUG_BITMAP_FLAG(f, flags, AT_STATX_SYNC_AS_STAT)
  DEBUG_BITMAP_FLAG(f, flags, AT_STATX_SYNC_TYPE)
  DEBUG_BITMAP_FLAG(f, flags, AT_SYMLINK_FOLLOW)
  DEBUG_BITMAP_FLAG(f, flags, AT_SYMLINK_NOFOLLOW)
  DEBUG_BITMAP_END_HEX(f, flags)
}

/**
 * Debug-print the 'cmd' parameter of an fcntl() call.
 */
void debug_fcntl_cmd(FILE *f, int cmd) {
  DEBUG_VALUE_START(f, cmd)
  DEBUG_VALUE_VALUE(f, cmd, F_DUPFD)
  DEBUG_VALUE_VALUE(f, cmd, F_DUPFD_CLOEXEC)
  DEBUG_VALUE_VALUE(f, cmd, F_GETFD)
  DEBUG_VALUE_VALUE(f, cmd, F_SETFD)
  DEBUG_VALUE_VALUE(f, cmd, F_GETFL)
  DEBUG_VALUE_VALUE(f, cmd, F_SETFL)
  DEBUG_VALUE_VALUE(f, cmd, F_GETLK)
  DEBUG_VALUE_VALUE(f, cmd, F_SETLK)
  DEBUG_VALUE_VALUE(f, cmd, F_SETLKW)
  DEBUG_VALUE_VALUE(f, cmd, F_GETOWN)
  DEBUG_VALUE_VALUE(f, cmd, F_SETOWN)
  DEBUG_VALUE_VALUE(f, cmd, F_GETOWN_EX)
  DEBUG_VALUE_VALUE(f, cmd, F_SETOWN_EX)
  DEBUG_VALUE_VALUE(f, cmd, F_GETSIG)
  DEBUG_VALUE_VALUE(f, cmd, F_SETSIG)
  DEBUG_VALUE_VALUE(f, cmd, F_GETLEASE)
  DEBUG_VALUE_VALUE(f, cmd, F_SETLEASE)
  DEBUG_VALUE_VALUE(f, cmd, F_NOTIFY)
  DEBUG_VALUE_VALUE(f, cmd, F_GETPIPE_SZ)
  DEBUG_VALUE_VALUE(f, cmd, F_SETPIPE_SZ)
  DEBUG_VALUE_VALUE(f, cmd, F_ADD_SEALS)
  DEBUG_VALUE_VALUE(f, cmd, F_GET_SEALS)
  DEBUG_VALUE_VALUE(f, cmd, F_GET_RW_HINT)
  DEBUG_VALUE_VALUE(f, cmd, F_SET_RW_HINT)
  DEBUG_VALUE_VALUE(f, cmd, F_GET_FILE_RW_HINT)
  DEBUG_VALUE_VALUE(f, cmd, F_SET_FILE_RW_HINT)
  DEBUG_VALUE_END_DEC(f, cmd)
}

/**
 * Debug-print fcntl()'s 'arg'parameter or return value. The debugging format depends on 'cmd'.
 */
void debug_fcntl_arg_or_ret(FILE *f, int cmd, int arg_or_ret) {
  switch (cmd) {
    case F_GETFD:
    case F_SETFD:
      if (arg_or_ret) {
        DEBUG_BITMAP_START(f, arg_or_ret)
        DEBUG_BITMAP_FLAG(f, arg_or_ret, FD_CLOEXEC)
        DEBUG_BITMAP_END_HEX(f, arg_or_ret)
      } else {
        fprintf(f, "0");
      }
      break;
    case F_GETFL:
    case F_SETFL:
      debug_open_flags(f, arg_or_ret);
      break;
    default:
      fprintf(f, "%d", arg_or_ret);
  }
}

/**
 * Debug-print an error number.
 */
void debug_error_no(FILE *f, int error_no) {
  // FIXME: glibc 2.32 adds strerrorname_np(), switch to that one day.
  DEBUG_VALUE_START(f, error_no)
  DEBUG_VALUE_VALUE(f, error_no, E2BIG)
  DEBUG_VALUE_VALUE(f, error_no, EACCES)
  DEBUG_VALUE_VALUE(f, error_no, EADDRINUSE)
  DEBUG_VALUE_VALUE(f, error_no, EADDRNOTAVAIL)
  DEBUG_VALUE_VALUE(f, error_no, EADV)
  DEBUG_VALUE_VALUE(f, error_no, EAFNOSUPPORT)
  DEBUG_VALUE_VALUE(f, error_no, EAGAIN)
  DEBUG_VALUE_VALUE(f, error_no, EALREADY)
  DEBUG_VALUE_VALUE(f, error_no, EBADE)
  DEBUG_VALUE_VALUE(f, error_no, EBADF)
  DEBUG_VALUE_VALUE(f, error_no, EBADFD)
  DEBUG_VALUE_VALUE(f, error_no, EBADMSG)
  DEBUG_VALUE_VALUE(f, error_no, EBADR)
  DEBUG_VALUE_VALUE(f, error_no, EBADRQC)
  DEBUG_VALUE_VALUE(f, error_no, EBADSLT)
  DEBUG_VALUE_VALUE(f, error_no, EBFONT)
  DEBUG_VALUE_VALUE(f, error_no, EBUSY)
  DEBUG_VALUE_VALUE(f, error_no, ECANCELED)
  DEBUG_VALUE_VALUE(f, error_no, ECHILD)
  DEBUG_VALUE_VALUE(f, error_no, ECHRNG)
  DEBUG_VALUE_VALUE(f, error_no, ECOMM)
  DEBUG_VALUE_VALUE(f, error_no, ECONNABORTED)
  DEBUG_VALUE_VALUE(f, error_no, ECONNREFUSED)
  DEBUG_VALUE_VALUE(f, error_no, ECONNRESET)
  DEBUG_VALUE_VALUE(f, error_no, EDEADLK)
  /* DEBUG_VALUE_VALUE(f, error_no, EDEADLOCK) - same as EDEADLK on Linux */
  DEBUG_VALUE_VALUE(f, error_no, EDESTADDRREQ)
  DEBUG_VALUE_VALUE(f, error_no, EDOM)
  DEBUG_VALUE_VALUE(f, error_no, EDOTDOT)
  DEBUG_VALUE_VALUE(f, error_no, EDQUOT)
  DEBUG_VALUE_VALUE(f, error_no, EEXIST)
  DEBUG_VALUE_VALUE(f, error_no, EFAULT)
  DEBUG_VALUE_VALUE(f, error_no, EFBIG)
  DEBUG_VALUE_VALUE(f, error_no, EHOSTDOWN)
  DEBUG_VALUE_VALUE(f, error_no, EHOSTUNREACH)
  DEBUG_VALUE_VALUE(f, error_no, EHWPOISON)
  DEBUG_VALUE_VALUE(f, error_no, EIDRM)
  DEBUG_VALUE_VALUE(f, error_no, EILSEQ)
  DEBUG_VALUE_VALUE(f, error_no, EINPROGRESS)
  DEBUG_VALUE_VALUE(f, error_no, EINTR)
  DEBUG_VALUE_VALUE(f, error_no, EINVAL)
  DEBUG_VALUE_VALUE(f, error_no, EIO)
  DEBUG_VALUE_VALUE(f, error_no, EISCONN)
  DEBUG_VALUE_VALUE(f, error_no, EISDIR)
  DEBUG_VALUE_VALUE(f, error_no, EISNAM)
  DEBUG_VALUE_VALUE(f, error_no, EKEYEXPIRED)
  DEBUG_VALUE_VALUE(f, error_no, EKEYREJECTED)
  DEBUG_VALUE_VALUE(f, error_no, EKEYREVOKED)
  DEBUG_VALUE_VALUE(f, error_no, EL2HLT)
  DEBUG_VALUE_VALUE(f, error_no, EL2NSYNC)
  DEBUG_VALUE_VALUE(f, error_no, EL3HLT)
  DEBUG_VALUE_VALUE(f, error_no, EL3RST)
  DEBUG_VALUE_VALUE(f, error_no, ELIBACC)
  DEBUG_VALUE_VALUE(f, error_no, ELIBBAD)
  DEBUG_VALUE_VALUE(f, error_no, ELIBEXEC)
  DEBUG_VALUE_VALUE(f, error_no, ELIBMAX)
  DEBUG_VALUE_VALUE(f, error_no, ELIBSCN)
  DEBUG_VALUE_VALUE(f, error_no, ELNRNG)
  DEBUG_VALUE_VALUE(f, error_no, ELOOP)
  DEBUG_VALUE_VALUE(f, error_no, EMEDIUMTYPE)
  DEBUG_VALUE_VALUE(f, error_no, EMFILE)
  DEBUG_VALUE_VALUE(f, error_no, EMLINK)
  DEBUG_VALUE_VALUE(f, error_no, EMSGSIZE)
  DEBUG_VALUE_VALUE(f, error_no, EMULTIHOP)
  DEBUG_VALUE_VALUE(f, error_no, ENAMETOOLONG)
  DEBUG_VALUE_VALUE(f, error_no, ENAVAIL)
  DEBUG_VALUE_VALUE(f, error_no, ENETDOWN)
  DEBUG_VALUE_VALUE(f, error_no, ENETRESET)
  DEBUG_VALUE_VALUE(f, error_no, ENETUNREACH)
  DEBUG_VALUE_VALUE(f, error_no, ENFILE)
  DEBUG_VALUE_VALUE(f, error_no, ENOANO)
  DEBUG_VALUE_VALUE(f, error_no, ENOBUFS)
  DEBUG_VALUE_VALUE(f, error_no, ENOCSI)
  DEBUG_VALUE_VALUE(f, error_no, ENODATA)
  DEBUG_VALUE_VALUE(f, error_no, ENODEV)
  DEBUG_VALUE_VALUE(f, error_no, ENOENT)
  DEBUG_VALUE_VALUE(f, error_no, ENOEXEC)
  DEBUG_VALUE_VALUE(f, error_no, ENOKEY)
  DEBUG_VALUE_VALUE(f, error_no, ENOLCK)
  DEBUG_VALUE_VALUE(f, error_no, ENOLINK)
  DEBUG_VALUE_VALUE(f, error_no, ENOMEDIUM)
  DEBUG_VALUE_VALUE(f, error_no, ENOMEM)
  DEBUG_VALUE_VALUE(f, error_no, ENOMSG)
  DEBUG_VALUE_VALUE(f, error_no, ENONET)
  DEBUG_VALUE_VALUE(f, error_no, ENOPKG)
  DEBUG_VALUE_VALUE(f, error_no, ENOPROTOOPT)
  DEBUG_VALUE_VALUE(f, error_no, ENOSPC)
  DEBUG_VALUE_VALUE(f, error_no, ENOSR)
  DEBUG_VALUE_VALUE(f, error_no, ENOSTR)
  DEBUG_VALUE_VALUE(f, error_no, ENOSYS)
  DEBUG_VALUE_VALUE(f, error_no, ENOTBLK)
  DEBUG_VALUE_VALUE(f, error_no, ENOTCONN)
  DEBUG_VALUE_VALUE(f, error_no, ENOTDIR)
  DEBUG_VALUE_VALUE(f, error_no, ENOTEMPTY)
  DEBUG_VALUE_VALUE(f, error_no, ENOTNAM)
  DEBUG_VALUE_VALUE(f, error_no, ENOTRECOVERABLE)
  DEBUG_VALUE_VALUE(f, error_no, ENOTSOCK)
  DEBUG_VALUE_VALUE(f, error_no, ENOTSUP)
  DEBUG_VALUE_VALUE(f, error_no, ENOTTY)
  DEBUG_VALUE_VALUE(f, error_no, ENOTUNIQ)
  DEBUG_VALUE_VALUE(f, error_no, ENXIO)
  /* DEBUG_VALUE_VALUE(f, error_no, EOPNOTSUPP) - same as ENOTSUPP on Linux */
  DEBUG_VALUE_VALUE(f, error_no, EOVERFLOW)
  DEBUG_VALUE_VALUE(f, error_no, EOWNERDEAD)
  DEBUG_VALUE_VALUE(f, error_no, EPERM)
  DEBUG_VALUE_VALUE(f, error_no, EPFNOSUPPORT)
  DEBUG_VALUE_VALUE(f, error_no, EPIPE)
  DEBUG_VALUE_VALUE(f, error_no, EPROTO)
  DEBUG_VALUE_VALUE(f, error_no, EPROTONOSUPPORT)
  DEBUG_VALUE_VALUE(f, error_no, EPROTOTYPE)
  DEBUG_VALUE_VALUE(f, error_no, ERANGE)
  DEBUG_VALUE_VALUE(f, error_no, EREMCHG)
  DEBUG_VALUE_VALUE(f, error_no, EREMOTE)
  DEBUG_VALUE_VALUE(f, error_no, EREMOTEIO)
  DEBUG_VALUE_VALUE(f, error_no, ERESTART)
  DEBUG_VALUE_VALUE(f, error_no, ERFKILL)
  DEBUG_VALUE_VALUE(f, error_no, EROFS)
  DEBUG_VALUE_VALUE(f, error_no, ESHUTDOWN)
  DEBUG_VALUE_VALUE(f, error_no, ESOCKTNOSUPPORT)
  DEBUG_VALUE_VALUE(f, error_no, ESPIPE)
  DEBUG_VALUE_VALUE(f, error_no, ESRCH)
  DEBUG_VALUE_VALUE(f, error_no, ESRMNT)
  DEBUG_VALUE_VALUE(f, error_no, ESTALE)
  DEBUG_VALUE_VALUE(f, error_no, ESTRPIPE)
  DEBUG_VALUE_VALUE(f, error_no, ETIME)
  DEBUG_VALUE_VALUE(f, error_no, ETIMEDOUT)
  DEBUG_VALUE_VALUE(f, error_no, ETOOMANYREFS)
  DEBUG_VALUE_VALUE(f, error_no, ETXTBSY)
  DEBUG_VALUE_VALUE(f, error_no, EUCLEAN)
  DEBUG_VALUE_VALUE(f, error_no, EUNATCH)
  DEBUG_VALUE_VALUE(f, error_no, EUSERS)
  /* DEBUG_VALUE_VALUE(f, error_no, EWOULDBLOCK) - same as EAGAIN on Linux */
  DEBUG_VALUE_VALUE(f, error_no, EXDEV)
  DEBUG_VALUE_VALUE(f, error_no, EXFULL)
  DEBUG_VALUE_END_DEC(f, error_no)
}

/**
 * Debug-print a mode_t variable.
 *
 * mode_t sometimes contains the file type (e.g. when returned by a stat() call) and sometimes
 * doesn't (e.g. when it's a parameter to an open(), chmod(), umask() call).
 *
 * Luckily, at least on Linux, none of the S_IF* constants are defined as 0. This means that we can
 * determine which category we fall into and we can produce nice debug output in both cases, without
 * having to maintain two separate functions.
 */
void debug_mode_t(FILE *f, mode_t mode) {
  const char *sep = "|";

  mode_t type = mode & S_IFMT;
  DEBUG_VALUE_START(f, type)
  DEBUG_VALUE_VALUE(f, type, S_IFREG)
  DEBUG_VALUE_VALUE(f, type, S_IFDIR)
  DEBUG_VALUE_VALUE(f, type, S_IFLNK)
  DEBUG_VALUE_VALUE(f, type, S_IFBLK)
  DEBUG_VALUE_VALUE(f, type, S_IFCHR)
  DEBUG_VALUE_VALUE(f, type, S_IFIFO)
  DEBUG_VALUE_VALUE(f, type, S_IFSOCK)
  case 0:
    /* File type info is not available. Don't print anything here. */
    sep = "";
    break;
  DEBUG_VALUE_END_OCT(f, type)

  mode &= ~S_IFMT;
  fprintf(f, "%s0%03o", sep, mode);
}

/*
 * Debug-print a "wait status", as usually seen in the non-error return value of system() and
 * pclose(), and in the "wstatus" out parameter of the wait*() family.
 */
void debug_wstatus(FILE *f, int wstatus) {
  const char *sep = "";
  fprintf(f, "%d (", wstatus);
  if (WIFEXITED(wstatus)) {
    fprintf(f, "%sexitstatus=%d", sep, WEXITSTATUS(wstatus));
    sep = ", ";
  }
  if (WIFSIGNALED(wstatus)) {
    fprintf(f, "%stermsig=%d", sep, WTERMSIG(wstatus));
    if (WCOREDUMP(wstatus)) {
      fprintf(f, ", coredump");
    }
    sep = ", ";
  }
  if (WIFSTOPPED(wstatus)) {
    fprintf(f, "%sstopsig=%d", sep, WSTOPSIG(wstatus));
    sep = ", ";
  }
  if (WIFCONTINUED(wstatus)) {
    fprintf(f, "%scontinued", sep);
    sep = ", ";
  }
  fprintf(f, ")");
}

#ifdef __cplusplus
}  /* extern "C" */
#endif