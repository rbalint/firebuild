
#ifndef FB_EXECED_PROCESS_H
#define FB_EXECED_PROCESS_H

#include "Process.h"

#include "fb-messages.pb.h"

using namespace std;

namespace firebuild 
{
  
class ExecedProcess : public Process
{
 public:
  Process *exec_parent = NULL;
  long int sum_utime_m = 0; /**< Sum of user time in milliseconds for all forked
                               but not exec()-ed children */
  long int sum_stime_m = 0; /**< Sum of system time in milliseconds for all
                               forked but not exec()-ed children */
  string cwd;
  vector<string> args;
  set<string> env_vars;
  string executable;
  ExecedProcess (firebuild::msg::ShortCutProcessQuery const & scpq);
  void exit_result (int status, long int utime_m, long int stime_m);
};


}
#endif