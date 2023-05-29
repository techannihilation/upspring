/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "CSevenZipArchive.h"

#include <algorithm>
#include <stdexcept>
#include <cstring>  //memcpy

#include "../string_util.h"

#include "simdutf.h"
#include "spdlog/spdlog.h"

extern "C" {
#include "7zip/C/7zTypes.h"
#include "7zip/C/7zAlloc.h"
#include "7zip/C/7zCrc.h"
}

//
// UTF-16 <-> UTF-8 using simdutf
//
std::wstring convert_utf8_to_wchar(const std::string& utf8_str) {
  const size_t utf8_length = utf8_str.size();

  // Determine the maximum possible UTF-16 output size
  const size_t utf16_max_length = simdutf::utf16_length_from_utf8(utf8_str.data(), utf8_length);

  // Allocate a buffer for the UTF-16 output
  std::vector<char16_t> utf16_buffer(utf16_max_length, 0);

  // Convert the UTF-8 string to UTF-16
  simdutf::convert_utf8_to_utf16(utf8_str.data(), utf8_length, utf16_buffer.data());

  // Return the UTF-16 string as a wstring
  return std::wstring(reinterpret_cast<const wchar_t*>(utf16_buffer.data()), utf16_buffer.size());
}

std::optional<std::string> convert_char16_to_utf8(const char16_t* cstr, std::size_t cstr_len) {
  if (cstr == nullptr) {
    return std::nullopt;
  }

  const size_t utf8_length = simdutf::utf8_length_from_utf16(cstr, cstr_len);

  std::vector<char> utf8_buffer(utf8_length, 0);

  if (simdutf::convert_utf16_to_utf8(cstr, cstr_len, utf8_buffer.data()) != utf8_length) {
    spdlog::error("Failed to convert UTF16 to UTF8");
    return std::nullopt;
  }

  return std::string(utf8_buffer.data(), utf8_buffer.size());
}

std::optional<std::string> convert_wchar_to_utf8(const wchar_t* wstr) {
  const size_t wstr_length = std::wcslen(wstr);
  return convert_char16_to_utf8(reinterpret_cast<const char16_t*>(wstr), wstr_length);
}

static std::optional<std::string> get_file_name(const CSzArEx* db, std::size_t i) {
  const size_t len = SzArEx_GetFileNameUtf16(db, i, nullptr);
  std::vector<uint16_t> utf16_buffer(len);

  // Get the name into the buffer
  const std::size_t name_len = SzArEx_GetFileNameUtf16(db, i, utf16_buffer.data());

  return convert_char16_to_utf8(reinterpret_cast<char16_t*>(utf16_buffer.data()), name_len);
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
  archiveStream.wres = 0;

  LookToRead2_CreateVTable(&lookStream, False);
  lookStream.realStream = &archiveStream.vt;
  lookStream.buf = static_cast<Byte*>(ISzAlloc_Alloc(&allocImp, kInputBufSize));
  lookStream.bufSize = kInputBufSize;
  LookToRead2_Init(&lookStream);

  CrcGenerateTable();
  SzArEx_Init(&db);

  SRes res = SzArEx_Open(&db, &lookStream.vt, &allocImp, &allocTempImp);
  if (res != SZ_OK) {
    spdlog::error("7zip error opening '{}', error was: {}", name, strerror(res));
    return;
  }
  isOpen = true;

  // Get contents of archive and store name->int mapping
  for (std::size_t i = 0; i < db.NumFiles; ++i) {
    if (SzArEx_IsDir(&db, i)) {
      continue;
    }

    auto name = get_file_name(&db, i);
    if (!name) {
      spdlog::error("Error getting filename in Archive: {} {}, file skipped", GetErrorStr(res),
                    res);
      continue;
    }
    spdlog::debug("Got '{}'", name.value());

    const std::string lower_name = to_lower(name.value());

    if (lcNameIndex.find(lower_name) != lcNameIndex.end()) {
      spdlog::debug("7zip: skipping '{}' it's already known", lower_name);
      continue;
    }

    FileData fd;
    fd.origName = std::move(name.value());
    fd.fp = i;
    fd.size = SzArEx_GetFileSize(&db, i);

    if (SzBitWithVals_Check(&db.Attribs, i)) {
      if ((db.Attribs.Vals[i] & 1 << 16) != 0U) {
        fd.mode = 0755;
      } else {
        fd.mode = 0644;
      }
    }

    lcNameIndex.emplace(to_lower(fd.origName), fileEntries.size());
    fileEntries.emplace_back(std::move(fd));
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

std::size_t CSevenZipArchive::NumFiles() const { return fileEntries.size(); }

bool CSevenZipArchive::GetFile(std::size_t fid, std::vector<std::uint8_t>& buffer) {
  // Get 7zip to decompress it
  size_t offset = 0;
  size_t outSizeProcessed = 0;
  const SRes res =
      SzArEx_Extract(&db, &lookStream.vt, fileEntries[fid].fp, &blockIndex, &outBuffer,
                     &outBufferSize, &offset, &outSizeProcessed, &allocImp, &allocTempImp);
  if (res == SZ_OK) {
    buffer.resize(outSizeProcessed);
    if (outSizeProcessed > 0) {
      memcpy(buffer.data(), reinterpret_cast<char*>(outBuffer) + offset, outSizeProcessed);
    }
    return true;
  }
  spdlog::error("Error extracting {}: {}", fileEntries[fid].origName, GetErrorStr(res));
  return false;
}

void CSevenZipArchive::FileInfo(std::size_t fid, std::string& par_name, int& par_size,
                                int& par_mode) const {
  par_name = fileEntries[fid].origName;
  par_size = fileEntries[fid].size;
  par_mode = fileEntries[fid].mode;
}

#if 0
std::size_t CSevenZipArchive::GetCrc32(std::size_t fid) { return fileData[fid].crc; }
#endif
