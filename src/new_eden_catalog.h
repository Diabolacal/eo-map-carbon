#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace neweden
{
struct System
{
	uint32_t id = 0;
	float dbX = 0.0f;
	float dbY = 0.0f;
	float dbZ = 0.0f;
	float sceneX = 0.0f;
	float sceneY = 0.0f;
	float sceneZ = 0.0f;
	std::string name;
};

struct Catalog
{
	uint32_t version = 0;
	uint32_t sdeBuild = 0;
	uint32_t knownSpaceCount = 0;
	uint32_t otherSpaceCount = 0;
	std::string sourceSha256;
	std::string loadedPath;
	std::vector<System> systems;
};

// Contract A DB light-years -> EO-Map / three.js world.
// Matches eve-frontier-map/src/utils/universeCoordinates.ts dbToScenePosition.
inline void DbToScene(float dbX, float dbY, float dbZ, float& sceneX, float& sceneY, float& sceneZ)
{
	sceneX = dbX;
	sceneY = -dbZ;
	sceneZ = -dbY;
}

bool LoadCatalog(const std::wstring& path, Catalog& out, std::string& error);
bool FindAndLoadCatalog(Catalog& out, std::string& error);
bool ValidateAnchors(const Catalog& catalog, std::string& error);
}
