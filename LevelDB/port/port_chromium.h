// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// See port_example.h for documentation for the following types/functions.

#ifndef STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_
#define STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_
#include <stdint.h>
#include <string>
#include <basetsd.h>
#include <cstring>

#define snprintf _snprintf
typedef SSIZE_T ssize_t;
#if !defined(__clang__) && _MSC_VER <= 1700
# define va_copy(a, b) do { (a) = (b); } while (0)
#endif

namespace leveldb {
namespace port {

// Chromium only supports little endian.
static const bool kLittleEndian = true;

class Mutex {
 public:
    Mutex();
    ~Mutex();
    void Lock();
    void Unlock();
  void AssertHeld();

 private:
  // Use opaque void* to allow different implement on different port.
  void* mu_;

  friend class CondVar;

  // No copying
  Mutex(const Mutex&);
  void operator=(const Mutex&);
};

// the Win32 API offers a dependable condition variable mechanism, but only starting with
// Windows 2008 and Vista
// no matter what we will implement our own condition variable with a semaphore
// implementation as described in a paper written by Andrew D. Birrell in 2003
class CondVar {
 public:
    explicit CondVar(Mutex* mu);
    ~CondVar();
    void Wait();
    void Signal();
  void SignalAll();

 private:
  Mutex* mu_;

  Mutex* wait_mtx_;
  long waiting_;

  void* sem1_;  // used as base::ConditionVariable in chromium port.
  void* sem2_;

  // No copying
  CondVar(const CondVar&);
  void operator=(const CondVar&);
};

// Storage for a lock-free pointer
class AtomicPointer {
 private:
  void* rep_;
 public:
  AtomicPointer() : rep_(nullptr) {}
  explicit AtomicPointer(void* p);

  void* Acquire_Load() const;
  void Release_Store(void* v);
  void* NoBarrier_Load() const;
  void NoBarrier_Store(void* v);
};

// Implementation of OnceType and InitOnce() pair, this is equivalent to
// pthread_once_t and pthread_once().
typedef int OnceType;

enum {
  ONCE_STATE_UNINITIALIZED = 0,
  ONCE_STATE_EXECUTING_CLOSURE = 1,
  ONCE_STATE_DONE = 2
};

#define LEVELDB_ONCE_INIT   leveldb::port::ONCE_STATE_UNINITIALIZED

// slow code path
  void InitOnce(OnceType* once, void (*initializer)());

bool Snappy_Compress(const char* input, size_t input_length,
                     std::string* output);
bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                  size_t* result);
bool Snappy_Uncompress(const char* input_data, size_t input_length,
                       char* output);

inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
  return false;
}

}
}

#endif  // STORAGE_LEVELDB_PORT_PORT_CHROMIUM_H_
