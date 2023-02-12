/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <string>
#include <vector>
#include "IArchive.h"

/**
 * A zip compressed, single-file archive.
 */
class CZipArchive : public IArchive {
 public:
  explicit CZipArchive(const std::string& archiveName);
  virtual ~CZipArchive();

  virtual bool IsOpen() const;

  virtual uint NumFiles() const override;
  virtual void FileInfo(uint fid, std::string& name, int& size, int& mode) const override;

  virtual bool GetFile(uint fid, std::vector<std::uint8_t>& buffer) override;

#if 0
  virtual uint GetCrc32(uint fid);
#endif

 protected:
  void* streamHandle;
  void* zipHandle;

  struct FileData {
    int64_t pos;
    uint64_t size;
    const std::string fileName;
    uint32_t crc;

    FileData(int64_t pPos, uint64_t pSize, const std::string& pFileName, uint32_t pCrc)
        : pos(pPos), size(pSize), fileName(std::move(pFileName)), crc(pCrc){};
  };
  std::vector<FileData> fileData;
};