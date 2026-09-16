#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace neweden
{
struct RegionRecord
{
	uint32_t id = 0;
	uint32_t regionId = 0;
};

struct RegionTable
{
	uint32_t version = 0;
	uint32_t sdeBuild = 0;
	std::string sourceSha256;
	std::string loadedPath;
	uint32_t distinctRegions = 0;
	std::vector<RegionRecord> records;
};

constexpr uint32_t kExpectedRegionCount = 5485;
constexpr uint32_t kExpectedDistinctRegions = 70;
constexpr uint32_t kJitaRegionId = 10000002;
constexpr uint32_t kJitaSystemId = 30000142;

struct Rgb
{
	float r;
	float g;
	float b;
};

Rgb RegionAtlasColor(uint32_t regionId);

bool LoadRegions(const std::wstring& path, RegionTable& out, std::string& error);
bool FindAndLoadRegions(RegionTable& out, std::string& error);
bool ValidateRegions(const RegionTable& regions, const std::vector<uint32_t>& systemIds, std::string& error);
}
