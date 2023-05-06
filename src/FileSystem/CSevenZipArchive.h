/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "IArchive.h"

extern "C" {
#include "7z/7zTypes.h"
#include "7z/7zFile.h"
#include "7z/7z.h"
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
    /**
     * Real/unpacked size of the file in bytes.
     * @see #unpackedSize
     * @see #packedSize
     */
    int size;
    std::string origName;
    std::size_t crc;
    /**
     * How many bytes of files have to be unpacked to get to this file.
     * This either equal to size, or is larger, if there are other files
     * in the same solid block.
     * @see #size
     * @see #packedSize
     */
    int unpackedSize;
    /**
     * How many bytes of the archive have to be read
     * from disc to get to this file.
     * This may be smaller or larger then size,
     * and is smaller then or equal to unpackedSize.
     * @see #size
     * @see #unpackedSize
     */
    int packedSize;
    /**
     * file mode
     */
    int mode;
  };
  int GetFileName(const CSzArEx* db, int i);
  static const char* GetErrorStr(int res);

  std::vector<FileData> fileData;
  UInt16* tempBuf = nullptr;
  size_t tempBufSize = 0;

  CFileInStream archiveStream{};
  CSzArEx db{};
  CLookToRead2 lookStream{};
  ISzAlloc allocImp{};
  ISzAlloc allocTempImp{};

  bool isOpen;
};