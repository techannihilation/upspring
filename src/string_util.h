#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <vector>
#include <algorithm>
#include <string>
#include <numeric>
#include <filesystem>

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

#if defined(__linux__)
inline std::string path_to_string(const std::filesystem::path& par_inpath) 
{
  return par_inpath.string();
}

inline const char *path_to_cstring(const std::filesystem::path& par_inpath)
{
  return par_inpath.c_str();
}

#elif defined(_WIN32)
inline std::wstring s2ws(const std::string& s)
{
	const size_t slength = s.length();
	const int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf, len);
	delete[] buf;
	return r;
}

inline std::string ws2s(const std::wstring& s)
{
	const size_t slength = s.length();
	const int len =
	    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, nullptr, 0, nullptr, nullptr);
	char* buf = new char[len];
	WideCharToMultiByte(CP_UTF8, 0, s.c_str(), slength, buf, len, nullptr, nullptr);
	std::string r(buf, len);
	delete[] buf;
	return r;
}

inline std::string path_to_string(const std::filesystem::path& par_inpath) 
{
  return ws2s(par_inpath.wstring());
}

inline const char *path_to_cstring(const std::filesystem::path& par_inpath)
{
  return path_to_string(par_inpath).c_str();
}
#endif
