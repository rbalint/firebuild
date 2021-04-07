/* Copyright (c) 2014 Balint Reczey <balint@balintreczey.hu> */
/* This file is an unpublished work. All rights reserved. */


#include "firebuild/process.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <utility>

#include "common/firebuild_common.h"
#include "firebuild/file.h"
#include "firebuild/pipe_recorder.h"
#include "firebuild/platform.h"
#include "firebuild/execed_process.h"
#include "firebuild/execed_process_env.h"
#include "firebuild/debug.h"
#include "firebuild/utils.h"

namespace firebuild {

static int fb_pid_counter;

Process::Process(const int pid, const int ppid, const int exec_count, const FileName *wd,
                 Process * parent, std::shared_ptr<std::vector<std::shared_ptr<FileFD>>> fds)
    : parent_(parent), state_(FB_PROC_RUNNING), fb_pid_(fb_pid_counter++),
      pid_(pid), ppid_(ppid), exec_count_(exec_count), exit_status_(-1), wd_(wd), fds_(fds),
      closed_fds_({}), fork_children_(), expected_child_(), exec_child_(NULL) {
  TRACKX(FB_DEBUG_PROC, 0, 1, Process, this, "pid=%d, ppid=%d, parent=%s", pid, ppid, D(parent));
}

void Process::update_rusage(const int64_t utime_u, const int64_t stime_u) {
  ExecedProcess* ep = exec_point();
  if (ep) {
    ep->add_utime_u(utime_u);
    ep->add_stime_u(stime_u);
  }
}

void Process::exit_result(const int status, const int64_t utime_u,
                          const int64_t stime_u) {
  /* The kernel only lets the low 8 bits of the exit status go through.
   * From the exit()/_exit() side, the remaining bits are lost (they are
   * still there in on_exit() handlers).
   * From wait()/waitpid() side, additional bits are used to denote exiting
   * via signal.
   * We use -1 if there's no exit status available (the process is still
   * running, or exited due to an unhandled signal). */
  exit_status_ = status & 0xff;
  update_rusage(utime_u, stime_u);
}

std::shared_ptr<FileFD>
Process::add_filefd(std::shared_ptr<std::vector<std::shared_ptr<FileFD>>> fds,
                    int fd,
                    std::shared_ptr<FileFD> ffd) {
  TRACK(FB_DEBUG_PROC, "fd=%d", fd);

  if (fds->size() <= static_cast<unsigned int>(fd)) {
    fds->resize(fd + 1, nullptr);
  }
  if ((*fds)[fd]) {
    firebuild::fb_error("Fd " + d(fd) + " is already tracked as being open.");
  }
  // the shared_ptr takes care of cleaning up the old fd if needed
  (*fds)[fd] = ffd;
  return ffd;
}

void Process::add_pipe(std::shared_ptr<Pipe> pipe) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "pipe=%s", D(pipe.get()));

  exec_point()->add_pipe(pipe);
}

void Process::drain_all_pipes() {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "");

  for (auto file_fd : *fds_) {
    if (!file_fd || !is_wronly(file_fd->flags())) {
      continue;
    }
    auto pipe = file_fd->pipe().get();
    if (pipe) {
      pipe->drain_fd1_end(file_fd.get());
    }
  }
}

std::shared_ptr<std::vector<std::shared_ptr<FileFD>>> Process::pass_on_fds(bool execed) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "execed=%s", D(execed));

  auto fds = std::make_shared<std::vector<std::shared_ptr<FileFD>>>();
  for (unsigned int i = 0; i < fds_->size(); i++) {
    auto const &file_fd_shared_ptr = fds_->at(i);
    if (file_fd_shared_ptr) {
      const FileFD& raw_file_fd = *file_fd_shared_ptr.get();
      if (!(execed && raw_file_fd.cloexec())) {
        /* The operations on the fds in the new process don't affect the fds in the parent,
         * thus create a copy of the parent's FileFD pointed to by a new shared pointer. */
        add_filefd(fds, i, std::make_shared<FileFD>(raw_file_fd));
        if (execed && raw_file_fd.close_on_popen()) {
          /* The newly exec()-ed process will not close inherited popen()-ed fds on pclose() */
          (*fds)[i]->set_close_on_popen(false);
        }
      }
    }
  }
  return fds;
}

