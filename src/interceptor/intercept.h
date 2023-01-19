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

/*
 * Interceptor library definitions
 */

#ifndef FIREBUILD_INTERCEPT_H_
#define FIREBUILD_INTERCEPT_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <bits/types/FILE.h>
#include <dlfcn.h>
#include <link.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <stdbool.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/firebuild_common.h"
#include "./fbbcomm.h"

/** A poor man's (plain C) implementation of a hashmap:
 *  posix_spawn_file_actions_t -> char**
 *  implemented as a dense array with linear lookup.
 *
 *  Each file action is encoded as a simple string, e.g.
 *  - open:  "o 10 0 0 /etc/hosts"
 *  - close: "c 11"
 *  - dup2:  "d 3 5"
 */
typedef struct {
  const posix_spawn_file_actions_t *p;
  voidp_array actions;
} psfa;
extern psfa *psfas;
extern int psfas_num;
extern int psfas_alloc;

/** This tells whether the supervisor needs to be notified on a read or write
 *  event. The supervisor needs to be notified only on the first of each kind,
 *  and only for file descriptors that were inherited by the process.
 *  The "p" ones are stronger than their "non-p" counterparts, e.g. after notifying
 *  the supervisor about a "pwrite" we don't need to notify it on a "write".
 *  Similarly, "seek" is stronger than "tell", i.e. after a "seek" we don't send a "tell". */
typedef struct {
  /* Whether to notify on a read()-like operation at the current file offset,
   * including preadv2() with offset == -1. */
  bool notify_on_read:1;
  /* Whether to notify on a pread()-like operation that reads at an arbitrary offset,
   * but not preadv2() with offset == -1. */
  bool notify_on_pread:1;
  /* Whether to notify on a write()-like operation at the current file offset,
   * including pwrite2() with offset == -1. */
  bool notify_on_write:1;
  /* Whether to notify on a pwrite()-like operation that writes at an arbitrary offset,
   * but not pwrite2() with offset == -1. */
  bool notify_on_pwrite:1;
  /* Whether to notify on an lseek()-like operation that queries (but does not modify) the
   * offset. */
  bool notify_on_tell:1;
  /* Whether to notify on an lseek()-like operation that modifies (and possibly also queries)
   * the offset. */
  bool notify_on_seek:1;
} fd_state;

typedef struct {
  int (*orig_fn)(void *);
  void *orig_arg;
  bool i_locked;
} clone_trampoline_arg;

/** file fd states */
#define IC_FD_STATES_SIZE 4096
extern fd_state ic_fd_states[];

/** An uint64_t bitmap is used for delayed signals. */
#define IC_WRAP_SIGRTMAX 64

/** Called unknown syscalls */
#define IC_CALLED_SYSCALL_SIZE 1024
extern bool ic_called_syscall[];

/** Resource usage at the process' last exec() */
extern struct rusage initial_rusage;

/** Global lock for preventing parallel system and popen calls */
extern pthread_mutex_t ic_system_popen_lock;

/** buffer size for getcwd */
#define IC_PATH_BUFSIZE 4096

/** Current working directory as reported to the supervisor */
extern char ic_cwd[IC_PATH_BUFSIZE];
extern size_t ic_cwd_len;

/** Reset globally maintained information about intercepted functions */
extern void reset_fn_infos();

/** Connect to supervisor */
extern int fb_connect_supervisor();

/** Set up main supervisor connection */
extern void fb_init_supervisor_conn();

/** Global lock for serializing critical interceptor actions */
extern pthread_mutex_t ic_global_lock;

/** Send message, delaying all signals in the current thread.
 *  The caller has to take care of thread locking. */
void fb_fbbcomm_send_msg(const void /*FBBCOMM_Builder*/ *ic_msg, int fd);

/** Send delaying all signals in the current thread, returning the ACK number sent.
 *  The caller has to take care of thread locking. */
uint16_t fb_fbbcomm_send_msg_with_ack(const void /*FBBCOMM_Builder*/ *ic_msg, int fd);

/** Wait for ACK, processing delayed signals in the current thread.
 *  The caller has to take care of thread locking. */
void fb_fbbcomm_check_ack(int fd, uint16_t ack_num);

/** Send message and wait for ACK, delaying all signals in the current thread.
 *  The caller has to take care of thread locking. */
void fb_fbbcomm_send_msg_and_check_ack(const void /*FBBCOMM_Builder*/ *ic_msg, int fd);

