/* Copyright (c) 2014 Balint Reczey <balint@balintreczey.hu> */
/* This file is an unpublished work. All rights reserved. */


#ifndef FIREBUILD_PROCESS_H_
#define FIREBUILD_PROCESS_H_

#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "firebuild/FileFD.h"
#include "firebuild/FileUsage.h"
#include "firebuild/cxx_lang_utils.h"

namespace firebuild {

typedef enum {FB_PROC_RUNNING,   ///< process is running
              FB_PROC_EXECED,    ///< process finished running by exec()
              FB_PROC_FINISHED,  ///< process exited cleanly
} process_state;

/**
 * Firebuild's model of a UNIX (TODO: Windows) process' period of life.
 * Generally it represents the period starting with a successful exec() or fork()
 * and finished by an other successful exec() or exit().
 *
 * Note the difference from UNIX's process concept. In Unix a process can call
 * exec() successfully several time preserving process id and some process
 * attributes, while replacing the process image. Those periods are handled as
 * different (but related) Processes in FireBuild.
 */
class Process {
 public:
  Process(int pid, int ppid, const std::string &wd);
  virtual ~Process();
  bool operator == (Process const & p) const;
  virtual bool exec_started() const {return false;};
  int state() const {return state_;}
  void set_state(process_state s) {state_ = s;}
  int fb_pid() {return fb_pid_;}
  int pid() const {return pid_;}
  int ppid() const {return ppid_;}
  int exit_status() const {return exit_status_;}
  void set_exit_status(const int e) {exit_status_ = e;}
  std::string& wd() {return wd_;}
  void set_wd(const std::string &d);
  virtual std::set<std::string>& libs() = 0;
  virtual const std::unordered_map<std::string, FileUsage*>&
      file_usages() const = 0;
  virtual std::unordered_map<std::string, FileUsage*>& file_usages() = 0;
  int64_t utime_m() const {return utime_m_;}
  void set_utime_m(int64_t t) {utime_m_ = t;}
  int64_t stime_m() const {return stime_m_;}
  void set_stime_m(int64_t t) {stime_m_ = t;}
  int64_t aggr_time() const {return aggr_time_;}
  void set_aggr_time(int64_t t) {aggr_time_ = t;}
  void set_exec_child(Process *p) {exec_child_ = p;}
  Process* exec_child() const {return exec_child_;}
  std::vector<Process*>& children() {return children_;}
  const std::vector<Process*>& children() const {return children_;}
  void update_rusage(int64_t utime_m, int64_t stime_m);
  void sum_rusage(int64_t *sum_utime_m, int64_t *sum_stime_m);
  virtual void exit_result(int status, int64_t utime_m, int64_t stime_m);
  int open_file(const std::string &name, const int flags, const mode_t mode,
                const int fd, const bool created = false, const int error = 0);
  /**
   * Handle file closure in in monitored process
   * @param fd file descriptor to close
   * @param error error code of close()
   */
  int close_file(const int fd, const int error = 0);

  /**
   * Fail to change to a working directory
   */
  virtual void fail_wd(const std::string &d) = 0;

  /**
   * Record visited working directory
   */
  virtual void add_wd(const std::string &d) = 0;

  /** Propagate exit status upward through exec()-ed processes */
  virtual void propagate_exit_status(const int status) = 0;

  virtual int64_t sum_rusage_recurse();

  virtual void export2js_recurse(const unsigned int level, FILE* stream,
                                 unsigned int *nodeid);

 private:
  process_state state_ :2;
  bool can_shortcut_:1;
  int fb_pid_;       ///< internal FireBuild id for the process
  int pid_;          ///< UNIX pid
  int ppid_;         ///< UNIX ppid
  int exit_status_;  ///< exit status, valid if state = FB_PROC_FINISHED
  std::string wd_;  ///< Current working directory
  std::vector<FileFD*> fds_;  ///< Active file descriptors
  int64_t utime_m_;  ///< user time in milliseconds as reported by getrusage()
  /// system time in milliseconds as reported by getrusage()
  int64_t stime_m_;
  /** Sum of user and system time in milliseconds for all forked and exec()-ed
      children */
  int64_t aggr_time_ = 0;
  std::vector<Process*> children_;  ///< children of the process
  Process * exec_child_;
  DISALLOW_COPY_AND_ASSIGN(Process);
};

inline bool Process::operator == (Process const & p) const {
  return (p.fb_pid_ == fb_pid_);
}

}  // namespace firebuild
#endif  // FIREBUILD_PROCESS_H_