void Process::AddPopenedProcess(int fd, const char *fifo, ExecedProcess *proc, int type_flags) {
  /* The stdin/stdout of the child needs to be conected to parent's fd */
  int child_fileno = is_wronly(type_flags) ? STDIN_FILENO : STDOUT_FILENO;
  if (child_fileno == STDOUT_FILENO) {
    /* Since only fd0 is passed only fd0's FileFD inherits O_CLOEXEC from type_flags. */
    auto pipe = handle_pipe_internal(fd, -1, type_flags, 0 /* fd1_flags */, 0, fifo, nullptr,
                                     true, false, this);
    auto ffd1 = std::make_shared<FileFD>(child_fileno, O_WRONLY, pipe->fd1_shared_ptr(),
                                         proc->parent_);

    (*proc->parent_->fds_)[child_fileno] = ffd1;
    (*proc->fds_)[child_fileno] = ffd1;
    add_pipe(pipe);
  } else {
    /* child_fileno == STDIN_FILENO */
    /* Create the pipe later, in accept_exec_child(). */
    assert(!fifo);
  }

  fd2popen_child_[fd] = proc;
}

int Process::handle_open(const int dirfd, const char * const ar_name, const int flags,
                         const int fd, const int error, int fd_conn, const int ack_num) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this,
         "dirfd=%d, ar_name=%s, flags=%d, fd=%d, error=%d, fd_conn=%s, ack_num=%d",
         dirfd, D(ar_name), flags, fd, error, D_FD(fd_conn), ack_num);

  const FileName* name = get_absolute(dirfd, ar_name);
  if (!name) {
    // FIXME don't disable shortcutting if openat() failed due to the invalid dirfd
    exec_point()->disable_shortcutting_bubble_up("Invalid dirfd passed to openat()");
    return -1;
  }

  if (fd >= 0) {
    add_filefd(fds_, fd, std::make_shared<FileFD>(name, fd, flags, this));
  }

  if (ack_num != 0) {
    ack_msg(fd_conn, ack_num);
  }

  if (!error && !exec_point()->register_parent_directory(name)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the implicit parent directory of " + d(name));
    return -1;
  }

  if (!exec_point()->register_file_usage(name, name, FILE_ACTION_OPEN, flags, error)) {
    exec_point()->disable_shortcutting_bubble_up("Could not register the opening of " + d(name));
    return -1;
  }

  return 0;
}

/* close that fd if open, silently ignore if not open */
int Process::handle_force_close(const int fd) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d", fd);

  if (get_fd(fd)) {
    return handle_close(fd, 0);
  }
  return 0;
}

int Process::handle_close(const int fd, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d, error=%d", fd, error);

  FileFD* file_fd = get_fd(fd);

  if (error == EIO) {
    // IO prevents shortcutting
    exec_point()->disable_shortcutting_bubble_up("IO error closing fd " + d(fd));
    return -1;
  } else if (error == 0 && !file_fd) {
    // closing an unknown fd successfully prevents shortcutting
    exec_point()->disable_shortcutting_bubble_up(
        "Process closed an unknown fd (" + d(fd) + ") successfully, "
        "which means interception missed at least one open()");
    return -1;
  } else if (error == EBADF) {
    // Process closed an fd unknown to it. Who cares?
    assert(!get_fd(fd));
    return 0;
  } else {
    if (!file_fd) {
      // closing an unknown fd with not EBADF prevents shortcutting
      exec_point()->disable_shortcutting_bubble_up(
          "Process closed an unknown fd (" + d(fd) + ") successfully, "
          "which means interception missed at least one open()");
      return -1;
    } else {
      if (file_fd->open() == true) {
        file_fd->set_open(false);
        if (file_fd->last_err() != error) {
          file_fd->set_last_err(error);
        }
        /* Remove from open fds. The (*fds_)[fd].reset() is performed by the move. */
        closed_fds_.push_back(std::move((*fds_)[fd]));
        auto pipe = file_fd->pipe().get();
        if (pipe) {
          /* There may be data pending, drain it and register closure. */
          pipe->handle_close(file_fd);
          file_fd->set_pipe(nullptr);
        }
        return 0;
      } else if ((file_fd->last_err() == EINTR) && (error == 0)) {
        // previous close got interrupted but the current one succeeded
        (*fds_)[fd].reset();
        return 0;
      } else {
        // already closed, it may be an error
        // TODO(rbalint) debug
        (*fds_)[fd].reset();
        return 0;
      }
    }
  }
}