/**
 * Send pre_open message to supervisor if it is needed.
 * @return if message has been sent
 */
void send_pre_open(int dirfd, const char* pathname);
void send_pre_open_without_ack_request(int dirfd, const char* pathname);
bool maybe_send_pre_open(int dirfd, const char* pathname, int flags);

/** Send message and disable interception before clone() */
void pre_clone_disable_interception(const int flags, bool *i_locked);

/** Function to call by the intercepted clone(), registering into the supervisor then
 calling the original clone() fn param. */
int clone_trampoline(void *arg);

/** Connection string to supervisor */
extern char fb_conn_string[IC_PATH_BUFSIZE];

/** Connection string length */
extern size_t fb_conn_string_len;

/** Connection file descriptor to supervisor */
extern int fb_sv_conn;

/** pthread_sigmask() if available (libpthread is loaded), otherwise sigprocmask() */
extern int (*ic_pthread_sigmask)(int, const sigset_t *, sigset_t *);

/** Fast check for whether interceptor init has been run */
extern bool ic_init_done;

extern bool intercepting_enabled;

/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_init():
 * Add an entry, with a new empty string array, to our pool.
 */
extern void psfa_init(const posix_spawn_file_actions_t *p);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_destroy():
 * Remove the entry, freeing up the string array, from our pool.
 * Do not shrink psfas.
 */
extern void psfa_destroy(const posix_spawn_file_actions_t *p);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_addopen():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_open builder to our structures.
 */
extern void psfa_addopen(const posix_spawn_file_actions_t *p, int fd,
                         const char *pathname, int flags, mode_t mode);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_addclose():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_close builder to our structures.
 */
extern void psfa_addclose(const posix_spawn_file_actions_t *p, int fd);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_addclosefrom_np():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_closefrom builder to our structures.
 */
extern void psfa_addclosefrom_np(const posix_spawn_file_actions_t *p, int fd);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_adddup2():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_dup2 builder to our structures.
 */
extern void psfa_adddup2(const posix_spawn_file_actions_t *p, int oldfd, int newfd);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_chdir_np():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_chdir builder to our structures.
 */
extern void psfa_addchdir_np(const posix_spawn_file_actions_t *p, const char *pathname);
/**
 * Additional bookkeeping to do after a successful posix_spawn_file_actions_fchdir_np():
 * Append a corresponding FBBCOMM_Builder_posix_spawn_file_action_fchdir builder to our structures.
 */
extern void psfa_addfchdir_np(const posix_spawn_file_actions_t *p, int fd);
/**
 * Find the voidp_array for a given posix_spawn_file_actions.
 */
extern voidp_array *psfa_find(const posix_spawn_file_actions_t *p);

extern voidp_set popened_streams;

/** Initial LD_LIBRARY_PATH so that we can fix it up if needed */
extern char env_ld_library_path[IC_PATH_BUFSIZE];

/** Insert marker open()-s for strace, ltrace, etc. */
extern bool insert_trace_markers;

/** System and ignore locations to not ask ACK for when opening them. */
extern cstring_view_array system_locations;
extern cstring_view_array ignore_locations;

/** Insert debug message */
extern void insert_debug_msg(const char*);

/** Insert begin marker strace, ltrace, etc. */
extern void insert_begin_marker(const char*);

/** Insert end marker strace, ltrace, etc. */
extern void insert_end_marker(const char*);

/**
 * Stored PID
 * When getpid() returns a different value, we missed a fork() :-)
 */
extern int ic_pid;

/**
 * Make the filename canonical in place.
 *
 * String operation only, does not look at the actual file system.
 * Removes double slashes, trailing slashes (except if the entire path is "/")
 * and "." components.
 * Preserves ".." components, since they might point elsewhere if a symlink led to
 * its containing directory.
 * See #401 for further details and gotchas.
 *
 * Returns the length of the canonicalized path.
 */
size_t make_canonical(char *path, size_t original_length);

#ifdef FB_EXTRA_DEBUG
static inline bool ic_cwd_ok() {
  char buf[IC_PATH_BUFSIZE];
  /* getcwd() is not intercepted */
  char * getcwd_ret = getcwd(buf, sizeof(buf));
  assert(getcwd_ret);
  return (strcmp(ic_cwd, buf) == 0);
}
#else
static inline bool ic_cwd_ok() {
  return true;
}
#endif

