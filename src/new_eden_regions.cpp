#include "new_eden_regions.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace neweden
{
namespace
{
#pragma pack(push, 1)
struct RegionHeader
{
	char magic[8];
	uint16_t version;
	uint16_t recordSize;
	uint32_t count;
	uint32_t sdeBuild;
	uint32_t reserved;
	uint8_t sourceSha256[32];
	uint8_t pad[8];
};

struct RegionBin
{
	uint32_t id;
	uint32_t regionId;
};
#pragma pack(pop)

static_assert(sizeof(RegionHeader) == 64, "NEREGN1 header must stay 64 bytes");
static_assert(sizeof(RegionBin) == 8, "NEREGN1 record must stay 8 bytes");

const char kMagic[8] = { 'N', 'E', 'R', 'E', 'G', 'N', '1', '\0' };
const uint16_t kVersion = 1;

const uint32_t kAtlas[11] = {
	0xff4c26, 0xffa62b, 0xf2ff61, 0x2ef0a9, 0x00c27a,
	0x45c6ff, 0x1f6bff, 0x9d7dff, 0xff5b90, 0xff3b30, 0x00d1ff,
};

std::string Hex32(const uint8_t bytes[32])
{
	static const char* kDigits = "0123456789abcdef";
	std::string out(64, '0');
	for (int i = 0; i < 32; ++i)
	{
		out[size_t(i) * 2] = kDigits[(bytes[i] >> 4) & 0xf];
		out[size_t(i) * 2 + 1] = kDigits[bytes[i] & 0xf];
	}
	return out;
}

std::wstring ExeDir()
{
	wchar_t exePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring path(exePath);
	const auto slash = path.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
	{
		path.erase(slash + 1);
	}
	return path;
}

bool FileExists(const std::wstring& path)
{
	const DWORD attrs = GetFileAttributesW(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string Narrow(const std::wstring& path)
{
	if (path.empty())
	{
		return {};
	}
	const int needed = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (needed <= 1)
	{
		return {};
	}
	std::string out(size_t(needed - 1), '\0');
	WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, out.data(), needed, nullptr, nullptr);
	return out;
}
}

Rgb RegionAtlasColor(uint32_t regionId)
{
	const uint32_t packed = kAtlas[regionId % 11];
	Rgb c;
	c.r = float((packed >> 16) & 0xff) / 255.0f;
	c.g = float((packed >> 8) & 0xff) / 255.0f;
	c.b = float(packed & 0xff) / 255.0f;
	return c;
}

bool LoadRegions(const std::wstring& path, RegionTable& out, std::string& error)
{
	out = RegionTable();
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		error = "could not open " + Narrow(path);
		return false;
	}

	in.seekg(0, std::ios::end);
	const std::streamoff fileSize = in.tellg();
	in.seekg(0, std::ios::beg);
	if (fileSize < std::streamoff(sizeof(RegionHeader)))
	{
		error = "region file is too small";
		return false;
	}

	RegionHeader header = {};
	in.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!in)
	{
		error = "failed to read region header";
		return false;
	}
	if (std::memcmp(header.magic, kMagic, 8) != 0)
	{
		error = "region magic is not NEREGN1";
		return false;
	}
	if (header.version != kVersion || header.recordSize != sizeof(RegionBin))
	{
		error = "unsupported region version or record size";
		return false;
	}
	if (header.count != kExpectedRegionCount)
	{
		error = "region count does not match the pinned 5485 known-space export";
		return false;
	}

	const uint64_t recordsBytes = uint64_t(header.count) * uint64_t(header.recordSize);
	if (uint64_t(fileSize) != uint64_t(sizeof(RegionHeader)) + recordsBytes)
	{
		error = "region size does not match header + records";
		return false;
	}

	std::vector<RegionBin> records(header.count);
	in.read(reinterpret_cast<char*>(records.data()), std::streamsize(recordsBytes));
	if (!in)
	{
		error = "failed to read region records";
		return false;
	}

	out.version = header.version;
	out.sdeBuild = header.sdeBuild;
	out.sourceSha256 = Hex32(header.sourceSha256);
	out.loadedPath = Narrow(path);
	out.records.reserve(header.count);

	std::unordered_set<uint32_t> seen;
	std::unordered_set<uint32_t> regionIds;
	seen.reserve(header.count);
	for (const RegionBin& rec : records)
	{
		if (rec.id < 30000000u || rec.id > 30999999u)
		{
			error = "region table contains a system id outside New Eden known-space";
			return false;
		}
		if (rec.regionId < 10000000u || rec.regionId > 10999999u)
		{
			error = "region table contains an unexpected region id";
			return false;
		}
		if (!seen.insert(rec.id).second)
		{
			error = "region table contains a duplicate system id";
			return false;
		}
		regionIds.insert(rec.regionId);
		RegionRecord row;
		row.id = rec.id;
		row.regionId = rec.regionId;
		out.records.push_back(row);
	}
	out.distinctRegions = uint32_t(regionIds.size());
	return true;
}

bool FindAndLoadRegions(RegionTable& out, std::string& error)
{
	const std::wstring exeDir = ExeDir();
	const std::wstring candidates[] = {
		exeDir + L"new_eden_regions.bin",
		L"new_eden_regions.bin",
		L"data\\new_eden_regions.bin",
		exeDir + L"..\\..\\data\\new_eden_regions.bin",
	};

	std::string lastError;
	for (const std::wstring& path : candidates)
	{
		if (!FileExists(path))
		{
			continue;
		}
		if (LoadRegions(path, out, error))
		{
			return true;
		}
		lastError = error;
	}
	error = lastError.empty() ? "new_eden_regions.bin not found next to the executable or in data/" : lastError;
	return false;
}

bool ValidateRegions(const RegionTable& regions, const std::vector<uint32_t>& systemIds, std::string& error)
{
	if (regions.records.size() != systemIds.size() || regions.records.size() != kExpectedRegionCount)
	{
		error = "region count does not match the system catalogue";
		return false;
	}
	for (size_t i = 0; i < systemIds.size(); ++i)
	{
		if (regions.records[i].id != systemIds[i])
		{
			std::ostringstream oss;
			oss << "region id mismatch at index " << i << ": " << regions.records[i].id << " vs " << systemIds[i];
			error = oss.str();
			return false;
		}
	}
	if (regions.distinctRegions != kExpectedDistinctRegions)
	{
		error = "region distinct count is not the pinned 70 known-space regions";
		return false;
	}
	bool foundJita = false;
	for (const RegionRecord& rec : regions.records)
	{
		if (rec.id == kJitaSystemId)
		{
			foundJita = true;
			if (rec.regionId != kJitaRegionId)
			{
				error = "Jita region_id is not 10000002 (The Forge)";
				return false;
			}
		}
	}
	if (!foundJita)
	{
		error = "region table is missing Jita";
		return false;
	}
	return true;
}
}