int Process::handle_unlink(const int dirfd, const char * const ar_name,
                           const int flags, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "dirfd=%d, ar_name=%s, flags=%d, error=%d",
         dirfd, D(ar_name), flags, error);

  const FileName* name = get_absolute(dirfd, ar_name);
  if (!name) {
    // FIXME don't disable shortcutting if unlinkat() failed due to the invalid dirfd
    exec_point()->disable_shortcutting_bubble_up("Invalid dirfd passed to unlinkat()");
    return -1;
  }

  if (!error) {
    if (!exec_point()->register_parent_directory(name)) {
      exec_point()->disable_shortcutting_bubble_up(
          "Could not register the implicit parent directory of " + d(name));
      return -1;
    }

    // FIXME When a directory is removed, register that it was an _empty_ directory
    FileUsage fu(flags & AT_REMOVEDIR ? ISDIR : ISREG);
    fu.set_written(true);
    if (!exec_point()->register_file_usage(name, fu)) {
      exec_point()->disable_shortcutting_bubble_up(
          "Could not register the unlink or rmdir of " + d(name));
      return -1;
    }
  }

  return 0;
}

int Process::handle_rmdir(const char * const ar_name, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "ar_name=%s, error=%d", D(ar_name), error);

  return handle_unlink(AT_FDCWD, ar_name, AT_REMOVEDIR, error);
}

int Process::handle_mkdir(const int dirfd, const char * const ar_name, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "dirfd=%d, ar_name=%s, error=%d",
         dirfd, D(ar_name), error);

  const FileName* name = get_absolute(dirfd, ar_name);
  if (!name) {
    // FIXME don't disable shortcutting if mkdirat() failed due to the invalid dirfd
    exec_point()->disable_shortcutting_bubble_up("Invalid dirfd passed to mkdirat()");
    return -1;
  }

  if (!error && !exec_point()->register_parent_directory(name)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the implicit parent directory of " + d(name));
    return -1;
  }

  if (!exec_point()->register_file_usage(name, name, FILE_ACTION_MKDIR, 0, error)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the directory creation of " + d(name));
    return -1;
  }

  return 0;
}

std::shared_ptr<Pipe> Process::handle_pipe_internal(const int fd0, const int fd1,
                                                    const int fd0_flags, const int fd1_flags,
                                                    const int error, const char *fd0_fifo,
                                                    const char *fd1_fifo, bool fd0_close_on_popen,
                                                    bool fd1_close_on_popen,  Process *fd0_proc) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this,
         "fd0=%d, fd1=%d, fd0_flags=%d, fd1_flags=%d, error=%d, fd0_fifo=%s, fd1_fifo=%s,"
         " fd0_proc=%s",
         fd0, fd1, fd0_flags, fd1_flags, error, fd0_fifo, fd1_fifo, D(fd0_proc));

  if (error) {
    return std::shared_ptr<Pipe>(nullptr);
  }

  // validate fd-s
  if (fd0_proc->get_fd(fd0)) {
    // we already have this fd, probably missed a close()
    fd0_proc->exec_point()->disable_shortcutting_bubble_up(
        "Process created an fd (" + d(fd0) + ") which is known to be open, "
        "which means interception missed at least one close()");
    return std::shared_ptr<Pipe>(nullptr);
  }

  if (fd1 != -1 && get_fd(fd1)) {
    // we already have this fd, probably missed a close()
    exec_point()->disable_shortcutting_bubble_up(
        "Process created an fd (" + d(fd1) + ") which is known to be open, "
        "which means interception missed at least one close()");
    return std::shared_ptr<Pipe>(nullptr);
  }

  assert(fd0_fifo);
  int fd0_conn = open(fd0_fifo, O_WRONLY | O_NONBLOCK);
  assert(fd0_conn != -1 && "opening fd0_fifo failed");
  firebuild::bump_fd_age(fd0_conn);

#ifdef __clang_analyzer__
  /* Scan-build reports a false leak for the correct code. This is used only in static
   * analysis. It is broken because all shared pointers to the Pipe must be copies of
   * the shared self pointer stored in it. */
  auto pipe = std::make_shared<Pipe>(fd0_conn, this);
#else
  auto pipe = (new Pipe(fd0_conn, this))->shared_ptr();
