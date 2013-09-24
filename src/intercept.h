/*
 * Interceptor library definitions
 */

#ifndef _INTERCEPT_H
#define _INTERCEPT_H

#include <dlfcn.h>
#include <pthread.h>
#include <dirent.h>
#include <vector>

#include "firebuild_common.h"

/**
 * Intercept call
 */
#define IC(ret_type, name, parameters, body)				\
  IC_VOID(ret_type, name, parameters,					\
	  { ret_type ret;						\
	    body;							\
	    intercept_on = false;					\
	    return ret;							\
	  })

#endif

/**
 * Just send the intercepted function's name
 */
#define IC_GENERIC(ret_type, name, parameters, body)		\
  IC(ret_type, name, parameters,				\
     {								\
       if (!ic_fn[IC_FN_IDX_##name].called) {			\
	 InterceptorMsg ic_msg;					\
	 GenericCall *m;					\
	 m = ic_msg.mutable_gen_call();				\
	 m->set_call(#name);					\
	 fb_send_msg(ic_msg, fb_sv_conn);			\
	 ic_fn[IC_FN_IDX_##name].called = true;			\
       }							\
       body;							\
     })

#define IC_GENERIC_VOID(ret_type, name, parameters, body)		\
  IC_VOID(ret_type, name, parameters,					\
	  {								\
	    if (!ic_fn[IC_FN_IDX_##name].called) {			\
	      InterceptorMsg ic_msg;					\
	      GenericCall *m;						\
	      m = ic_msg.mutable_gen_call();				\
	      m->set_call(#name);					\
	      fb_send_msg(ic_msg, fb_sv_conn);				\
	      ic_fn[IC_FN_IDX_##name].called = true;			\
	    }								\
	    body;							\
	  })


/* create global array indexed by intercepted function's id */
#define IC_VOID(_ret_type, name, _parameters, _body)	\
  IC_FN_IDX_##name,

/* we need to include every file using IC() macro to create index for all
 * functions */
enum {
#include "ic_file_ops.h"
  IC_FN_IDX_MAX
};
#undef IC_VOID

/* create ic_orig_... version of intercepted function */
#define IC_VOID(ret_type, name, parameters, _body)	\
  extern ret_type (*ic_orig_##name) parameters;

/* we need to include every file using IC() macro to create ic_orig_... version
 * for all functions */
#include "ic_file_ops.h"
#undef IC_VOID

typedef struct {
  bool called;
} ic_fn_info;

extern ic_fn_info ic_fn[IC_FN_IDX_MAX];

/** file usage state */
typedef struct {
  bool read; /** file has been read */
  bool written; /** file has been written to */
} fd_state;

/** file fd states */
extern std::vector<fd_state> fd_states;

/** Global lock for manipulating fd states */
extern pthread_mutex_t ic_fd_states_lock;

/** buffer size for getcwd */
#define CWD_BUFSIZE 4096

/** Reset globally maintained information about intercepted funtions */
extern void reset_fn_infos ();

/**  Set up supervisor connection */
extern void init_supervisor_conn ();

/** Global lock for serializing critical interceptor actions */
extern pthread_mutex_t ic_global_lock;

/** Connection file descriptor to supervisor */
extern int fb_sv_conn;

/** interceptor init has been run */
extern bool ic_init_done;

/** interceptor handled exit */
extern bool fb_exit_handled;

/** Add shared library's name to the file list */
extern int shared_libs_cb(struct dl_phdr_info *info, size_t size, void *data);

/** Send error message to supervisor */
extern void fb_error(const char* msg);

/** Send debug message to supervisor id debug level is at least lvl */
extern void fb_debug(int lvl, const char* msg);

/**
 * Stored PID
 * When getpid() returns a different value, we missed a fork() :-)
 */
extern int ic_pid;

/** Per thread variable which we turn on inside call interception */
extern __thread bool intercept_on;

#ifdef  __cplusplus
extern "C" {
#endif

extern void fb_ic_load() __attribute__ ((constructor));
extern void handle_exit (const int status, void*);
extern int __libc_start_main (int (*main) (int, char **, char **),
                              int argc, char **ubp_av,
                              void (*init) (void), void (*fini) (void),
                              void (*rtld_fini) (void), void (* stack_end));

#ifdef  __cplusplus
}
#endif

/**
 * Intercept call returning void
 */
#define IC_VOID(ret_type, name, parameters, body)			\
  extern ret_type (name) parameters					\
  {									\
    /* local name for original intercepted function */			\
    ret_type (* orig_fn)parameters = ic_orig_##name;			\
    /* If we are called before the constructor we have to look up */	\
    /* function for ourself. This happens once per process run. */	\
    if (!orig_fn) {							\
      orig_fn = (ret_type(*)parameters)dlsym(RTLD_NEXT, #name);		\
      assert(orig_fn);							\
    }									\
    assert(intercept_on == false);					\
    intercept_on = true;						\
    fb_ic_load();							\
  { 									\
    body; /* this is where interceptor function body goes */		\
  }									\
  intercept_on = false;							\
}

