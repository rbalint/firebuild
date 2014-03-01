/* Copyright (c) 2014 Balint Reczey <balint@balintreczey.hu> */
/* This file is an unpublished work. All rights reserved. */


#ifndef FIREBUILD_PROCESSTREE_H_
#define FIREBUILD_PROCESSTREE_H_

#include <map>
#include <set>
#include <string>
#include <unordered_map>


#include "firebuild/Process.h"
#include "firebuild/ExecedProcess.h"
#include "firebuild/ForkedProcess.h"
#include "firebuild/cxx_lang_utils.h"

namespace firebuild {

struct subcmd_prof {
  int64_t sum_aggr_time;
  int64_t count;
  bool recursed;
};

struct cmd_prof {
  int64_t aggr_time;
  int64_t cmd_time;
  /**  {time_m, count} */
  std::unordered_map<std::string, subcmd_prof> subcmds;
};

class ProcessTree {
 public:
  ProcessTree()
     : sock2proc_(), fb_pid2proc_(), pid2proc_(), cmd_profs_()
  {}
  ~ProcessTree();

  void insert(Process *p, const int sock);
  void insert(ExecedProcess *p, const int sock);
  void insert(ForkedProcess *p, const int sock);
  void exit(Process *p, const int sock);
  static int64_t sum_rusage_recurse(Process *p);
  void export2js(FILE* stream);
  void export_profile2dot(FILE* stream);
  ExecedProcess* root() {return root_;}
  std::unordered_map<int, Process*>& sock2proc() {return sock2proc_;}
  std::unordered_map<int, Process*>& fb_pid2proc() {return fb_pid2proc_;}
  std::unordered_map<int, Process*>& pid2proc() {return pid2proc_;}

 private:
  ExecedProcess *root_ = NULL;
  std::unordered_map<int, Process*> sock2proc_;
  std::unordered_map<int, Process*> fb_pid2proc_;
  std::unordered_map<int, Process*> pid2proc_;
  /**
   * Profile is aggregated by command name (argv[0]).
   * For each command (C) we store the cumulated CPU time in milliseconds
   * (system + user time), and count the invocations of each other command
   * by C. */
  std::unordered_map<std::string, cmd_prof> cmd_profs_;
  void profile_collect_cmds(const Process &p,
                            std::unordered_map<std::string, subcmd_prof> *cmds,
                            std::set<std::string> *ancestors);
  void build_profile(const Process &p, std::set<std::string> *ancestors);

  DISALLOW_COPY_AND_ASSIGN(ProcessTree);
};

}  // namespace firebuild
#endif  // FIREBUILD_PROCESSTREE_H_