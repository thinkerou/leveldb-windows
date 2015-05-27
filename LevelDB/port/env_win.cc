// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <deque>

#pragma warning(disable:4244)

#include <windows.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <io.h>

#include "leveldb/env.h"
#include "leveldb/slice.h"

#include "port_chromium.h"
#include "win_logger.h"

#include <list>
#include <process.h>

#include "util/file_misc.h"

#pragma comment(lib, "Winmm.lib")

namespace leveldb {

namespace {

class WinSequentialFile: public SequentialFile {
 public:
  WinSequentialFile(const std::string& fname, FILE* f)
      : filename_(fname), file_(f) { }
  virtual ~WinSequentialFile() { fclose(file_); }

  virtual Status Read(size_t n, Slice* result, char* scratch) {
    Status s;
    size_t r = _fread_nolock(scratch, 1, n, file_);
    *result = Slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // We leave status as ok if we hit the end of the file
      } else {
        // A partial read with an error: return a non-ok status
        s = Status::IOError(filename_, strerror(errno));
      }
    }
    return s;
  }

  virtual Status Skip(uint64_t n) {
    if (fseek(file_, n, SEEK_CUR)) {
      return Status::IOError(filename_, strerror(errno));
    }
    return Status::OK();
  }

 private:
  std::string filename_;
  FILE* file_;
};

class WinRandomAccessFile: public RandomAccessFile {
 public:
  WinRandomAccessFile(const std::string& fname, HANDLE file)
      : filename_(fname), file_(file) { }
  virtual ~WinRandomAccessFile() { CloseHandle(file_); }

  virtual Status Read(uint64_t offset, size_t n, Slice* result,
            char* scratch) const {
    Status s;

    int r = -1;
    LARGE_INTEGER offset_li;
    offset_li.QuadPart = offset;

    OVERLAPPED overlapped = {0};
    overlapped.Offset = offset_li.LowPart;
    overlapped.OffsetHigh = offset_li.HighPart;

    DWORD bytes_read;
    if (::ReadFile(file_, scratch, n, &bytes_read, &overlapped) != 0)
      r =  bytes_read;
    else if (ERROR_HANDLE_EOF == GetLastError())
      r = 0;
    *result = Slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // An error: return a non-ok status
      s = Status::IOError(filename_, strerror(errno));
    }
    return s;
  }

 private:
  std::string filename_;
  HANDLE file_;
};

// We preallocate up to an extra megabyte and use memcpy to append new
// data to the file.  This is safe since we either properly close the
// file before reading from it, or for log files, the reading code
// knows enough to skip zero suffixes.

class WinWritableFile : public WritableFile {
 public:
  explicit WinWritableFile(const std::string& fname, FILE* f)
      : fname_(fname), file_(f) {
  }

  virtual ~WinWritableFile() {
    if (file_ != NULL) {
      // Ignoring any potential errors
      fclose(file_);
    }
  }

  virtual Status Append(const Slice& data) {
    size_t r = _fwrite_nolock(data.data(), 1, data.size(), file_);
    if (r != data.size()) {
      return Status::IOError(fname_ + " Append", "cannot write");
    }
    return Status::OK();
  }

  virtual Status Close() {
    Status result;
    if (fclose(file_) != 0) {
      result = Status::IOError(fname_ + " close", "cannot close");
    }
    file_ = NULL;
    return result;
  }

  virtual Status Flush() {
    Status result;
    if (_fflush_nolock(file_)) {
      result = Status::IOError(fname_ + " flush", "cannot flush");
    }
    return result;
  }

  virtual Status Sync() {
    Status result;
    int error = 0;
    if (_fflush_nolock(file_))
      error = errno;
    // Sync even if fflush gave an error; perhaps the data actually got out,
    // even though something went wrong.
    if (_commit(_fileno(file_)) && !error)
      error = errno;
    // Report the first error we found.
    if (error) {
      result = Status::IOError(fname_ + " sync", "cannot sync");
    }
    return result;
  }

 private:
  std::string fname_;
  FILE* file_;
};

class WinFileLock : public FileLock {
 public:
  HANDLE file_;
};

const int iSleep = 10;
const int iRetryCount = 30;