#define BUILDER_SET_CANONICAL2(msg, field, make_abs) do {               \
  const int orig_len = strlen(field);                                   \
  const bool fix_abs = make_abs && field[0] != '/';                     \
  const bool canonical = is_canonical(field, orig_len);                 \
  if (!fix_abs && canonical) {                                          \
    fbbcomm_builder_##msg##_set_##field##_with_length(&ic_msg, field,   \
                                                      orig_len);        \
  } else if (fix_abs                                                    \
             && (orig_len == 0                                          \
                 || (orig_len == 1 && field[0] == '.'))) {              \
    fbbcomm_builder_##msg##_set_##field##_with_length(&ic_msg, ic_cwd,  \
                                                      ic_cwd_len);      \
  } else {                                                              \
    char * c_buf =                                                      \
        alloca(orig_len + (fix_abs ? (ic_cwd_len + 2) : 1));            \
    int c_len;                                                          \
    if (fix_abs) {                                                      \
      assert(ic_cwd_ok());                                              \
      const size_t adjusted_cwd_len = ic_cwd_len == 1 ? 0 : ic_cwd_len; \
      memcpy(c_buf, ic_cwd, adjusted_cwd_len);                          \
      c_buf[adjusted_cwd_len] = '/';                                    \
      memcpy(&c_buf[adjusted_cwd_len + 1], field, orig_len + 1);        \
      c_len =                                                           \
          make_canonical(&c_buf[adjusted_cwd_len], orig_len + 1)        \
          + adjusted_cwd_len;                                           \
      if (c_len > 1 && c_buf[c_len - 1] == '/') {                       \
        c_buf[c_len - 1] = '\0';                                        \
        c_len--;                                                        \
      }                                                                 \
    } else {                                                            \
      memcpy(c_buf, field, orig_len + 1);                               \
      c_len = make_canonical(c_buf, orig_len);                          \
    }                                                                   \
    fbbcomm_builder_##msg##_set_##field##_with_length(&ic_msg, c_buf,   \
                                                      c_len);           \
  }                                                                     \
} while (0)

/* Note: These macros must be called in the function serializing the FBB message because the buffer
 * holding the canonical field is allocated on the stack. */
#define BUILDER_SET_ABSOLUTE_CANONICAL(msg, field) BUILDER_SET_CANONICAL2(msg, field, true)
#define BUILDER_SET_CANONICAL(msg, field) BUILDER_SET_CANONICAL2(msg, field, false)
#define BUILDER_MAYBE_SET_ABSOLUTE_CANONICAL(msg, dirfd, field)   \
  BUILDER_SET_CANONICAL2(msg, field, (dirfd == AT_FDCWD));

/** The method name the current thread is intercepting, or NULL. In case of nested interceptions
 *  (which can happen with signal handlers), it contains the outermost intercepted method. The value
 *  is used for internal assertions and debugging messages only, not for actual business logic. */
extern __thread const char *thread_intercept_on;

/** Whether the current thread is in "signal danger zone" where we don't like if a signal handler
 *  kicks in, because our data structures are inconsistent. Blocking/unblocking signals would be too
 *  slow for us (a pair of pthread_sigmask() kernel syscalls). So we just detect this scenario from
 *  the wrapped signal handler. It's a counter, similar to a recursive lock. */
extern __thread sig_atomic_t thread_signal_danger_zone_depth;

/** Only if thread_signal_danger_zone_depth == 0: tells whether the current thread holds
 *  ic_global_lock. Querying the lock itself would not be async-signal-safe (and there aren't direct
 *  methods for querying either), hence this accompanying value.
 *  If thread_signal_danger_zone_depth > 0, the contents are undefined and must not be relied on. */
extern __thread bool thread_has_global_lock;

/** The thread is performing a popen() call. */
extern __thread bool thread_performs_popen;

/** Counting the depth of nested signal handlers running in the current thread. */
extern __thread sig_atomic_t thread_signal_handler_running_depth;

/** Counting the nested depth of libc calls that might call other libc methods externally.
 *  Currently fork() (atfork handlers) and dlopen() (constructor method) increment this level.
 *  exit(), err(), error() etc. might also be ported to this infrastructure one day. */
extern __thread sig_atomic_t thread_libc_nesting_depth;

/** Bitmap of signals that we're delaying. Multiplicity is irrelevant. Since signals are counted
 *  from 1 to 64 (on Linux x86), it's bit number (signum-1) that corresponds to signal signum. */
extern __thread uint64_t thread_delayed_signals_bitmap;

