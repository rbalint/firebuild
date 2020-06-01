/* Copyright (c) 2020 Interri Kft. */
/* This file is an unpublished work. All rights reserved. */

/* interceptors.{cc,h} are the minimum necessary boilerplate
 * around the auto-generated gen_* interceptor files. */

#ifndef FIREBUILD_INTERCEPTORS_H_
#define FIREBUILD_INTERCEPTORS_H_

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "fb-messages.pb.h"

#if __GLIBC_PREREQ (2, 28)
#define FB_SSIZE_T ssize_t
#define FB_VA_LIST va_list
#else
#define FB_SSIZE_T _IO_ssize_t
#define FB_VA_LIST _G_va_list
#endif

namespace firebuild {

#ifdef  __cplusplus
extern "C" {
#endif

void init_interceptors();
void reset_interceptors();

/* Include the auto-generated declarations of the ic_orig function pointers,
 * and some convenience #define redirects */
#include "interceptor/gen_decl.h"

#ifdef  __cplusplus
}  // extern "C"
#endif

}  // namespace firebuild

#endif  // FIREBUILD_INTERCEPTORS_H_