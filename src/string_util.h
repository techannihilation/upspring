#pragma once

#include <algorithm>
#include <string>
#include <numeric>

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