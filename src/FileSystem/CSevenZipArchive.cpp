/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "CSevenZipArchive.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>  //memcpy

#include "../string_util.h"

#include "spdlog/spdlog.h"


extern "C" {
#include "7z/7zTypes.h"
#include "7z/7zAlloc.h"
#include "7z/7zCrc.h"
}

static Byte kUtf8Limits[5] = {0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
static bool Utf16_To_Utf8(char* dest, size_t* destLen, const UInt16* src, size_t srcLen) {
  size_t destPos = 0;
  size_t srcPos = 0;
  for (;;) {
    unsigned numAdds = 0;
    if (srcPos == srcLen) {
      *destLen = destPos;
      return True;
    }
    UInt32 value = src[srcPos++];
    if (value < 0x80) {
      if (dest != nullptr) {
        dest[destPos] = static_cast<char>(value);
      }
      destPos++;
      continue;
    }
    if (value >= 0xD800 && value < 0xE000) {
      if (value >= 0xDC00 || srcPos == srcLen) {
        break;
      }
      const UInt32 c2 = src[srcPos++];
      if (c2 < 0xDC00 || c2 >= 0xE000) {
        break;
      }
      value = (((value - 0xD800) << 10) | (c2 - 0xDC00)) + 0x10000;
    }
    for (numAdds = 1; numAdds < 5; numAdds++) {
      if (value < ((static_cast<UInt32>(1)) << (numAdds * 5 + 6))) {
        break;
      }
    }
    if (dest != nullptr) {
      dest[destPos] = static_cast<char>(kUtf8Limits[numAdds - 1] + (value >> (6 * numAdds)));
    }
    destPos++;
    do {
      numAdds--;
      if (dest != nullptr) {
        dest[destPos] = static_cast<char>(0x80 + ((value >> (6 * numAdds)) & 0x3F));
      }
      destPos++;
    } while (numAdds != 0);
  }
  *destLen = destPos;
  return false;
}

int CSevenZipArchive::GetFileName(const CSzArEx* db, int i) {
  const size_t len = SzArEx_GetFileNameUtf16(db, i, nullptr);

  if (len > tempBufSize) {
    SzFree(nullptr, tempBuf);
    tempBufSize = len;
    tempBuf = static_cast<UInt16*>(SzAlloc(nullptr, tempBufSize * sizeof(tempBuf[0])));
    if (tempBuf == nullptr) {
      return SZ_ERROR_MEM;
    }
  }
  tempBuf[len - 1] = 0;
  return SzArEx_GetFileNameUtf16(db, i, tempBuf);
}

const char* CSevenZipArchive::GetErrorStr(int res) {
  switch (res) {
    case SZ_OK:
      return "OK";
    case SZ_ERROR_FAIL:
      return "Extracting failed";
    case SZ_ERROR_CRC:
      return "CRC error (archive corrupted?)";
    case SZ_ERROR_INPUT_EOF:
      return "Unexpected end of file (truncated?)";
    case SZ_ERROR_MEM:
      return "Out of memory";
    case SZ_ERROR_UNSUPPORTED:
      return "Unsupported archive";
    case SZ_ERROR_NO_ARCHIVE:
      return "Archive not found";
  }
  return "Unknown error";
}

CSevenZipArchive::CSevenZipArchive(const std::string& name) : IArchive(name), isOpen(false) {
  allocImp.Alloc = SzAlloc;
  allocImp.Free = SzFree;
  allocTempImp.Alloc = SzAllocTemp;
  allocTempImp.Free = SzFreeTemp;
  constexpr const size_t kInputBufSize(static_cast<size_t>(1) << 18);

  SzArEx_Init(&db);

#ifdef _WIN32
  WRes wres = InFile_OpenW(&archiveStream.file, s2ws(name).c_str());
#else
  WRes const wres = InFile_Open(&archiveStream.file, name.c_str());
#endif
  if (wres != 0) {
    spdlog::error("OS error opening '{}', error was: {}", name, strerror(wres));
    return;
  }

  FileInStream_CreateVTable(&archiveStream);
  LookToRead2_CreateVTable(&lookStream, False);

  lookStream.realStream = &archiveStream.vt;
  LookToRead2_Init(&lookStream);
  lookStream.buf = nullptr;
  lookStream.buf = static_cast<Byte*>(ISzAlloc_Alloc(&allocImp, kInputBufSize));

  CrcGenerateTable();
  SzArEx_Init(&db);

  SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
  if (res != SZ_OK) {
    spdlog::error("7Zip error opening '{}', error was: {}", name, strerror(res));
    return;
  }
  isOpen = true;

  // Get contents of archive and store name->int mapping
  for (std::size_t i = 0; i < db.NumFiles; ++i) {
    const bool isDir = SzArEx_IsDir(&db, i);

    if (isDir) {
      continue;
    }

    const int written = GetFileName(&db, i);
    if (written <= 0) {
      spdlog::error("Error getting filename in Archive: {} {}, file skipped in {}",
                    GetErrorStr(res), res, name);
      continue;
    }

    char buf[1024];
    size_t dstlen = sizeof(buf);

    Utf16_To_Utf8(buf, &dstlen, tempBuf, written);
    const std::string lower_name = to_lower(buf);

    if (lcNameIndex.find(lower_name) != lcNameIndex.end()) {
      spdlog::debug("7zip: skipping '{}' it's already known", lower_name);
      continue;
    }


    FileData fd;
    fd.origName = buf;
    fd.fp = i;
    fd.size = SzArEx_GetFileSize(&db, i);
    fd.crc = 0;  //; (f->Size > 0) ? f->Crc : 0;
    if (SzBitWithVals_Check(&db.Attribs, i)) {
      // LOG_DEBUG("%s %d", fd.origName.c_str(), db.Attribs.Vals[i])
      if ((db.Attribs.Vals[i] & 1 << 16) != 0U) {
        fd.mode = 0755;
      } else {
        fd.mode = 0644;
      }
    }

    fileData.emplace_back(std::move(fd));
    lcNameIndex.insert({to_lower(fd.origName), fileData.size() - 1});
  }
}

CSevenZipArchive::~CSevenZipArchive() {
  if (outBuffer != nullptr) {
    IAlloc_Free(&allocImp, outBuffer);
  }
  if (isOpen) {
    File_Close(&archiveStream.file);
    ISzAlloc_Free(&allocImp, lookStream.buf);
  }
  SzArEx_Free(&db, &allocImp);
  SzFree(nullptr, tempBuf);
}

std::size_t CSevenZipArchive::NumFiles() const { return fileData.size(); }

bool CSevenZipArchive::GetFile(std::size_t fid, std::vector<std::uint8_t>& buffer) {
  // Get 7zip to decompress it
  size_t offset = 0;
  size_t outSizeProcessed = 0;
  const SRes res =
      SzArEx_Extract(&db, &lookStream.vt, fileData[fid].fp, &blockIndex, &outBuffer, &outBufferSize,
                     &offset, &outSizeProcessed, &allocImp, &allocTempImp);
  if (res == SZ_OK) {
    buffer.resize(outSizeProcessed);
    if (outSizeProcessed > 0) {
      memcpy(buffer.data(), reinterpret_cast<char*>(outBuffer) + offset, outSizeProcessed);
    }
    return true;
  }
  spdlog::error("Error extracting {}: {}", fileData[fid].origName, GetErrorStr(res));
  return false;
}

void CSevenZipArchive::FileInfo(std::size_t fid, std::string& par_name, int& par_size, int& par_mode) const {
  par_name = fileData[fid].origName;
  par_size = fileData[fid].size;
  par_mode = fileData[fid].mode;
}

#if 0
std::size_t CSevenZipArchive::GetCrc32(std::size_t fid) { return fileData[fid].crc; }
#endif
