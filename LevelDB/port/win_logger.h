// Logger implementation for Windows

#ifndef STORAGE_LEVELDB_UTIL_WIN_LOGGER_H_
#define STORAGE_LEVELDB_UTIL_WIN_LOGGER_H_

#pragma warning(disable : 4996)

#include <stdio.h>
#include "leveldb/env.h"

#include <windows.h>

namespace leveldb {

class WinLogger : public Logger {
 public:
  explicit WinLogger(FILE* f) : file_(f) { assert(file_); }
  virtual ~WinLogger() {
    fclose(file_);
  }
  virtual void Logv(const char* format, va_list ap) {
    const uint64_t thread_id = static_cast<uint64_t>(::GetCurrentThreadId());

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];

    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }

      char* p = base;
      char* limit = base + bufsize;

      SYSTEMTIME st;

      // GetSystemTime returns UTC time, we want local time!
      ::GetLocalTime(&st);

      p += _snprintf_s(p, limit - p, _TRUNCATE,
        "%04d/%02d/%02d-%02d:%02d:%02d.%03d %llx ",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        static_cast<long long unsigned int>(thread_id));

      // Print the message
      if (p < limit) {
        va_list backup_ap = ap;
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // Truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue; // Try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // Add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      fwrite(base, 1, p - base, file_);
      fflush(file_);
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
 private:
  FILE* file_;
};

}
#endif  // STORAGE_LEVELDB_UTIL_WIN_LOGGER_H_
