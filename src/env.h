#ifndef FIREBUILD_ENV_H
#define FIREBUILD_ENV_H

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * Get process's arguments and environment
 */
void get_argv_env(char *** argv, char ***env);

#ifdef  __cplusplus
}
#endif

#endif