#endif
  fd0_proc->add_filefd(fd0_proc->fds_, fd0, std::make_shared<FileFD>(
      fd0, (fd0_flags & ~O_ACCMODE) | O_RDONLY, pipe->fd0_shared_ptr(), fd0_proc,
      fd0_close_on_popen));
  /* Fd1_fifo may not be set. */
  if (fd1_fifo) {
    int fd1_conn = open(fd1_fifo, O_NONBLOCK | O_RDONLY);
    assert(fd1_conn != -1 && "opening fd1_fifo failed");
    firebuild::bump_fd_age(fd1_conn);
    auto ffd1 = std::make_shared<FileFD>(fd1, (fd1_flags & ~O_ACCMODE) | O_WRONLY,
                                         pipe->fd1_shared_ptr(),
                                         this, fd1_close_on_popen);
    add_filefd(fds_, fd1, ffd1);
    /* Empty recorders array. We don't start recording after a pipe(), this data wouldn't be
     * used anywhere. We only start recording after an exec(), to catch the traffic as seen
     * from that potential shortcutting point. */
    auto recorders = std::vector<std::shared_ptr<PipeRecorder>>();
    pipe->add_fd1_and_proc(fd1_conn, ffd1.get(), exec_point(), recorders);
  }
  return pipe;
}

int Process::handle_dup3(const int oldfd, const int newfd, const int flags,
                         const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "oldfd=%d, newfd=%d, flags=%d, error=%d",
         oldfd, newfd, flags, error);

  switch (error) {
    case EBADF:
    case EBUSY:
    case EINTR:
    case EINVAL:
    case ENFILE: {
      // dup() failed
      return 0;
    }
    case 0:
    default:
      break;
  }

  // validate fd-s
  if (!get_fd(oldfd)) {
    // we already have this fd, probably missed a close()
    exec_point()->disable_shortcutting_bubble_up(
        "Process created an fd (" + d(oldfd) + ") which is known to be open, "
        "which means interception missed at least one close()");
    return -1;
  }

  // dup2()'ing an existing fd to itself returns success and does nothing
  if (oldfd == newfd) {
    return 0;
  }

  handle_force_close(newfd);

  add_filefd(fds_, newfd, std::make_shared<FileFD>(
      newfd, (((*fds_)[oldfd]->flags() & ~O_CLOEXEC) | flags), FD_ORIGIN_DUP,
      (*fds_)[oldfd]));
  return 0;
}

int Process::handle_rename(const int olddirfd, const char * const old_ar_name,
                           const int newdirfd, const char * const new_ar_name,
                           const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this,
         "olddirfd=%d, old_ar_name=%s, newdirfd=%d, new_ar_name=%s, error=%d",
         olddirfd, D(old_ar_name), newdirfd, D(new_ar_name), error);

  if (error) {
    return 0;
  }

  /*
   * Note: rename() is different from "mv" in at least two aspects:
   *
   * - Can't move to a containing directory, e.g.
   *     rename("/home/user/file.txt", "/tmp");  // or "/tmp/"
   *   fails, you have to specify the full new name like
   *     rename("/home/user/file.txt", "/tmp/file.txt");
   *   instead.
   *
   * - If the source is a directory then the target can be an empty directory,
   *   which will be atomically removed beforehand. E.g. if "mytree" is a directory then
   *     mkdir("/tmp/target");
   *     rename("mytree", "/tmp/target");
   *   will result in the former "mytree/file.txt" becoming "/tmp/target/file.txt",
   *   with no "mytree" component. "target" has to be an empty directory for this to work.
   */

  const FileName* old_name = get_absolute(olddirfd, old_ar_name);
  const FileName* new_name = get_absolute(newdirfd, new_ar_name);
  if (!old_name || !new_name) {
    // FIXME don't disable shortcutting if renameat() failed due to the invalid dirfd
    exec_point()->disable_shortcutting_bubble_up("Invalid dirfd passed to renameat()");
    return -1;
  }

  struct stat64 st;
  if (lstat64(new_name->c_str(), &st) < 0 ||
      !S_ISREG(st.st_mode)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the renaming of non-regular file " + d(old_name));
    return -1;
  }

  if (!exec_point()->register_parent_directory(old_name)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the implicit parent directory of " + d(old_name));
    return -1;
  }
  if (!exec_point()->register_parent_directory(new_name)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the implicit parent directory of " + d(new_name));
    return -1;
  }

  /* It's tricky because the renaming has already happened, there's supposedly nothing
   * at the old filename. Yet we need to register that we read that file with its
   * particular hash value.
   * FIXME we compute the hash twice, both for the old and new location.
   * FIXME refactor so that it plays nicer together with register_file_usage(). */

  /* Register the opening for reading at the old location */
  if (!exec_point()->register_file_usage(old_name, new_name, FILE_ACTION_OPEN, O_RDONLY, error)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the renaming from " + d(old_name));
    return -1;
  }

  /* Register the opening for writing at the new location */
  if (!exec_point()->register_file_usage(new_name, new_name,
                                         FILE_ACTION_OPEN, O_CREAT|O_WRONLY|O_TRUNC, error)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Could not register the renaming to " + d(new_name));
    return -1;
  }

  return 0;
}

