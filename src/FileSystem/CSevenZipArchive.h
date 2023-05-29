/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include "IArchive.h"

extern "C" {
#include "7zip/C/7zFile.h"
#include "7zip/C/7z.h"
}

/**
 * An LZMA/7zip compressed, single-file archive.
 */
class CSevenZipArchive : public IArchive {
 public:
  explicit CSevenZipArchive(const std::string& name);
  virtual ~CSevenZipArchive();

  virtual std::size_t NumFiles() const override;
  virtual void FileInfo(std::size_t fid, std::string& name, int& size, int& mode) const override;

  virtual bool GetFile(std::size_t fid, std::vector<std::uint8_t>& buffer) override;

#if 0
  virtual unsigned GetCrc32(std::size_t fid);
#endif

 private:
  UInt32 blockIndex = 0xFFFFFFFF;
  Byte* outBuffer = nullptr;
  size_t outBufferSize = 0;

  struct FileData {
    int fp;
    std::size_t size;
    std::string origName;
    int mode;
  };
  static const char* GetErrorStr(int res);

  std::vector<FileData> fileEntries;
  UInt16* tempBuf = nullptr;
  size_t tempBufSize = 0;

  CFileInStream archiveStream{};
  CSzArEx db{};
  CLookToRead2 lookStream{};
  ISzAlloc allocImp{};
  ISzAlloc allocTempImp{};

  bool isOpen;
};