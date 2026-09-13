#include "new_eden_star_visuals.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace neweden
{
namespace
{
#pragma pack(push, 1)
struct VisualHeader
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

struct VisualRecord
{
	uint32_t id;
	float temperatureK;
	uint8_t spectralLetter;
	uint8_t pad[3];
};
#pragma pack(pop)

static_assert(sizeof(VisualHeader) == 64, "NESTAR1 header must stay 64 bytes");
static_assert(sizeof(VisualRecord) == 12, "NESTAR1 record must stay 12 bytes");

const char kMagic[8] = { 'N', 'E', 'S', 'T', 'A', 'R', '1', '\0' };
const uint16_t kVersion = 1;

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

bool LoadStarVisuals(const std::wstring& path, StarVisuals& out, std::string& error)
{
	out = StarVisuals();
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		error = "could not open " + Narrow(path);
		return false;
	}

	in.seekg(0, std::ios::end);
	const std::streamoff fileSize = in.tellg();
	in.seekg(0, std::ios::beg);
	if (fileSize < std::streamoff(sizeof(VisualHeader)))
	{
		error = "star visuals file is too small";
		return false;
	}

	VisualHeader header = {};
	in.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!in)
	{
		error = "failed to read star visuals header";
		return false;
	}
	if (std::memcmp(header.magic, kMagic, 8) != 0)
	{
		error = "star visuals magic is not NESTAR1";
		return false;
	}
	if (header.version != kVersion || header.recordSize != sizeof(VisualRecord))
	{
		error = "unsupported star visuals version or record size";
		return false;
	}
	if (header.count != kExpectedStarVisualCount)
	{
		error = "star visuals count does not match the pinned 5485 known-space export";
		return false;
	}

	const uint64_t recordsBytes = uint64_t(header.count) * uint64_t(header.recordSize);
	if (uint64_t(fileSize) != uint64_t(sizeof(VisualHeader)) + recordsBytes)
	{
		error = "star visuals size does not match header + records";
		return false;
	}

	std::vector<VisualRecord> records(header.count);
	in.read(reinterpret_cast<char*>(records.data()), std::streamsize(recordsBytes));
	if (!in)
	{
		error = "failed to read star visuals records";
		return false;
	}

	out.version = header.version;
	out.sdeBuild = header.sdeBuild;
	out.sourceSha256 = Hex32(header.sourceSha256);
	out.loadedPath = Narrow(path);
	out.records.reserve(header.count);
	out.minTemperatureK = 1.0e9f;
	out.maxTemperatureK = 0.0f;

	std::unordered_set<uint32_t> seen;
	seen.reserve(header.count);
	for (const VisualRecord& rec : records)
	{
		if (rec.id < 30000000u || rec.id > 30999999u)
		{
			error = "star visuals contains a system id outside New Eden known-space";
			return false;
		}
		if (!std::isfinite(rec.temperatureK) || rec.temperatureK <= 0.0f)
		{
			error = "star visuals contains a non-finite or non-positive temperature";
			return false;
		}
		if (!seen.insert(rec.id).second)
		{
			error = "star visuals contains a duplicate system id";
			return false;
		}
		StarVisual visual;
		visual.id = rec.id;
		visual.temperatureK = rec.temperatureK;
		visual.spectralLetter = char(rec.spectralLetter);
		out.minTemperatureK = (std::min)(out.minTemperatureK, rec.temperatureK);
		out.maxTemperatureK = (std::max)(out.maxTemperatureK, rec.temperatureK);
		out.records.push_back(visual);
	}
	return true;
}

bool FindAndLoadStarVisuals(StarVisuals& out, std::string& error)
{
	const std::wstring exeDir = ExeDir();
	const std::wstring candidates[] = {
		exeDir + L"new_eden_star_visuals.bin",
		L"new_eden_star_visuals.bin",
		L"data\\new_eden_star_visuals.bin",
		exeDir + L"..\\..\\data\\new_eden_star_visuals.bin",
	};

	std::string lastError;
	for (const std::wstring& path : candidates)
	{
		if (!FileExists(path))
		{
			continue;
		}
		if (LoadStarVisuals(path, out, error))
		{
			return true;
		}
		lastError = error;
	}
	error = lastError.empty() ? "new_eden_star_visuals.bin not found next to the executable or in data/" : lastError;
	return false;
}

bool ValidateStarVisuals(const StarVisuals& visuals, const std::vector<uint32_t>& systemIds, std::string& error)
{
	if (visuals.records.size() != systemIds.size() || visuals.records.size() != kExpectedStarVisualCount)
	{
		error = "star visuals count does not match the system catalogue";
		return false;
	}
	for (size_t i = 0; i < systemIds.size(); ++i)
	{
		if (visuals.records[i].id != systemIds[i])
		{
			std::ostringstream oss;
			oss << "star visuals id mismatch at index " << i << ": " << visuals.records[i].id << " vs " << systemIds[i];
			error = oss.str();
			return false;
		}
	}
	if (std::fabs(visuals.minTemperatureK - kExpectedTempMinK) > 0.5f ||
		std::fabs(visuals.maxTemperatureK - kExpectedTempMaxK) > 0.5f)
	{
		error = "star temperature window does not match the pinned 2010-7496 K range";
		return false;
	}

	bool foundJita = false;
	for (const StarVisual& visual : visuals.records)
	{
		if (visual.id == kJitaVisualId)
		{
			foundJita = true;
			if (std::fabs(visual.temperatureK - kJitaTemperatureK) > 0.5f)
			{
				error = "Jita star_temperature does not match the pinned 7305 K value";
				return false;
			}
			if (visual.spectralLetter != 'F')
			{
				error = "Jita spectral letter is not F";
				return false;
			}
		}
	}
	if (!foundJita)
	{
		error = "star visuals is missing Jita";
		return false;
	}
	return true;
}
}
