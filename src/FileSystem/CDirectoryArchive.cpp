#include "CDirectoryArchive.h"

#include <algorithm>
#include <fstream>
#include <filesystem>

#include "../string_util.h"

#include "spdlog/spdlog.h"

CDirectoryArchive::CDirectoryArchive(const std::string& name) : IArchive(name) {
  auto tmp = std::filesystem::absolute(name).string();
  if ('/' == tmp.back()) {
    tmp.pop_back();
  }
  dirname_ = tmp;
  auto dirnameLen = tmp.length();

  spdlog::debug("Scanning {}", tmp);

  for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(dirname_)) {
    auto filename = dirEntry.path().string().substr(dirnameLen + 1);
    auto lc_file_name = to_lower(filename);

    std::size_t size = 0;
    if (dirEntry.is_regular_file()) {
      size = dirEntry.file_size();
    }

    lcNameIndex[lc_file_name] = fileEntries_.size();
    fileEntries_.emplace_back(size, lc_file_name, filename, 0777);
  }
}

CDirectoryArchive::~CDirectoryArchive() {}

std::size_t CDirectoryArchive::NumFiles() const { return fileEntries_.size(); }

bool CDirectoryArchive::GetFile(std::size_t par_fid, std::vector<std::uint8_t>& par_buffer) {
  auto realpath = dirname_ / fileEntries_[par_fid].orig_name;

  std::ifstream instream(realpath, std::ios::in | std::ios::binary);
  if (!instream.is_open()) {
    return false;
  }

  std::for_each(std::istreambuf_iterator<char>(instream), std::istreambuf_iterator<char>(),
                [&par_buffer](const char c) { par_buffer.push_back(c); });

  return true;
}

void CDirectoryArchive::FileInfo(std::size_t par_fid, std::string& par_name, int& par_size,
                                 int& par_mode) const {
  par_name = fileEntries_[par_fid].lower_name;
  par_size = fileEntries_[par_fid].size;
  par_mode = fileEntries_[par_fid].mode;
}
