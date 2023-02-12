/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "IArchive.h"

#include "../string_util.h"

// #include "System/StringUtil.h"

uint IArchive::FindFile(const std::string& filePath) const
{
	const std::string normalizedFilePath = to_lower(filePath);
	const auto f_it = lcNameIndex.find(normalizedFilePath);

	if (f_it != lcNameIndex.end()) {
		return f_it->second;
    }

	return NumFiles();
}

#if 0
bool IArchive::CalcHash(uint32_t fid, uint8_t hash[sha512::SHA_LEN], std::vector<std::uint8_t>& fb)
{
	// NOTE: should be possible to avoid a re-read for buffered archives
	if (!GetFile(fid, fb))
		return false;

	if (fb.empty())
		return false;

	sha512::calc_digest(fb.data(), fb.size(), hash);
	return true;
}
#endif

bool IArchive::GetFileByName(const std::string& name, std::vector<std::uint8_t>& buffer)
{
	const uint fid = FindFile(name);

	if (!IsFileId(fid)) {
		return false;
    }

	GetFile(fid, buffer);
	return true;
}
