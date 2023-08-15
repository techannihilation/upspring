#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#if _MSC_VER > 1310
#define VSNPRINTF _vsnprintf_s
#else
#define VSNPRINTF _vsnprintf
#endif
#else
#define VSNPRINTF vsnprintf
#endif

#include <stdarg.h>
#include <vector>
#include <algorithm>
#include <string>
#include <numeric>
#include <filesystem>

// Printf style std::string formatting
inline std::string SPrintf(const char* fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  char buf[256];
  VSNPRINTF(buf, sizeof(buf), fmt, vl);
  va_end(vl);

  return buf;
}

inline std::string to_lower(const std::string& str) {
  std::string tmp = str;
  std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::tolower);
  return tmp;
}

inline std::string join(const std::vector<std::string> alist) {
  return alist.empty() ? "" : /* leave early if there are no items in the list */
             std::accumulate( /* otherwise, accumulate */
                             ++alist.begin(), alist.end(), /* the range 2nd to after-last */
                             *alist.begin(), /* and start accumulating with the first item */
                             [](auto& a, auto& b) { return a + "," + b; });
}

#if defined(_WIN32)
inline std::wstring s2ws(const std::string& s) {
  const size_t slength = s.length();
  const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
  wchar_t* buf = new wchar_t[len];
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
  std::wstring r(buf, len);
  delete[] buf;
  return r;
}

inline std::string ws2s(const std::wstring& s) {
  const size_t slength = s.length();
  const int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, nullptr, 0, nullptr, nullptr);
  char* buf = new char[len];
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, buf, len, nullptr, nullptr);
  std::string r(buf, len);
  delete[] buf;
  return r;
}
#endif