int Process::handle_symlink(const char * const old_ar_name,
                            const int newdirfd, const char * const new_ar_name,
                            const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this,
         "old_ar_name=%s, newdirfd=%d, new_ar_name=%s, error=%d",
         D(old_ar_name), newdirfd, D(new_ar_name), error);

  if (!error) {
    exec_point()->disable_shortcutting_bubble_up(
        "Process created a symlink "
        "([" + d(newdirfd) + "]" + d(new_ar_name) + " -> " + d(old_ar_name) + ")");
    return -1;
  }
  return 0;
}

int Process::handle_clear_cloexec(const int fd) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d", fd);

  if (!get_fd(fd)) {
    exec_point()->disable_shortcutting_bubble_up(
        "Process successfully cleared cloexec on fd (" + d(fd) + ") which is known to be closed, "
        "which means interception missed at least one open()");
    return -1;
  }
  (*fds_)[fd]->set_cloexec(false);
  return 0;
}

int Process::handle_fcntl(const int fd, const int cmd, const int arg,
                          const int ret, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d, cmd=%d, arg=%d, ret=%d, error=%d",
         fd, cmd, arg, ret, error);

  switch (cmd) {
    case F_DUPFD:
      return handle_dup3(fd, ret, 0, error);
    case F_DUPFD_CLOEXEC:
      return handle_dup3(fd, ret, O_CLOEXEC, error);
    case F_SETFD:
      if (error == 0) {
        if (!get_fd(fd)) {
          exec_point()->disable_shortcutting_bubble_up(
              "Process successfully fcntl'ed on fd (" + d(fd) + ") which is known to be closed, "
              "which means interception missed at least one open()");
          return -1;
        }
        (*fds_)[fd]->set_cloexec(arg & FD_CLOEXEC);
      }
      return 0;
    default:
      exec_point()->disable_shortcutting_bubble_up("Process executed unsupported fcntl " + d(cmd));
      return 0;
  }
}

int Process::handle_ioctl(const int fd, const int cmd,
                          const int ret, const int error) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d, cmd=%d, ret=%d, error=%d",
         fd, cmd, ret, error);

  (void) ret;

  switch (cmd) {
    case FIOCLEX:
      if (error == 0) {
        if (!get_fd(fd)) {
          exec_point()->disable_shortcutting_bubble_up(
              "Process successfully ioctl'ed on fd (" + d(fd) + ") which is known to be closed, "
              "which means interception missed at least one open()");
          return -1;
        }
        (*fds_)[fd]->set_cloexec(true);
      }
      return 0;
    case FIONCLEX:
      if (error == 0) {
        if (!get_fd(fd)) {
          exec_point()->disable_shortcutting_bubble_up(
              "Process successfully ioctl'ed on fd (" + d(fd) + ") which is known to be closed, "
              "which means interception missed at least one open()");
          return -1;
        }
        (*fds_)[fd]->set_cloexec(false);
      }
      return 0;
    default:
      exec_point()->disable_shortcutting_bubble_up("Process executed unsupported ioctl " + d(cmd));
      return 0;
  }
}

void Process::handle_read(const int fd) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d", fd);

  if (!get_fd(fd)) {
    exec_point()->disable_shortcutting_bubble_up("Process successfully read from (" + d(fd) +
                                   ") which is known to be closed, which means interception"
                                   " missed at least one open()");
    return;
  }
  /* Note: this doesn't disable any shortcutting if (*fds_)[fd]->opened_by() == this,
   * i.e. the file was opened by the current process. */
  Process* opened_by = (*fds_)[fd]->opened_by();
  exec_point()->disable_shortcutting_bubble_up_to_excl(
      opened_by ? opened_by->exec_point() : nullptr, "Process read from inherited fd " + d(fd));
}

void Process::handle_write(const int fd) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d", fd);

  if (!get_fd(fd)) {
    exec_point()->disable_shortcutting_bubble_up("Process successfully wrote to (" + d(fd) +
                                   ") which is known to be closed, which means interception"
                                   " missed at least one open()");
    return;
  }
  if (!(*fds_)[fd]->pipe()) {
    /* Note: this doesn't disable any shortcutting if (*fds_)[fd]->opened_by() == this,
     * i.e. the file was opened by the current process. */
    Process* opened_by = (*fds_)[fd]->opened_by();
    exec_point()->disable_shortcutting_bubble_up_to_excl(
        opened_by ? opened_by->exec_point() : nullptr,
        "Process wrote to inherited non-pipe fd " + d(fd));
  }
}

