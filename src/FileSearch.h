#pragma once

#include <vector>
#include <string>
#include <filesystem>

std::vector<std::filesystem::path> find_files(
    const std::filesystem::path& par_path,
    std::function<bool(const std::filesystem::path par_path)> par_filter,
    bool par_recursive = false) {
  std::vector<std::filesystem::path> r;

  // No dir given?
  if (!std::filesystem::is_directory(par_path)) {
    return r;
  }

  // Iterate over the directory
  for (auto& entry : std::filesystem::directory_iterator(par_path)) {
    // Directories
    if (entry.is_directory()) {
      if (!par_recursive) {
        continue;
      }

      auto s = find_files(entry.path(), par_filter, true);
      r.reserve(r.size() + distance(s.begin(), s.end()));
      r.insert(r.end(), s.begin(), s.end());

      continue;
    }

    // Only regular files
    if (!entry.is_regular_file()) {
      continue;
    }

    // Filter
    if (!par_filter(entry.path())) {
      continue;
    }

    r.emplace_back(entry.path());
  }

  return r;
}
