/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include "IArchive.h"
#include <filesystem>

/**
 * A Directory as archive.
 */
class CDirectoryArchive : public IArchive {
 public:
  explicit CDirectoryArchive(const std::string& name);
  virtual ~CDirectoryArchive();

  virtual std::size_t NumFiles() const override;
  virtual void FileInfo(std::size_t fid, std::string& name, int& size, int& mode) const override;

  virtual bool GetFile(std::size_t fid, std::vector<std::uint8_t>& buffer) override;

 private:
  struct FileData {
    std::size_t size;
    std::string lower_name;
    std::string orig_name;
    int mode;

    FileData(std::size_t par_size, std::string par_lower_name, std::string par_name, int par_mode)
        : size(par_size), lower_name(par_lower_name), orig_name(par_name), mode(par_mode){};
  };
  static const char* GetErrorStr(int res);

  std::vector<FileData> fileEntries_;

  std::filesystem::path dirname_;
};