void Process::handle_set_wd(const char * const ar_d) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "ar_d=%s", ar_d);

  wd_ = get_absolute(AT_FDCWD, ar_d);
  assert(wd_);
  add_wd(wd_);
}

void Process::handle_set_fwd(const int fd) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "fd=%d", fd);

  const FileFD* ffd = get_fd(fd);
  if (!ffd) {
    exec_point()->disable_shortcutting_bubble_up("Process successfully fchdir()'ed to (" + d(fd) +
                                   ") which is known to be closed, which means interception"
                                   " missed at least one open()");
    return;
  }
  wd_ = ffd->filename();
  assert(wd_);
  add_wd(wd_);
}

/**
 * Canonicalize the filename in place.
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
static inline size_t canonicalize_path(char *path, size_t original_length) {
  TRACK(FB_DEBUG_PROC, "path=%s, original_length=%ld", D(path), original_length);

  char *src = path, *dst = path;  /* dst <= src all the time */
  bool add_slash = true;

  if (path[0] == '\0') return 0;

  if (!(path[0] == '.' && path[1] == '/')) {
    char *a = strstr(path, "//");
    char *b = strstr(path, "/./");
    if (a == NULL && b == NULL) {
      /* This is the quick code path for most of the well-behaved paths:
       * doesn't start with "./", doesn't contain "//" or "/./".
       * If a path passes this check then the only thing that might need
       * fixing is a trailing "/" or "/.". */
      size_t len = original_length;
      if (len >= 2 && path[len - 1] == '.' && path[len - 2] == '/') {
        /* Strip the final "." if the path ends in "/.". */
        len--;
        path[len] = '\0';
      }
      if (len >= 2 && path[len - 1] == '/') {
        /* Strip the final "/" if the path ends in "/" and that's not the entire path. */
        len--;
        path[len] = '\0';
      }
      /* The quick code path is done here. */
      return len;
    }
    /* Does not start with "./", but contains at least a "//" or "/./".
     * Everything is fine up to that point. Fast forward src and dst. */
    if (a != NULL && b != NULL) {
      src = dst = a < b ? a : b;
    } else if (a != NULL) {
      src = dst = a;
    } else {
      src = dst = b;
    }
  } else {
    /* Starts with "./", needs fixing from the beginning. */
    src++;
    add_slash = false;  /* Don't add "/" to dst when skipping the first one(s) in src. */
  }

  while (src[0] != '\0') {
    /* Skip through a possible run of slashes and non-initial "." components, e.g. "//././". */
    if (src[0] == '/') {
      while (src[0] == '/' || (src[0] == '.' && (src[1] == '/' || src[1] == '\0'))) src++;
      if (add_slash) {
        *dst++ = '/';
      }
    }
    /* Handle a regular (not ".") component. */
    while (src[0] != '/' && src[0] != '\0') {
      *dst++ = *src++;
    }
    add_slash = true;
  }

  /* If got empty path then it should be a "." instead. */
  if (dst == path) {
    *dst++ = '.';
  }
  /* Strip trailing slash, except if the entire path is "/". */
  if (dst > path + 1 && dst[-1] == '/') {
    dst--;
  }

  *dst = '\0';
  return dst - path;
}

const FileName* Process::get_absolute(const int dirfd, const char * const name, ssize_t length) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "dirfd=%d, name=%s, length=%ld",
         dirfd, D(name), length);

  if (platform::path_is_absolute(name)) {
    return FileName::Get(name, length);
  } else {
    char on_stack_buf[2048], *buf;

    const FileName* dir;
    if (dirfd == AT_FDCWD) {
      dir = wd();
    } else {
      const FileFD* ffd = get_fd(dirfd);
      if (ffd) {
        dir = ffd->filename();
        if (!dir) {
          return nullptr;
        }
      } else {
        return nullptr;
      }
    }

    const size_t on_stack_buffer_size = sizeof(on_stack_buf);
    const ssize_t name_length = (length == -1) ? strlen(name) : length;
    const size_t total_buf_len = dir->length() + 1 + name_length + 1;
    if (on_stack_buffer_size < total_buf_len) {
      buf = reinterpret_cast<char *>(malloc(total_buf_len));
    } else {
      buf = reinterpret_cast<char *>(on_stack_buf);
    }
    memcpy(buf, dir->c_str(), dir->length());
    buf[dir->length()] = '/';
    memcpy(buf + dir->length() + 1, name, name_length);
    buf[total_buf_len - 1] = '\0';
    const size_t canonicalized_len = canonicalize_path(buf, total_buf_len - 1);
    const FileName* ret = FileName::Get(buf, canonicalized_len);
    if (on_stack_buffer_size < total_buf_len) {
      free(buf);
    }
    return ret;
  }
}

