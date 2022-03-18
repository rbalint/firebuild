/* Copyright (c) 2020 Interri Kft. */
/* This file is an unpublished work. All rights reserved. */

/**
 * FileUsage describes, for one particular Process and one particular
 * filename, the inital and final contents found at the given location
 * with as much accuracy as it matters to us.
 *
 * E.g. if the Process potentially reads from the file then its original
 * hash is computed and stored here, but if the Process does not read
 * the contents then it is not stored. Similarly, it's recorded whether
 * the process potentially modified the file.
 */

#include "firebuild/file_usage.h"

#include <sys/stat.h>

#include <unordered_set>

#include "common/firebuild_common.h"
#include "firebuild/debug.h"
#include "firebuild/hash.h"
#include "firebuild/hash_cache.h"

namespace firebuild {

std::unordered_set<FileUsage, FileUsageHasher>* FileUsage::db_;
const FileUsage* FileUsage::no_hash_not_written_states_[INITIAL_STATE_MAX + 1];
const FileUsage* FileUsage::no_hash_written_states_[INITIAL_STATE_MAX + 1];


FileUsage::DbInitializer::DbInitializer() {
  db_ = new std::unordered_set<FileUsage, FileUsageHasher>();
  for (int i = 0; i <= INITIAL_STATE_MAX; i++) {
    const FileUsage fu(int_to_initial_state(i), Hash());
    no_hash_not_written_states_[i] = &*db_->insert(fu).first;
  }
  for (int i = 0; i <= INITIAL_STATE_MAX; i++) {
    const FileUsage fu(int_to_initial_state(i), Hash(), true);
    no_hash_written_states_[i] = &*db_->insert(fu).first;
  }
}

FileUsage::DbInitializer FileUsage::db_initializer_;

bool operator==(const FileUsage& lhs, const FileUsage& rhs) {
  return (lhs.initial_state_ == rhs.initial_state_
          && lhs.initial_hash_ == rhs.initial_hash_
          // && lhs.stated_ == rhs.stated_
          // TODO(rbalint) no operator==()
          // && lhs.initial_stat_ == rhs.initial_stat_
          && lhs.written_ == rhs.written_
          // && lhs.stat_changed_ == rhs.stat_changed_
          && lhs.unknown_err_ == rhs.unknown_err_);
          }

const FileUsage* FileUsage::Get(const FileUsage& candidate) {
  auto it = db_->find(candidate);
  if (it != db_->end()) {
    return &*it;
  } else {
    /* Not found, add a copy to the set. */
    return &*db_->insert(candidate).first;
  }
}

/**
 * Merge the other FileUsage object into this one.
 *
 * "this" describes the older event(s) which happened to a file, and
 * "that" describes the new one. Sometimes the file usages to merge are conflicting, like
 * a directory was expected to not exist, then it is expected to exist without creating it in
 * the meantime. In those cases the return is nullptr and it should disable shortcutting of the
 * process and its ancestors.
 * @return pointer to the merge result, it may be different from either "this" and "that" and
 *        can be nullptr
 */
const FileUsage* FileUsage::merge(const FileUsage* that) const {
  TRACKX(FB_DEBUG_PROC, 1, 1, FileUsage, this, "other=%s", D(that));

  if (*this == *that) {
    return this;
  }

  FileUsage tmp = *this;

  bool changed = false;
  switch (initial_state_) {
    case DONTKNOW: {
      if (initial_state_ != that->initial_state_) {
        tmp.initial_state_ = that->initial_state_;
        changed = true;
      }
      if (that->initial_hash_known_ && initial_hash_ != that->initial_hash_) {
        tmp.initial_hash_ = that->initial_hash_;
        tmp.initial_hash_known_ = true;
        changed = true;
      }
      break;
    }
    case NOTEXIST:
    case NOTEXIST_OR_ISREG:
    case NOTEXIST_OR_ISREG_EMPTY: {
      if (!written_ && !that->written_ && that->initial_state_ == ISDIR) {
        return nullptr;
      }
      break;
    }
    case ISREG: {
      if (!written_ && !that->written_ && !initial_hash_known_ && that->initial_hash_known_) {
        tmp.initial_hash_ = that->initial_hash_;
        tmp.initial_hash_known_ = true;
        changed = true;
      }
      break;
    }
    case ISDIR: {
      if (!written_ && !that->written_ && !initial_hash_known_ && that->initial_hash_known_) {
        tmp.initial_hash_ = that->initial_hash_;
        tmp.initial_hash_known_ = true;
        changed = true;
      }
      break;
    }
  }

  if (!written_ && that->written_) {
    changed = true;
  }
  tmp.written_ = written_ || that->written_;

  if (!changed) {
    return this;
  } else {
    if (tmp == *that) {
      return that;
    } else {
      return FileUsage::Get(tmp);
    }
  }
}

/**
 * Based on the parameters and return value of an open() or similar
 * call, updates the current FileUsage object to reflect the known
 * initial state of the file.
 *
 * If do_read is false, it's assumed that the file was already opened
 * (or at least attempted to) for reading, and as such the initial
 * values are not changed; only the written property is updated.
 */
bool FileUsage::update_from_open_params(const FileName* filename,
                                        FileAction action, int flags, int err,
                                        bool do_read, bool* hash_changed) {
  TRACKX(FB_DEBUG_PROC, 1, 1, FileUsage, this,
         "filename=%s, action=%s, flags=%d, err=%d, do_read=%s",
         D(filename), file_action_to_string(action), flags, err, D(do_read));

  if (!do_read) {
    if (is_write(flags) && !err) {
      written_ = true;
    }
    return true;
  }

  if (!err) {
    if (action == FILE_ACTION_OPEN) {
      if (is_write(flags)) {
        /* If successfully opened for writing:
         *
         *     trunc   creat   excl
         * A     +       -            => prev file must exist, contents don't matter
         * B     +       +       -    => prev file doesn't matter
         * C     +       +       +    => prev file mustn't exist
         * D     -       -            => prev file must exist, contents preserved and matter
         * E     -       +       -    => contents preserved (or new empty) and matter
         * F     -       +       +    => prev file mustn't exist
         */
        if ((flags & O_CREAT) && (flags & O_EXCL)) {
          /* C+F: If an exclusive new file was created, take a note that
           * the file didn't exist previously. */
          initial_state_ = NOTEXIST;
        } else if (flags & O_TRUNC) {
          if (!(flags & O_CREAT)) {
            /* A: What a nasty combo! We must take a note that the file
             * existed, but don't care about its previous contents (also
             * it's too late now to figure that out). */
            initial_state_ = ISREG;
          } else {
            /* B: The old contents could have been any regular file, or
             * even no such file (but not e.g. a directory). */
            initial_state_ = NOTEXIST_OR_ISREG;
          }
        } else {
          if (!(flags & O_CREAT)) {
            /* D: Contents unchanged. Need to checksum the file. */
            if (!hash_cache->get_hash(filename, &initial_hash_)) {
              unknown_err_ = errno;
              return false;
            }
            initial_hash_known_ = true;
            initial_state_ = ISREG;
            *hash_changed  = true;
          } else {
            /* E: Another nasty combo. We can't distinguish a newly
             * created empty file from a previously empty one. If the file
             * is non-empty, we need to store its hash. */
            struct stat st;
            if (stat(filename->c_str(), &st) == -1) {
              unknown_err_ = errno;
              return false;
            }
            if (st.st_size > 0) {
              if (!hash_cache->get_hash(filename, &initial_hash_)) {
                unknown_err_ = errno;
                return false;
              }
              initial_hash_known_ = true;
              initial_state_ = ISREG;
              *hash_changed  = true;
            } else {
              initial_state_ = NOTEXIST_OR_ISREG_EMPTY;
            }
          }
        }
        written_ = true;
      } else {
        /* The file or directory was successfully opened for reading only.
         * Note that a plain open() can open a directory for reading, even without O_DIRECTORY. */
        bool is_dir;
        if (!hash_cache->get_hash(filename, &initial_hash_, &is_dir)) {
          unknown_err_ = errno;
          return false;
        }
        initial_hash_known_ = true;
        initial_state_ = is_dir ? ISDIR : ISREG;
      }
    } else if (action == FILE_ACTION_MKDIR) {
      initial_state_ = NOTEXIST;
      written_ = true;
    } else if (action == FILE_ACTION_STATFILE) {
      /* The file exists. */
      // TODO(rbalint) handle size info, and possibly other info, too
      initial_state_ = ISREG;
    } else if (action == FILE_ACTION_STATDIR) {
      /* The directory exists. */
      initial_state_ = ISDIR;
    }
  } else /* if (err) */ {
    if (action == FILE_ACTION_OPEN) {
      /* The attempt to open failed. */
      if (is_write(flags)) {
        /* ENOENT and ENOTDIR are handled in the caller. */
        assert_cmp(err, !=, ENOENT);
        assert_cmp(err, !=, ENOTDIR);
        /* We don't support other errors such as permission denied. */
        unknown_err_ = err;
        return false;
      } else {
        /* Opening for reading failed. */
        if (err == ENOENT) {
          initial_state_ = NOTEXIST;
        } else {
          /* ENOTDIR is handled in the caller. */
          assert_cmp(err, !=, ENOTDIR);
          /* We don't support other errors such as permission denied. */
          unknown_err_ = err;
          return false;
        }
      }
    } else if (action == FILE_ACTION_MKDIR) {
      if (err == EEXIST) {
        /* The directory already exists. It may not be a directory, but in that case process inputs
         * will not match either. */
        initial_state_ = ISDIR;
      } else {
        /* We don't support other errors such as permission denied. */
        unknown_err_ = err;
        return false;
      }
    } else if (action == FILE_ACTION_STATFILE) {
      /* The file does not exist. */
      initial_state_ = NOTEXIST;
    } else if (action == FILE_ACTION_STATDIR) {
      assert(0 && "If file does not exist stat() can't tell that it is a directory");
      initial_state_ = NOTEXIST;
    }
  }
  return true;
}

bool file_file_usage_cmp(const file_file_usage& lhs, const file_file_usage& rhs) {
  return strcmp(lhs.file->c_str(), rhs.file->c_str()) < 0;
}

/* Global debugging methods.
 * level is the nesting level of objects calling each other's d(), bigger means less info to print.
 * See #431 for design and rationale. */
std::string d(const FileUsage& fu, const int level) {
  (void)level;  /* unused */
  return std::string("{FileUsage initial_state=") +
      file_initial_state_to_string(fu.initial_state()) +
      (fu.initial_hash_known() ?
          ", hash=" + d(fu.initial_hash()) : "") +
      ", written=" + d(fu.written()) + "}";
}
std::string d(const FileUsage *fu, const int level) {
  if (fu) {
    return d(*fu, level);
  } else {
    return "{FileUsage NULL}";
  }
}

const char *file_initial_state_to_string(FileInitialState state) {
  switch (state) {
    case DONTKNOW:
      return "dontknow";
    case NOTEXIST:
      return "notexist";
    case NOTEXIST_OR_ISREG_EMPTY:
      return "notexist_or_isreg_empty";
    case NOTEXIST_OR_ISREG:
      return "notexist_or_isreg";
    case ISREG:
      return "isreg";
    case ISDIR:
      return "isdir";
    default:
      assert(0 && "unknown state");
      return "UNKNOWN";
  }
}

const char *file_action_to_string(FileAction action) {
  switch (action) {
    case FILE_ACTION_OPEN:
      return "open";
    case FILE_ACTION_MKDIR:
      return "mkdir";
    case FILE_ACTION_STATFILE:
      return "stat (on file)";
    case FILE_ACTION_STATDIR:
      return "stat (on dir)";
    default:
      assert(0 && "unknown action");
      return "UNKNOWN";
  }
}

}  /* namespace firebuild */