class WinEnv : public Env {
 public:
  WinEnv() : bgsignal_(&mu_), started_bgthread_(false) { }
  virtual ~WinEnv() {
    fprintf(stderr, "Destroying Env::Default()\n");
    exit(1);
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) {
    FILE* f = _wfopen(AToW(fname).c_str(), L"rb");
    if (f == NULL) {
      *result = NULL;
      return Status::IOError(fname, strerror(GetLastError()));
    } else {
      *result = new WinSequentialFile(fname, f);
      return Status::OK();
    }
  }

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) {
    DWORD disposition = OPEN_EXISTING;
    DWORD access = GENERIC_READ;
    DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
    HANDLE file = ::CreateFile(AToW(fname).c_str(), access, sharing, NULL, disposition, 0, NULL);
    if (file != INVALID_HANDLE_VALUE) {
      *result = new WinRandomAccessFile(fname, file);
      return Status::OK();
    }
    *result = NULL;
    return Status::IOError(fname, strerror(GetLastError()));
  }

  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) {
    *result = NULL;
    FILE* f = _wfopen(AToW(fname).c_str(), L"wb");
    if (f == NULL) {
      return Status::IOError(fname, strerror(GetLastError()));
    } else {
      *result = new WinWritableFile(fname, f);
      return Status::OK();
    }
  }

  virtual bool FileExists(const std::string& fname) {
    return FileMisc::IsFileExist(AToW(fname).c_str(), true);
  }

  virtual Status GetChildren(const std::string& dir,
               std::vector<std::string>* result) {
    result->clear();

    EnumResultList ls;
    FileMisc::EnumDirectory(AToW(dir + "\\*").c_str(), ls, ED_FILE);
    for (EnumResultList::iterator iter = ls.begin(); iter != ls.end(); iter++) {
      result->push_back(WToA(iter->cFileName));
    }
    // TODO(jorlow): Unfortunately, the FileEnumerator swallows errors, so
    //               we'll always return OK. Maybe manually check for error
    //               conditions like the file not existing?
    return Status::OK();
  }

  virtual Status DeleteFile(const std::string& fname) {
    Status result;
    std::wstring uni_fname = AToW(fname);
    // TODO(jorlow): Should we assert this is a file?
    if (FileMisc::IsFileExist(uni_fname.c_str())) {
      if (!::DeleteFile(uni_fname.c_str()))
        result = Status::IOError(fname, "cannot delete file");
    }
    return result;
  }

  virtual Status CreateDir(const std::string& name) {
    Status result;

    for (int i = 0; i < iRetryCount; i++)
    {
        if (FileMisc::CreateDirectory(AToW(name).c_str()))
            return result;
        ::Sleep(iSleep);
    }
    
    return Status::IOError(name, "cannot create directory");
  };

  virtual Status DeleteDir(const std::string& name) {
    Status result;
    std::wstring uni_name = AToW(name);
    // TODO(jorlow): Should we assert this is a directory?
    if (FileMisc::IsDirectory(uni_name.c_str())) {
      if (!::FileMisc::RemoveDirectory(uni_name.c_str()))
        result = Status::IOError(name, "cannot delete directory");
    }
    return result;
  };

  virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
    Status s;
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesEx(AToW(fname).c_str(), GetFileExInfoStandard, &attr)) {
      *size = 0;
      s = Status::IOError(fname, "cannot get file size");
    } else {
      ULARGE_INTEGER ui;
      ui.HighPart = attr.nFileSizeHigh;
      ui.LowPart = attr.nFileSizeLow;
      *size = static_cast<uint64_t>(ui.QuadPart);
    }
    return s;
  }

  virtual Status RenameFile(const std::string& src, const std::string& target) {
    Status result;
    std::wstring uni_src = AToW(src), uni_target = AToW(target);
    if (!FileMisc::IsFileExist(uni_src.c_str(), true))
      return result;

    for (int i = 0; i < iRetryCount; i++)
    {
        if (::MoveFile(uni_src.c_str(), uni_target.c_str()) != 0)
            return result;
        else
            ::DeleteFile(uni_target.c_str());
        ::Sleep(iSleep);
    }
    
    return Status::IOError(target, "can't replace file");

    // Try a simple move first.  It will only succeed when |to_path| doesn't
    // already exist.
//    if (::MoveFile(uni_src.c_str(), uni_target.c_str()))
//      return result;
    // Try the full-blown replace if the move fails, as ReplaceFile will only
    // succeed when |to_path| does exist. When writing to a network share, we may
    // not be able to change the ACLs. Ignore ACL errors then
    // (REPLACEFILE_IGNORE_MERGE_ERRORS).
//     if (::ReplaceFile(uni_target.c_str(), uni_src.c_str(), NULL, 
//                       REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
//       return result;
//     }
//     char buf[512];
//     snprintf(buf,
//              sizeof(buf),
//              "Could not rename file: %s", strerror(GetLastError()));
//     return Status::IOError(src, buf);
  }

  virtual Status LockFile(const std::string& fname, FileLock** lock) {
    *lock = NULL;
    Status result;
    DWORD disposition = OPEN_ALWAYS;
    DWORD access = GENERIC_READ | GENERIC_WRITE;
    HANDLE file = ::CreateFile(AToW(fname).c_str(), access, 0, NULL, disposition, 0, NULL);
    if (file == INVALID_HANDLE_VALUE) {
      result = Status::IOError("lock " + fname, strerror(GetLastError()));
    } else {
      WinFileLock* my_lock = new WinFileLock;
      my_lock->file_ = file;
      *lock = my_lock;
    }
    return result;
  }

  virtual Status UnlockFile(FileLock* lock) {
    WinFileLock * my_lock = reinterpret_cast<WinFileLock *>(lock);
    Status result;
    if (!CloseHandle(my_lock->file_)) {
      result = Status::IOError("unlock", strerror(GetLastError()));
    }
    delete my_lock;
    return result;
  }

  virtual void Schedule(void (*function)(void*), void* arg);
  virtual void StartThread(void (*function)(void* arg), void* arg);

  virtual Status GetTestDirectory(std::string* result) {
    mu_.Lock();
    if (test_directory_.empty())  {
      wchar_t temp_name[MAX_PATH + 1];
      DWORD path_len = ::GetTempPath(MAX_PATH, temp_name);
      if (path_len > 0 && path_len < MAX_PATH) {
        std::wstring dir(temp_name);
        if (GetTempFileName(dir.c_str(), L"", 0, temp_name)) {
          wchar_t long_temp_name[MAX_PATH + 1];
          DWORD long_name_len = GetLongPathName(temp_name, long_temp_name, MAX_PATH);
          // GetLongPathName() failed, but we still have a temporary file.
          if (long_name_len > 0 && long_name_len < MAX_PATH)
            test_directory_ = std::wstring(long_temp_name, long_name_len);
          else
            test_directory_ = temp_name;
        }
      }
    }
    if (!test_directory_.empty())
      *result = WToA(test_directory_);
    mu_.Unlock();
    if (result->empty())
      return Status::IOError("temp directory", "Could not create temp directory");
    else
    {
        DeleteFileA(result->c_str());
        return Status::OK();
    }
  }

  virtual Status NewLogger(const std::string& fname, Logger** result) {
    FILE* f = _wfopen(AToW(fname).c_str(), L"w");
    if (f == NULL) {
      *result = NULL;
      return Status::IOError(fname, strerror(errno));
    } else {
      *result = new WinLogger(f);
      return Status::OK();
    }
  }

  virtual uint64_t NowMicros() {
    // Accumulation of time lost due to rollover (in milliseconds).
    static uint64_t rollover_ms = 0;
    // The last timeGetTime value we saw, to detect rollover.
    static DWORD last_seen_now = 0;
    DWORD now = timeGetTime();
    if (now < last_seen_now)
      rollover_ms += 0x100000000I64;  // ~49.7 days.
    last_seen_now = now;
    return (now + rollover_ms) * 1000; // 1000 microseconds as 1 millisecond.
  }

  virtual void SleepForMicroseconds(int micros) {
    uint64_t end = NowMicros() + micros;
    uint64_t now;
    while ((now = NowMicros()) < end)
      ::Sleep((end - now + 999) / 1000); // Round up to 1 millisecond.
  }

 private:
  // BGThread() is the body of the background thread
  void BGThread();

  static void BGThreadWrapper(void* arg) {
    reinterpret_cast<WinEnv*>(arg)->BGThread();
  }

  // WinNT use unicode as base file system.
  std::wstring test_directory_;

  leveldb::port::Mutex mu_;
  leveldb::port::CondVar bgsignal_;
  bool started_bgthread_;

  // Entry per Schedule() call
  struct BGItem {
    void* arg;
    void (*function)(void*);
  };
  typedef std::deque<BGItem> BGQueue;
  BGQueue queue_;
};