static bool argv_matches_expectation(const std::vector<std::string>& actual,
                                     const std::vector<std::string>& expected) {
  /* When launching ["foo", "arg1"], the new process might be something like
   * ["/bin/bash", "-e", "./foo", "arg1"].
   *
   * If the program name already contained a slash, or if it refers to a binary executable,
   * it remains unchanged. If it didn't contain a slash and it refers to an interpreted file,
   * it's replaced by an absolute or relative path containing at least one slash (e.g. "./foo").
   *
   * A single interpreter might only add one additional command line parameter, but there can be
   * a chain of interpreters, so allow for arbitrarily many prepended parameters.
   */
  if (actual.size() < expected.size()) {
    return false;
  }

  if (actual.size() == expected.size()) {
    /* If the length is the same, exact match is required. */
    return actual == expected;
  }

  /* If the length grew, check the expected arg0. */
  int offset = actual.size() - expected.size();
  if (expected[0].find('/') != std::string::npos) {
    /* If it contained a slash, exact match is needed. */
    if (actual[offset] != expected[0]) {
      return false;
    }
  } else {
    /* If it didn't contain a slash, it needed to get prefixed with some path. */
    int expected_arg0_len = expected[0].length();
    int actual_arg0_len = actual[offset].length();
    /* Needs to be at least 1 longer. */
    if (actual_arg0_len <= expected_arg0_len) {
      return false;
    }
    /* See if the preceding character is a '/'. */
    if (actual[offset][actual_arg0_len - expected_arg0_len - 1] != '/') {
      return false;
    }
    /* See if the stuff after the '/' is the same. */
    if (actual[offset].compare(actual_arg0_len - expected_arg0_len,
                               expected_arg0_len, expected[0]) != 0) {
      return false;
    }
  }

  /* For the remaining parameters exact match is needed. */
  for (unsigned int i = 1; i < expected.size(); i++) {
    if (actual[offset + i] != expected[i]) {
      return false;
    }
  }
  return true;
}

std::shared_ptr<std::vector<std::shared_ptr<FileFD>>>
Process::pop_expected_child_fds(const std::vector<std::string>& argv,
                                LaunchType *launch_type_p,
                                int *type_flags_p,
                                const bool failed) {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "failed=%s", D(failed));

  std::shared_ptr<std::vector<std::shared_ptr<firebuild::FileFD>>> fds;
  if (expected_child_) {
    if (argv_matches_expectation(argv, expected_child_->argv())) {
      auto fds = expected_child_->fds();
      if (launch_type_p)
          *launch_type_p = expected_child_->launch_type();
      if (type_flags_p)
          *type_flags_p = expected_child_->type_flags();
      delete(expected_child_);
      expected_child_ = nullptr;
      return fds;
    } else {
      exec_point()->disable_shortcutting_bubble_up(
          "Unexpected system/popen/posix_spawn child appeared: " + d(argv) + " "
          "while waiting for: " + d(expected_child_));
    }
    delete(expected_child_);
    expected_child_ = nullptr;
  } else {
    exec_point()->disable_shortcutting_bubble_up("Unexpected system/popen/posix_spawn child " +
                                   std::string(failed ? "failed: " : "appeared: ") + d(argv));
  }
  return std::make_shared<std::vector<std::shared_ptr<FileFD>>>();
}

bool Process::any_child_not_finalized() {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "");

  if (exec_pending_ || pending_popen_child_) {
    return true;
  }
  if (exec_child() && exec_child()->state() != FB_PROC_FINALIZED) {
    /* The exec child is not yet finalized. We're not ready to finalize either. */
    return true;
  }

  for (auto fork_child : fork_children_) {
    if (fork_child->state_ != FB_PROC_FINALIZED) {
      return true;
    }
  }
  return false;
}

/**
 * Finalize the current process.
 */
