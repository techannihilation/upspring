/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "CZipArchive.h"

#include <algorithm>
#include <stdexcept>

#include "mz.h"
#include "mz_strm.h"
#include "mz_strm_os.h"
#include "mz_zip.h"

#include "../string_util.h"

#include "spdlog/spdlog.h"

CZipArchive::CZipArchive(const std::string& archiveName)
    : IArchive(archiveName), streamHandle(nullptr), zipHandle(nullptr) {
  mz_stream_os_create(&streamHandle);
  if (mz_stream_open(streamHandle, archiveName.c_str(), MZ_OPEN_MODE_READ) != MZ_OK) {
    spdlog::error("minizip: Error opening: {}", archiveName);
    return;
  }

  mz_zip_create(&zipHandle);
  if (mz_zip_open(zipHandle, streamHandle, MZ_OPEN_MODE_READ) != MZ_OK) {
    spdlog::error("minizip: Error opening: {}", archiveName);
    return;
  }

  // We need to map file positions to speed up opening later
  for (int ret = mz_zip_goto_first_entry(zipHandle); ret == MZ_OK;
       ret = mz_zip_goto_next_entry(zipHandle)) {
    mz_zip_file* fileInfo = nullptr;

    if (mz_zip_entry_get_info(zipHandle, &fileInfo) != MZ_OK) {
      spdlog::error("minizip: Failed to get entries from archive: {}", archiveName);
      break;
    }

    const std::string fLowerName = to_lower(fileInfo->filename);
    if (fLowerName.empty()) {
      continue;
    }
    const char last = fLowerName[fLowerName.length() - 1];
    if ((last == '/') || (last == '\\')) {
      continue;  // exclude directory names
    }

    if (lcNameIndex.find(fLowerName) != lcNameIndex.end()) {
      spdlog::debug("minizip: Skipping '{}' it's already known", fLowerName);
      continue;
    }

    fileData.emplace_back(mz_zip_get_entry(zipHandle), fileInfo->uncompressed_size, fLowerName,
                          fileInfo->crc);
    lcNameIndex.insert({fLowerName, fileData.size() - 1});
  }
}

CZipArchive::~CZipArchive() {
  if (zipHandle != nullptr) {
    mz_zip_close(zipHandle);
  }
  if (streamHandle != nullptr) {
    mz_stream_close(streamHandle);
  }
}

bool CZipArchive::IsOpen() const { return zipHandle != nullptr; }

uint CZipArchive::NumFiles() const { return fileData.size(); }

void CZipArchive::FileInfo(uint fid, std::string& name, int& size, int& mode) const {
  //	assert(IsFileId(fid));

  name = fileData[fid].fileName;
  size = static_cast<int>(fileData[fid].size);
  mode = 0644;
}

#if 0
uint CZipArchive::GetCrc32(uint fid) {
  //	assert(IsFileId(fid));

  return fileData[fid].crc;
}
#endif

// To simplify things, files are always read completely into memory from
// the zip-file, since zlib does not provide any way of reading more
// than one file at a time
bool CZipArchive::GetFile(uint fid, std::vector<std::uint8_t>& buffer) {
  if (mz_zip_goto_entry(zipHandle, fileData[fid].pos) != MZ_OK) {
    spdlog::debug("Failed to goto zip entry");
    return false;
  }

  if (mz_zip_entry_read_open(zipHandle, 0, nullptr) != MZ_OK) {
    spdlog::debug("Failed to goto open entry");
    return false;
  }

  auto sizeWanted = fileData[fid].size;
  buffer.resize(sizeWanted);
  bool ret = true;
  auto readLen = mz_zip_entry_read(zipHandle, buffer.data(), static_cast<int32_t>(buffer.size()));
  if (buffer.empty() || readLen != sizeWanted) {
    spdlog::debug("File size '{}' buffer size '{}'", sizeWanted, readLen);
    ret = false;
  }

  if (mz_zip_entry_close(zipHandle) != MZ_OK) {
    ret = false;
  }

  if (!ret) {
    buffer.clear();
  }

  return ret;
}
