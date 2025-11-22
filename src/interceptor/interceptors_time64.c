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

/* interceptors.{cc,h} are the minimum necessary boilerplate
 * around the auto-generated gen_* interceptor files. */

/* This file is for the interceptors using 64bit time format on 32 architectures. */
#if (__SIZEOF_POINTER__ == 4)
#define _TIME_BITS 64
#define _FILE_OFFSET_BITS 64
#endif

#include "interceptor/interceptors.h"

#include <errno.h>

#if __has_include(<sys/pidfd.h>)
#include <sys/pidfd.h>
#endif

#include "common/firebuild_common.h"
#include "common/platform.h"
#include "interceptor/env.h"
#include "interceptor/ic_file_ops.h"
#include "interceptor/intercept.h"


/* Include the auto-generated definitions of the get_ic_orig function pointers */
#include "interceptor/gen_time64_def.c"

/* Include the auto-generated implementations of the interceptor functions */
#include "interceptor/gen_time64_impl.c"
