#include "new_eden_catalog.h"
#include "new_eden_anchors.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace neweden
{
namespace
{
#pragma pack(push, 1)
struct FileHeader
{
	char magic[8];
	uint16_t version;
	uint16_t recordSize;
	uint32_t systemCount;
	uint32_t sdeBuild;
	uint32_t stringsSize;
	uint32_t knownSpaceCount;
	uint32_t otherSpaceCount;
	uint8_t sourceSha256[32];
};

struct FileRecord
{
	uint32_t id;
	float dbX;
	float dbY;
	float dbZ;
	uint32_t nameOffset;
	uint16_t nameLength;
	uint8_t space;
	uint8_t pad;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 64, "NEDEN1B header must stay 64 bytes");
static_assert(sizeof(FileRecord) == 24, "NEDEN1B record must stay 24 bytes");

const char kMagic[8] = { 'N', 'E', 'D', 'E', 'N', '1', 'B', '\0' };
const uint16_t kVersion = 1;
const uint32_t kMinKnownSpace = 5000;
const uint32_t kMaxKnownSpace = 6000;
const float kAnchorEpsilon = 1.0e-4f;

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

bool LoadCatalog(const std::wstring& path, Catalog& out, std::string& error)
{
	out = Catalog();
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		error = "could not open " + Narrow(path);
		return false;
	}

	in.seekg(0, std::ios::end);
	const std::streamoff fileSize = in.tellg();
	in.seekg(0, std::ios::beg);
	if (fileSize < std::streamoff(sizeof(FileHeader)))
	{
		error = "catalogue file is too small";
		return false;
	}

	FileHeader header = {};
	in.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!in)
	{
		error = "failed to read catalogue header";
		return false;
	}
	if (std::memcmp(header.magic, kMagic, 8) != 0)
	{
		error = "catalogue magic is not NEDEN1B";
		return false;
	}
	if (header.version != kVersion || header.recordSize != sizeof(FileRecord))
	{
		error = "unsupported catalogue version or record size";
		return false;
	}
	if (header.sdeBuild != kExpectedSdeBuild)
	{
		error = "catalogue SDE build does not match the pinned 3464040 artefact";
		return false;
	}
	if (header.systemCount != header.knownSpaceCount || header.otherSpaceCount != 0)
	{
		error = "catalogue is not a known-space-only New Eden export";
		return false;
	}
	if (header.systemCount < kMinKnownSpace || header.systemCount > kMaxKnownSpace)
	{
		error = "catalogue system count is outside the expected New Eden range";
		return false;
	}
	if (header.systemCount != kExpectedSystemCount || header.knownSpaceCount != kExpectedKnownSpaceCount)
	{
		error = "catalogue system count does not match the pinned 5485 known-space export";
		return false;
	}

	const uint64_t recordsBytes = uint64_t(header.systemCount) * uint64_t(header.recordSize);
	const uint64_t expectedSize = uint64_t(sizeof(FileHeader)) + recordsBytes + uint64_t(header.stringsSize);
	if (uint64_t(fileSize) != expectedSize)
	{
		error = "catalogue size does not match header + records + string table";
		return false;
	}

	std::vector<FileRecord> records(header.systemCount);
	if (header.systemCount > 0)
	{
		in.read(reinterpret_cast<char*>(records.data()), std::streamsize(recordsBytes));
		if (!in)
		{
			error = "failed to read catalogue records";
			return false;
		}
	}

	std::string strings(header.stringsSize, '\0');
	if (header.stringsSize > 0)
	{
		in.read(strings.data(), std::streamsize(header.stringsSize));
		if (!in)
		{
			error = "failed to read catalogue string table";
			return false;
		}
	}

	out.version = header.version;
	out.sdeBuild = header.sdeBuild;
	out.knownSpaceCount = header.knownSpaceCount;
	out.otherSpaceCount = header.otherSpaceCount;
	out.sourceSha256 = Hex32(header.sourceSha256);
	out.loadedPath = Narrow(path);
	out.systems.reserve(header.systemCount);

	std::unordered_set<uint32_t> seen;
	seen.reserve(header.systemCount);
	for (const FileRecord& rec : records)
	{
		if (rec.space != 0)
		{
			error = "catalogue contains a non-known-space record";
			return false;
		}
		if (rec.id < 30000000u || rec.id > 30999999u)
		{
			error = "catalogue contains a system id outside New Eden known-space";
			return false;
		}
		if (!std::isfinite(rec.dbX) || !std::isfinite(rec.dbY) || !std::isfinite(rec.dbZ))
		{
			error = "catalogue contains a non-finite coordinate";
			return false;
		}
		if (!seen.insert(rec.id).second)
		{
			error = "catalogue contains a duplicate system id";
			return false;
		}
		if (uint64_t(rec.nameOffset) + uint64_t(rec.nameLength) > uint64_t(header.stringsSize) || rec.nameLength == 0)
		{
			error = "catalogue name offset is out of range";
			return false;
		}

		System system;
		system.id = rec.id;
		system.dbX = rec.dbX;
		system.dbY = rec.dbY;
		system.dbZ = rec.dbZ;
		DbToScene(rec.dbX, rec.dbY, rec.dbZ, system.sceneX, system.sceneY, system.sceneZ);
		system.name.assign(strings.data() + rec.nameOffset, rec.nameLength);
		out.systems.push_back(std::move(system));
	}

	if (out.systems.size() != header.systemCount)
	{
		error = "catalogue record count mismatch after validation";
		return false;
	}
	return true;
}

bool FindAndLoadCatalog(Catalog& out, std::string& error)
{
	const std::wstring exeDir = ExeDir();
	const std::wstring candidates[] = {
		exeDir + L"new_eden_systems.bin",
		L"new_eden_systems.bin",
		L"data\\new_eden_systems.bin",
		exeDir + L"..\\..\\data\\new_eden_systems.bin",
	};

	std::string lastError;
	for (const std::wstring& path : candidates)
	{
		if (!FileExists(path))
		{
			continue;
		}
		if (LoadCatalog(path, out, error))
		{
			return true;
		}
		lastError = error;
	}
	error = lastError.empty() ? "new_eden_systems.bin not found next to the executable or in data/" : lastError;
	return false;
}

bool ValidateAnchors(const Catalog& catalog, std::string& error)
{
	for (const AnchorExpect& expect : kAnchors)
	{
		const auto it = std::find_if(
			catalog.systems.begin(),
			catalog.systems.end(),
			[&](const System& system) { return system.id == expect.id; });
		if (it == catalog.systems.end())
		{
			std::ostringstream oss;
			oss << "missing anchor system " << expect.id << " (" << expect.name << ")";
			error = oss.str();
			return false;
		}
		if (it->name != expect.name)
		{
			std::ostringstream oss;
			oss << "anchor " << expect.id << " name is '" << it->name << "', expected '" << expect.name << "'";
			error = oss.str();
			return false;
		}
		const float dx = it->sceneX - expect.x;
		const float dy = it->sceneY - expect.y;
		const float dz = it->sceneZ - expect.z;
		if (std::fabs(dx) > kAnchorEpsilon || std::fabs(dy) > kAnchorEpsilon || std::fabs(dz) > kAnchorEpsilon)
		{
			std::ostringstream oss;
			oss << "anchor " << expect.name << " (" << expect.id << ") scene coord mismatch: got "
				<< it->sceneX << "," << it->sceneY << "," << it->sceneZ << " expected " << expect.x << ","
				<< expect.y << "," << expect.z;
			error = oss.str();
			return false;
		}
	}
	return true;
}
}
