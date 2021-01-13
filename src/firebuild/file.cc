/* Copyright (c) 2014 Balint Reczey <balint@balintreczey.hu> */
/* This file is an unpublished work. All rights reserved. */


#include "firebuild/file.h"

#include <libgen.h>
#include <sys/stat.h>

#include <cstring>

#include "firebuild/debug.h"
#include "firebuild/hash_cache.h"

namespace firebuild {

File::File(const FileName* p)
    : mtimes_(), path_(p), exists_(false), hash_() {
  TRACK(FB_DEBUG_PROC, "this=%s, path=%s", D(this), D(p));
}


int File::set_hash() {
  TRACK(FB_DEBUG_PROC, "this=%s", D(this));

  return hash_cache->get_hash(path_, &hash_);
}

int File::update() {
  TRACK(FB_DEBUG_PROC, "this=%s", D(this));

  if (!this->set_hash()) {
    return -1;
  }

  unsigned int i = 0;
  struct stat s;
  char *tmp_path = strdup(path_->c_str());
  if (lstat(tmp_path, &s) == -1) {
    perror("lstat");
    free(tmp_path);
    return -1;
  } else {
    if (mtimes_.size() <= i) {
      mtimes_.resize(i+1);
    }
    mtimes_[i] = s.st_mtim;
  }
  // dirname may modify path and return dir pointing to a statically
  // allocated buffer. This is how we ended up having this complicated code
  char *dir;
  while (true) {
    i++;
    dir = dirname(tmp_path);
    /* XXX lstat is intercepted */
    if (lstat(dir, &s) == -1) {
      perror("lstat");
      free(tmp_path);
      return -1;
    } else {
      if (mtimes_.size() <= i) {
        mtimes_.resize(i+1);
      }
      mtimes_[i] = s.st_mtim;
      // https://pubs.opengroup.org/onlinepubs/000095399/basedefs/xbd_chap04.html#tag_04_11
      // "A pathname that begins with two successive slashes may be interpreted
      // in an implementation-defined manner [...]"
      if ((strcmp(".", dir) == 0) || (strcmp("/", dir) == 0) || (strcmp("//", dir) == 0)) {
        break;
      } else {
        char * next_path = strdup(dir);
        free(tmp_path);
        tmp_path = next_path;
      }
    }
  }
  // we could resize the vector here, but the number of dirs in the path
  // can't change over time
  free(tmp_path);
  return 0;
}

#ifndef timespeccmp
#define timespeccmp(a, b, CMP)                                          \
  ((a)->tv_sec == (b)->tv_sec ? (a)->tv_nsec CMP (b)->tv_nsec : (a)->tv_sec CMP (b)->tv_sec)  /* NOLINT */
#endif

int File::is_changed() {
  TRACK(FB_DEBUG_PROC, "this=%s", D(this));

  int i = 0;
  char *tmp_path = strdup(path_->c_str());
  struct stat s;

  if (lstat(tmp_path, &s) == -1) {
    perror("lstat");
    free(tmp_path);
    return -1;
  } else {
    if (!timespeccmp(&mtimes_[i], &s.st_mtim, ==)) {
      free(tmp_path);
      return 1;
    }
  }
  // dirname may modify path and return dir pointing to a statically
  // allocated buffer. This is how we ended up having this complicated code
  char *dir;
  while (true) {
    i++;
    dir = dirname(tmp_path);
    if (lstat(dir, &s) == -1) {
      perror("lstat");
      free(tmp_path);
      return -1;
    } else {
      if (!timespeccmp(&mtimes_[i], &s.st_mtim, ==)) {
        free(tmp_path);
        return 1;
      }
      // https://pubs.opengroup.org/onlinepubs/000095399/basedefs/xbd_chap04.html#tag_04_11
      // "A pathname that begins with two successive slashes may be interpreted
      // in an implementation-defined manner [...]"
      if ((strcmp(".", dir) == 0) || (strcmp("/", dir) == 0) || (strcmp("//", dir) == 0)) {
        break;
      } else {
        char * next_path = strdup(dir);
        free(tmp_path);
        tmp_path = next_path;
      }
    }
  }
  // we could resize the vector here, but the number of dirs in the path
  // can't change over time
  free(tmp_path);
  return 0;
}

/* Global debugging methods.
 * level is the nesting level of objects calling each other's d(), bigger means less info to print.
 * See #431 for design and rationale. */
std::string d(const File& f, const int level) {
  (void)level;  /* unused */
  return std::string("[File path=") + d(f.path()) + ", exists=" + d(f.exists()) +
      (f.exists() ? ", hash=" + d(f.hash()) : "");
}
std::string d(const File *f, const int level) {
  if (f) {
    return d(*f, level);
  } else {
    return "[File NULL]";
  }
}

}  // namespace firebuild