void Process::do_finalize() {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "");

  /* Now we can ack the previous system()'s second message,
   * or a pending pclose() or wait*(). */
  if (on_finalized_ack_id_ != -1 && on_finalized_ack_fd_ != -1) {
    ack_msg(on_finalized_ack_fd_, on_finalized_ack_id_);
  }

  reset_file_fd_pipe_refs();

  assert_cmp(state(), ==, FB_PROC_TERMINATED);
  set_state(FB_PROC_FINALIZED);
}

/**
 * Finalize the current process if possible, and if successful then
 * bubble it up.
 */
void Process::maybe_finalize() {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "");

  if (state() != FB_PROC_TERMINATED) {
    return;
  }
  if (any_child_not_finalized()) {
    /* A child is yet to be finalized. We're not ready to finalize. */
    return;
  }
  drain_all_pipes();
  do_finalize();

  if (parent()) {
    parent()->maybe_finalize();
  }
}

void Process::finish() {
  TRACKX(FB_DEBUG_PROC, 1, 1, Process, this, "");

  if (FB_DEBUGGING(FB_DEBUG_PROC) && expected_child_) {
    FB_DEBUG(FB_DEBUG_PROC, "Expected system()/popen()/posix_spawn() children that did not appear"
                            " (e.g. posix_spawn() failed in the pre-exec or exec step):");
    FB_DEBUG(FB_DEBUG_PROC, "  " + d(expected_child_));
  }

  if (!exec_pending_ && !pending_popen_child_) {
    /* The pending child may show up and it needs to inherit the pipes. */
    reset_file_fd_pipe_refs();
  }

  set_state(FB_PROC_TERMINATED);
  maybe_finalize();
}

void Process::export2js_recurse(const unsigned int level, FILE* stream,
                                unsigned int *nodeid) {
  if (exec_child() != NULL) {
    exec_child_->export2js_recurse(level + 1, stream, nodeid);
  }
  for (auto& fork_child : fork_children_) {
    fork_child->export2js_recurse(level, stream, nodeid);
  }
}

/* Member debugging method. Not to be called directly, call the global d(obj_or_ptr) instead.
 * level is the nesting level of objects calling each other's d(), bigger means less info to print.
 * See #431 for design and rationale. */
std::string Process::d_internal(const int level) const {
  (void)level;  /* unused */
  return "{Process " + pid_and_exec_count() + "}";
}

Process::~Process() {
  TRACKX(FB_DEBUG_PROC, 1, 0, Process, this, "");

  free(pending_popen_fifo_);
}

/* Global debugging methods.
 * level is the nesting level of objects calling each other's d(), bigger means less info to print.
 * See #431 for design and rationale. */
std::string d(const Process& p, const int level) {
  return p.d_internal(level);
}
std::string d(const Process *p, const int level) {
  if (p) {
    return d(*p, level);
  } else {
    return "{Process NULL}";
  }
}


#if 0  /* unittests for canonicalize_path() */

/* Macro so that assert() reports useful line numbers. */
#define test(A, B) { \
  char *str = strdup(A); \
  canonicalize_path(str); \
  if (strcmp(str, B)) { \
    fprintf(stderr, "Error:  input: %s\n", A); \
    fprintf(stderr, "     expected: %s\n", B); \
    fprintf(stderr, "  got instead: %s\n", str); \
  } \
}

int main() {
  test("/", "/");
  test("/etc/hosts", "/etc/hosts");
  test("/usr/include/vte-2.91/vte/vteterminal.h", "/usr/include/vte-2.91/vte/vteterminal.h");
  test("/usr/bin/", "/usr/bin");
  test("/usr/bin/.", "/usr/bin");
  test("/usr/./bin", "/usr/bin");
  test("/./usr/bin", "/usr/bin");
  test("//", "/");
  test("", "");
  test(".", ".");
  test("/.", "/");
  test("./", ".");
  test("/./././", "/");
  test("./././.", ".");
  test("//foo//bar//", "/foo/bar");
  test("/././foo/././bar/././", "/foo/bar");
  test("///.//././/.///foo//.//bar//.", "/foo/bar");
  test("////foo/../bar", "/foo/../bar");
  test("/foo/bar/../../../../../", "/foo/bar/../../../../..");
  test("/.foo/.bar/..quux", "/.foo/.bar/..quux");
  test("foo", "foo");
  test("foo/bar", "foo/bar");
  test("././foo/./bar/./.", "foo/bar");
}

#endif  /* unittests for canonicalize_path() */


}  // namespace firebuild
