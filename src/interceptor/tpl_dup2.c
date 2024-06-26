{# ------------------------------------------------------------------ #}
{# Copyright (c) 2022 Firebuild Inc.                                  #}
{# All rights reserved.                                               #}
{# Free for personal use and commercial trial.                        #}
{# Non-trial commercial use requires licenses available from          #}
{# https://firebuild.com.                                             #}
{# Modification and redistribution are permitted, but commercial use  #}
{# of derivative works is subject to the same requirements of this    #}
{# license.                                                           #}
{# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,    #}
{# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF #}
{# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND              #}
{# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT        #}
{# HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,       #}
{# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, #}
{# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER      #}
{# DEALINGS IN THE SOFTWARE.                                          #}
{# ------------------------------------------------------------------ #}
{# Template for the dup2() and dup3() calls.                          #}
{# See issue #632 for detailed explanation.                           #}
{# ------------------------------------------------------------------ #}
### extends "tpl.c"

### block guard_connection_fd
  /* Only handle oldfd here, newfd is handled a bit later. */
  if (oldfd == fb_sv_conn) { errno = EBADF; return -1; }
### endblock

### block before
  int fb_sv_conn_new = -1;
  if (newfd == fb_sv_conn) {
    /* In order to make this dup2() or dup3() actually happen to the desired newfd
     * and still be able to talk to the supervisor,
     * we need to move fb_sv_conn to some other file descriptor. */
    fb_sv_conn_new = TEMP_FAILURE_RETRY(get_ic_orig_dup()(fb_sv_conn));
    if (fb_sv_conn_new < 0) {
      /* This dup() failed, which is very unlikely (out of available fds).
       * There's no hope to succeed with the actual dup2() and still be able to talk
       * to the supervisor. So just bail out. */
      if (i_locked) {
        release_global_lock();
      }
      errno = EBADF;
      return -1;
    }
    /* The communication fd has the close-on-exec flag set, and dup() doesn't copy it. */
    TEMP_FAILURE_RETRY(
#if defined(_TIME_BITS) && (_TIME_BITS == 64)
        get_ic_orig___fcntl_time64()(
#else
        get_ic_orig_fcntl()(
#endif
            fb_sv_conn_new, F_SETFD, FD_CLOEXEC));
  }
### endblock

### block after
  if (newfd == fb_sv_conn) {
    if (success) {
      /* The actual dup2() succeeded and thus automatically closed fb_sv_conn.
       * Use the new fd number from now on for the communication. */
      fb_sv_conn = fb_sv_conn_new;
    } else {
      /* The actual dup2() failed for whatever reason. Close the dupped connection fd.
       * POSIX says to retry close() on EINTR (e.g. wrap in TEMP_FAILURE_RETRY())
       * but Linux probably disagrees, see #723. */
      get_ic_orig_close()(fb_sv_conn_new);
    }
  }

  if (i_am_intercepting && success) copy_notify_on_read_write_state(newfd, oldfd);
### endblock
