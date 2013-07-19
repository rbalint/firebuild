
#include <cassert>
#include <cstdarg>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "intercept.h"
#include "env.h"
#include "fb-messages.pb.h"
#include "firebuild_common.h"

using namespace std;

#ifdef  __cplusplus
extern "C" {
#endif

static void fb_ic_cleanup() __attribute__ ((destructor));

#ifdef  __cplusplus
}
#endif

/* global vars */
ic_fn_info ic_fn[IC_FN_IDX_MAX];

/* original intercepted functions */
__pid_t (*ic_orig_getpid) (void);
__pid_t (*ic_orig_getppid) (void);
char * (*ic_orig_getcwd) (char *, size_t);
ssize_t(*ic_orig_write)(int, const void *, size_t);
ssize_t(*ic_orig_read)(int, const void *, size_t);

/** Global lock for serializing critical interceptor actions */
pthread_mutex_t ic_global_lock = PTHREAD_MUTEX_INITIALIZER;

/** Connection string to supervisor */
char * fb_conn_string = NULL;

/** Connection file descriptor to supervisor */
int fb_sv_conn = -1;

/** interceptor init has been run */
bool ic_init_done = false;

/**
 * Stored PID
 * When getpid() returns a different value, we missed a fork() :-)
 */
int ic_pid;

/** Per thread variable which we turn on inside call interception */
__thread bool intercept_on = false;

/**
 * Reset globally maintained information about intercepted funtions
 */
void
reset_fn_infos ()
{
  int i;
  for (i = 0; i < IC_FN_IDX_MAX ; i++) {
    ic_fn[i].called = false;
  }
}

/**
 * Get pointer to a function implemented in the next shared
 * library. In our case this is a function we intercept.
 * @param[in] name function's name
 */
static void *
get_orig_fn (const char* name)
{
  void * function = dlsym(RTLD_NEXT, name);
  assert(function);
  return function;
}

/**
 * Get pointers to all the functions we intercept but we also want to use
 */
static void
set_orig_fns ()
{
  ic_orig_getpid = (__pid_t(*)(void))get_orig_fn("getpid");
  ic_orig_getppid = (__pid_t(*)(void))get_orig_fn("getppid");
  ic_orig_getcwd = (char *(*)(char *, size_t))get_orig_fn("getppid");
  ic_orig_write = (ssize_t(*)(int, const void *, size_t))get_orig_fn("write");
  ic_orig_read = (ssize_t(*)(int, const void *, size_t))get_orig_fn("read");
}

/**  Set up supervisor connection */
void
init_supervisor_conn () {

  struct sockaddr_un remote;
  size_t len;

  if (fb_conn_string == NULL) {
    fb_conn_string = strdup(getenv("FB_SOCKET"));
  }

  if ((fb_sv_conn = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    assert(fb_sv_conn != -1);
  }

  remote.sun_family = AF_UNIX;
  assert(strlen(fb_conn_string) < sizeof(remote.sun_path));
  strncpy(remote.sun_path, fb_conn_string, sizeof(remote.sun_path));

  len = strlen(remote.sun_path) + sizeof(remote.sun_family);
  if (connect(fb_sv_conn, (struct sockaddr *)&remote, len) == -1) {
    assert(0 && "connection to supervisor failed");
  }
}

/** buffer for getcwd */
#define CWD_BUFSIZE 4096
static char cwd_buf[CWD_BUFSIZE];

/**
 * Initialize interceptor's data structures and sync with supervisor
 */
static void fb_ic_init()
{
  char **argv, **env, **cursor, *cwd_ret;
  __pid_t pid, ppid;
  ShortCutProcessQuery *proc;
  ShortCutProcessResp * resp;
  InterceptorMsg ic_msg;
  SupervisorMsg sv_msg;

  GOOGLE_PROTOBUF_VERIFY_VERSION;

  set_orig_fns();
  reset_fn_infos();

  init_supervisor_conn();

  get_argv_env(&argv, &env);
  ic_pid = pid = ic_orig_getpid();
  ppid = ic_orig_getppid();
  cwd_ret = ic_orig_getcwd(cwd_buf, CWD_BUFSIZE);
  assert(cwd_ret != NULL);

  proc = ic_msg.mutable_scproc_query();

  proc->set_pid(pid);
  proc->set_ppid(ppid);
  proc->set_cwd(cwd_buf);

  for (cursor = argv; *cursor != NULL; cursor++) {
    proc->add_arg(*cursor);
  }

  for (cursor = env; *cursor != NULL; cursor++) {
    proc->add_env_var(*cursor);
  }

  fb_send_msg(ic_msg, fb_sv_conn);
  fb_recv_msg(sv_msg, fb_sv_conn);

  resp = sv_msg.mutable_scproc_resp();
  // we may return immediately if supervisor decides that way
  if (resp->shortcut()) {
    if (resp->has_exit_status()) {
      exit(resp->exit_status());
    } else {
      // TODO send error
    }
  }
  ic_init_done = true;
}

/**
 * Collect information about process the earliest possible, right
 * when interceptor library loads or when the first interceped call happens
 */
void fb_ic_load()
{
  if (!ic_init_done) {
    fb_ic_init();
  }
}


static void fb_ic_cleanup()
{
  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();
  close(fb_sv_conn);
}


/** wrapper for write() retrying on recoverable errors*/
ssize_t fb_write_buf(int fd, const void *buf, const size_t count)
{
  pthread_mutex_lock(&ic_global_lock);
  FB_IO_OP_BUF(ic_orig_write, fd, buf, count, {pthread_mutex_unlock(&ic_global_lock);});
}

/** wrapper for write() retrying on recoverable errors*/
ssize_t fb_read_buf(int fd, const void *buf, const size_t count)
{
  pthread_mutex_lock(&ic_global_lock);
  FB_IO_OP_BUF(ic_orig_read, fd, buf, count, {pthread_mutex_unlock(&ic_global_lock);});
}
