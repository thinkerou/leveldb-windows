// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "port_chromium.h"

#include <windows.h>
#include <cassert>

namespace leveldb {
namespace port {

Mutex::Mutex() :
    mu_(nullptr) {
  assert(!mu_);
  mu_ = static_cast<void *>(new CRITICAL_SECTION());
  ::InitializeCriticalSection(static_cast<CRITICAL_SECTION *>(mu_));
  assert(mu_);
}

Mutex::~Mutex() {
  assert(mu_);
  ::DeleteCriticalSection(static_cast<CRITICAL_SECTION *>(mu_));
  delete static_cast<CRITICAL_SECTION *>(mu_);
  mu_ = nullptr;
  assert(!mu_);
}

void Mutex::Lock() {
  assert(mu_);
  ::EnterCriticalSection(static_cast<CRITICAL_SECTION *>(mu_));
}

void Mutex::Unlock() {
  assert(mu_);
  ::LeaveCriticalSection(static_cast<CRITICAL_SECTION *>(mu_));
}

void Mutex::AssertHeld() {
  assert(mu_);
  assert(1);
}

CondVar::CondVar(Mutex* mu) :
    waiting_(0),
    mu_(mu),
    wait_mtx_(new Mutex()),
    sem1_(::CreateSemaphore(NULL, 0, 10000, NULL)),
    sem2_(::CreateSemaphore(NULL, 0, 10000, NULL)) {
  assert(mu_);
}

CondVar::~CondVar() {
  delete wait_mtx_;
  ::CloseHandle(sem1_);
  ::CloseHandle(sem2_);
}

void CondVar::Wait() {
  mu_->AssertHeld();

  wait_mtx_->Lock();
  ++waiting_;
  wait_mtx_->Unlock();

  mu_->Unlock();

  // initiate handshake
  ::WaitForSingleObject(sem1_, INFINITE);
  ::ReleaseSemaphore(sem2_, 1, NULL);
  mu_->Lock();
}

void CondVar::Signal() {
  wait_mtx_->Lock();
  if (waiting_ > 0) {
    --waiting_;

    // finalize handshake
    ::ReleaseSemaphore(sem1_, 1, NULL);
    ::WaitForSingleObject(sem2_, INFINITE);
  }
  wait_mtx_->Unlock();
}

void CondVar::SignalAll() {
  wait_mtx_->Lock();
  for(long i = 0; i < waiting_; ++i) {
    ::ReleaseSemaphore(sem1_, 1, NULL);
    while(waiting_ > 0) {
      --waiting_;
      ::WaitForSingleObject(sem2_, INFINITE);
    }
  }
  wait_mtx_->Unlock();
}

AtomicPointer::AtomicPointer(void* p) {
  Release_Store(p);
}

void* AtomicPointer::Acquire_Load() const {
  void * p = nullptr;
  InterlockedExchangePointer(&p, rep_);
  return p;
}

void AtomicPointer::Release_Store(void* v) {
  InterlockedExchangePointer(&rep_, v);
}

void* AtomicPointer::NoBarrier_Load() const {
  return rep_;
}

void AtomicPointer::NoBarrier_Store(void* v) {
  rep_ = v;
}

void InitOnce(OnceType* once, void (*initializer)()) {
  if (*once != ONCE_STATE_DONE) {
    OnceType state = *once;
    if (state == ONCE_STATE_DONE)
      return;

    state = InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(once),
                static_cast<LONG>(ONCE_STATE_UNINITIALIZED),
                static_cast<LONG>(ONCE_STATE_EXECUTING_CLOSURE));

    if (state == ONCE_STATE_UNINITIALIZED) {
      // We are the first thread, we have to call the closure.
      (*initializer)();
      InterlockedExchangePointer((PVOID*)once, (PVOID*)ONCE_STATE_DONE);
    } else {
      // Another thread is running the closure, wait until completion.
      while (state == ONCE_STATE_EXECUTING_CLOSURE) {
        ::Sleep(0);
        state = *once;
      }
    }
  }
}

bool Snappy_Compress(const char* input, size_t input_length,
                     std::string* output) {
#if defined(USE_SNAPPY)
  output->resize(snappy::MaxCompressedLength(input_length));
  size_t outlen;
  snappy::RawCompress(input, input_length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#else
  return false;
#endif
}

bool Snappy_GetUncompressedLength(const char* input_data,
                                  size_t input_length,
                                  size_t* result) {
#if defined(USE_SNAPPY)
  return snappy::GetUncompressedLength(input_data, input_length, result);
#else
  return false;
#endif
}

bool Snappy_Uncompress(const char* input_data, size_t input_length,
                       char* output) {
#if defined(USE_SNAPPY)
  return snappy::RawUncompress(input_data, input_length, output);
#else
  return false;
#endif
}

}
}