void WinEnv::Schedule(void (*function)(void*), void* arg) {
  mu_.Lock();

  // Start background thread if necessary
  if (!started_bgthread_) {
     started_bgthread_ = true;
     StartThread(&WinEnv::BGThreadWrapper, this);
  }

  // If the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    bgsignal_.Signal();
  }

  // Add to priority queue
  queue_.push_back(BGItem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  mu_.Unlock();
}

void WinEnv::BGThread() {
  while (true) {
    // Wait until there is an item that is ready to run
    mu_.Lock();
    while (queue_.empty()) {
      bgsignal_.Wait();
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    mu_.Unlock();
    (*function)(arg);
  }
}

struct StartThreadState {
  void (*user_function)(void*);
  void* arg;
};

static void StartThreadWrapper(void* arg) {
  StartThreadState* state = reinterpret_cast<StartThreadState*>(arg);
  state->user_function(state->arg);
  delete state;
}

void WinEnv::StartThread(void (*function)(void* arg), void* arg) {
  StartThreadState* state = new StartThreadState;
  state->user_function = function;
  state->arg = arg;

  // Set stack size to 1M
  _beginthread(StartThreadWrapper, 1024 * 1024, (void*)state);
}

}

static port::OnceType once = LEVELDB_ONCE_INIT;
static Env* default_env;

static void InitDefaultEnv() { 
  //default_env = new WinEnv;
  static WinEnv env;
  default_env = &env;
}

Env* DefaultImpl() {
  port::InitOnce(&once, InitDefaultEnv);
  return default_env;
}

}
