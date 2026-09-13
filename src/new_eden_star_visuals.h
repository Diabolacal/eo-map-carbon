#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace neweden
{
struct StarVisual
{
	uint32_t id = 0;
	float temperatureK = 0.0f;
	char spectralLetter = 0;
};

struct StarVisuals
{
	uint32_t version = 0;
	uint32_t sdeBuild = 0;
	std::string sourceSha256;
	std::string loadedPath;
	float minTemperatureK = 0.0f;
	float maxTemperatureK = 0.0f;
	std::vector<StarVisual> records;
};

constexpr uint32_t kExpectedStarVisualCount = 5485;
constexpr float kExpectedTempMinK = 2010.0f;
constexpr float kExpectedTempMaxK = 7496.0f;
constexpr float kJitaTemperatureK = 7305.0f;
constexpr uint32_t kJitaVisualId = 30000142;

bool LoadStarVisuals(const std::wstring& path, StarVisuals& out, std::string& error);
bool FindAndLoadStarVisuals(StarVisuals& out, std::string& error);
bool ValidateStarVisuals(const StarVisuals& visuals, const std::vector<uint32_t>& systemIds, std::string& error);
}