/** Array of the original signal handlers.
 *  The items are actually either void (*)(int) a.k.a. sighandler_t,
 *  or void (*)(int, siginfo_t *, void *), depending on how the handler was installed. */
extern void (*orig_signal_handlers[IC_WRAP_SIGRTMAX])(void);

/** Whether we can intercept the given signal. */
bool signal_is_wrappable(int);

/** When a signal handler is installed using signal(), sigset(), or sigaction() without the
 *  SA_SIGINFO flag, this wrapper gets installed instead.
 *
 *  See tpl_signal.c for how this wrapper is installed instead of the actual handler.
 *
 *  This wrapper makes sure that the actual signal handler is only executed straight away if the
 *  thread is not inside a "signal danger zone". Otherwise execution is deferred until the danger
 *  zone is left (thread_signal_danger_zone_leave()).
 */
void wrapper_signal_handler_1arg(int);

/** When a signal handler is installed using sigaction() with the SA_SIGINFO flag,
 *  this wrapper gets installed instead.
 *
 *  See wrapper_signal_handler_1arg() for further details.
 */
void wrapper_signal_handler_3arg(int, siginfo_t *, void *);

/** Internal helper for thread_signal_danger_zone_leave(), see there for details. */
void thread_raise_delayed_signals();

/** Enter a "signal danger zone" where if a signal arrives then its execution is delayed.
 *  This delaying is done manually because sigprocmask() or pthread_sigmask() are too expensive
 *  for us. Our signal handler wrapper returns immediately (after the necessary bookkeeping,
 *  but without invoking the actual handler).
 *  The signal is later re-raised from thread_signal_danger_zone_leave().
 *  There can be multiple levels nested.
 *  Inline so that it's as fast as possible. */
static inline void thread_signal_danger_zone_enter() { thread_signal_danger_zone_depth++; }

/** Leave one level of "signal danger zone".
 *  See thread_signal_danger_zone_enter() for how we delay signals.
 *  If leaving the outermost level, re-raise the delayed signals.
 *  Inline so that the typical branch is as fast as possible. */
static inline void thread_signal_danger_zone_leave() {
  /* Leave this danger zone first.
   *
   * If leaving the outermost danger zone, a signal can now kick in any time after this decrement,
   * potentially even before we reach that raise() below to emit the delayed signal, and its real
   * handler is executed immediately. This reordering is not a problem (fingers crossed).
   *
   * (The other possible order of code lines, i.e. raise the delayed signals first, then leave the
   * danger zone, would suffer from a race condition if a signal kicks in between these steps.) */
  thread_signal_danger_zone_depth--;

  /* If this wasn't the outermost danger zone, there's nothing more to do, just return.
   * Also, obviously nothing to do if there's no delayed signal.
   *
   * Otherwise, re-raise them. (Note that in this case thread_delayed_signals_bitmap is stable now,
   * a randomly arriving signal cannot surprisingly modify it.) */
  if (thread_delayed_signals_bitmap != 0 && thread_signal_danger_zone_depth == 0) {
    /* The rarely executed heavy stuff is factored out to a separate method to reduce code size. */
    thread_raise_delayed_signals();
  }
}

/** Take the global lock if the thread does not hold it already */
void grab_global_lock(bool *i_locked, const char * const function_name);
void release_global_lock();

/**
 * Notify the supervisor after a fork(). Do it from the first registered pthread_atfork
 * handler so that it happens before other such handlers are run.
 * See #819 for further details.
 */
void atfork_parent_handler(void);

/**
 * Reconnect to the supervisor and reinitialize other stuff in the child
 * after a fork(). Do it from the first registered pthread_atfork
 * handler so that it happens before other such handlers are run.
 * See #237 for further details.
 */
void atfork_child_handler(void);

extern void fb_ic_load();
extern void handle_exit();
/**
 * A wrapper in front of the start_routine of a pthread_create(), inserting a useful trace marker.
 * pthread_create()'s two parameters start_routine and arg are accessed via one,
 * malloc()'ed in the intercepted pthread_create() and free()'d here.
 */
void *pthread_start_routine_wrapper(void *routine_and_arg);
extern int __libc_start_main(int (*main)(int, char **, char **),
                             int argc, char **ubp_av,
                             void (*init)(void), void (*fini)(void),
                             void (*rtld_fini)(void), void *stack_end);

#endif  // FIREBUILD_INTERCEPT_H_
