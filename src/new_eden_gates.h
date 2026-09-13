#pragma once

#include "new_eden_catalog.h"

#include <cstdint>
#include <string>
#include <vector>

namespace neweden
{
struct Edge
{
	uint32_t sourceId = 0;
	uint32_t destId = 0;
};

struct Graph
{
	uint32_t version = 0;
	uint32_t sdeBuild = 0;
	std::string sourceSha256;
	std::string loadedPath;
	std::vector<Edge> edges;
};

bool LoadGraph(const std::wstring& path, Graph& out, std::string& error);
bool FindAndLoadGraph(Graph& out, std::string& error);
bool ValidateGraph(const Catalog& catalog, const Graph& graph, std::string& error);
int HopDistance(const Graph& graph, uint32_t fromId, uint32_t toId);
}