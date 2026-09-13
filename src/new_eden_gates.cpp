#include "new_eden_gates.h"
#include "new_eden_anchors.h"
#include "new_eden_gates_expect.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_map>
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
	uint32_t edgeCount;
	uint32_t sdeBuild;
	uint32_t flags;
	uint8_t sourceSha256[32];
	uint8_t pad[8];
};

struct FileRecord
{
	uint32_t sourceId;
	uint32_t destId;
};
#pragma pack(pop)

static_assert(sizeof(FileHeader) == 64, "NEGATE1 header must stay 64 bytes");
static_assert(sizeof(FileRecord) == 8, "NEGATE1 record must stay 8 bytes");

const char kMagic[8] = { 'N', 'E', 'G', 'A', 'T', 'E', '1', '\0' };
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

using Adjacency = std::unordered_map<uint32_t, std::vector<uint32_t>>;

Adjacency BuildAdjacency(const Graph& graph)
{
	Adjacency adj;
	adj.reserve(graph.edges.size() * 2);
	for (const Edge& edge : graph.edges)
	{
		adj[edge.sourceId].push_back(edge.destId);
		adj[edge.destId].push_back(edge.sourceId);
	}
	return adj;
}

const System* FindSystem(const Catalog& catalog, uint32_t id)
{
	const auto it = std::find_if(
		catalog.systems.begin(),
		catalog.systems.end(),
		[&](const System& system) { return system.id == id; });
	return it == catalog.systems.end() ? nullptr : &(*it);
}

bool ValidateNeighbors(
	const Catalog& catalog,
	const Adjacency& adj,
	uint32_t hubId,
	const char* hubName,
	const NeighborExpect* expected,
	size_t expectedCount,
	std::string& error)
{
	const System* hub = FindSystem(catalog, hubId);
	if (!hub)
	{
		std::ostringstream oss;
		oss << "missing hub system " << hubId << " (" << hubName << ")";
		error = oss.str();
		return false;
	}
	if (hub->name != hubName)
	{
		std::ostringstream oss;
		oss << "hub " << hubId << " name is '" << hub->name << "', expected '" << hubName << "'";
		error = oss.str();
		return false;
	}

	std::unordered_set<uint32_t> got;
	const auto it = adj.find(hubId);
	if (it != adj.end())
	{
		got.insert(it->second.begin(), it->second.end());
	}
	if (got.size() != expectedCount)
	{
		std::ostringstream oss;
		oss << hubName << " degree is " << got.size() << ", expected " << expectedCount;
		error = oss.str();
		return false;
	}
	for (size_t i = 0; i < expectedCount; ++i)
	{
		if (got.find(expected[i].id) == got.end())
		{
			std::ostringstream oss;
			oss << hubName << " is missing neighbour " << expected[i].name << " (" << expected[i].id << ")";
			error = oss.str();
			return false;
		}
		const System* neighbor = FindSystem(catalog, expected[i].id);
		if (!neighbor)
		{
			std::ostringstream oss;
			oss << "missing neighbour system " << expected[i].id << " (" << expected[i].name << ")";
			error = oss.str();
			return false;
		}
		if (neighbor->name != expected[i].name)
		{
			std::ostringstream oss;
			oss << "neighbour " << expected[i].id << " name is '" << neighbor->name << "', expected '"
				<< expected[i].name << "'";
			error = oss.str();
			return false;
		}
	}
	return true;
}
}

int HopDistance(const Graph& graph, uint32_t fromId, uint32_t toId)
{
	if (fromId == toId)
	{
		return 0;
	}
	const Adjacency adj = BuildAdjacency(graph);
	std::unordered_map<uint32_t, int> dist;
	dist.reserve(adj.size());
	std::queue<uint32_t> queue;
	dist[fromId] = 0;
	queue.push(fromId);
	while (!queue.empty())
	{
		const uint32_t current = queue.front();
		queue.pop();
		const int here = dist[current];
		const auto it = adj.find(current);
		if (it == adj.end())
		{
			continue;
		}
		for (uint32_t nxt : it->second)
		{
			if (dist.find(nxt) != dist.end())
			{
				continue;
			}
			dist[nxt] = here + 1;
			if (nxt == toId)
			{
				return dist[nxt];
			}
			queue.push(nxt);
		}
	}
	return -1;
}

bool LoadGraph(const std::wstring& path, Graph& out, std::string& error)
{
	out = Graph();
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
		error = "stargate file is too small";
		return false;
	}

	FileHeader header = {};
	in.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!in)
	{
		error = "failed to read stargate header";
		return false;
	}
	if (std::memcmp(header.magic, kMagic, 8) != 0)
	{
		error = "stargate magic is not NEGATE1";
		return false;
	}
	if (header.version != kVersion || header.recordSize != sizeof(FileRecord))
	{
		error = "unsupported stargate version or record size";
		return false;
	}
	if (header.sdeBuild != kExpectedSdeBuild)
	{
		error = "stargate SDE build does not match the pinned 3464040 artefact";
		return false;
	}
	if (header.flags != 0)
	{
		error = "stargate header flags must be zero";
		return false;
	}
	if (header.edgeCount != kExpectedEdgeCount)
	{
		error = "stargate edge count does not match the pinned 6989 undirected export";
		return false;
	}

	const uint64_t recordsBytes = uint64_t(header.edgeCount) * uint64_t(header.recordSize);
	const uint64_t expectedSize = uint64_t(sizeof(FileHeader)) + recordsBytes;
	if (uint64_t(fileSize) != expectedSize)
	{
		error = "stargate size does not match header + records";
		return false;
	}

	std::vector<FileRecord> records(header.edgeCount);
	if (header.edgeCount > 0)
	{
		in.read(reinterpret_cast<char*>(records.data()), std::streamsize(recordsBytes));
		if (!in)
		{
			error = "failed to read stargate records";
			return false;
		}
	}

	out.version = header.version;
	out.sdeBuild = header.sdeBuild;
	out.sourceSha256 = Hex32(header.sourceSha256);
	out.loadedPath = Narrow(path);
	out.edges.reserve(header.edgeCount);

	std::unordered_set<uint64_t> seen;
	seen.reserve(header.edgeCount);
	for (const FileRecord& rec : records)
	{
		if (rec.sourceId == rec.destId)
		{
			error = "stargate catalogue contains a self-edge";
			return false;
		}
		if (rec.sourceId >= rec.destId)
		{
			error = "stargate catalogue is not stored as sorted undirected pairs";
			return false;
		}
		if (rec.sourceId < 30000000u || rec.sourceId > 30999999u || rec.destId < 30000000u || rec.destId > 30999999u)
		{
			error = "stargate catalogue contains a non-known-space endpoint";
			return false;
		}
		const uint64_t key = (uint64_t(rec.sourceId) << 32) | uint64_t(rec.destId);
		if (!seen.insert(key).second)
		{
			error = "stargate catalogue contains a duplicate undirected edge";
			return false;
		}
		out.edges.push_back({ rec.sourceId, rec.destId });
	}
	return true;
}

bool FindAndLoadGraph(Graph& out, std::string& error)
{
	const std::wstring exeDir = ExeDir();
	const std::wstring candidates[] = {
		exeDir + L"new_eden_stargates.bin",
		L"new_eden_stargates.bin",
		L"data\\new_eden_stargates.bin",
		exeDir + L"..\\..\\data\\new_eden_stargates.bin",
	};

	std::string lastError;
	for (const std::wstring& path : candidates)
	{
		if (!FileExists(path))
		{
			continue;
		}
		if (LoadGraph(path, out, error))
		{
			return true;
		}
		lastError = error;
	}
	error = lastError.empty() ? "new_eden_stargates.bin not found next to the executable or in data/" : lastError;
	return false;
}

bool ValidateGraph(const Catalog& catalog, const Graph& graph, std::string& error)
{
	if (graph.edges.size() != kExpectedEdgeCount)
	{
		error = "loaded stargate count does not match the pinned 6989 undirected export";
		return false;
	}
	if (graph.sourceSha256 != catalog.sourceSha256)
	{
		error = "stargate source sha256 does not match the systems catalogue";
		return false;
	}

	std::unordered_map<uint32_t, const System*> byId;
	byId.reserve(catalog.systems.size());
	for (const System& system : catalog.systems)
	{
		byId.emplace(system.id, &system);
	}

	for (const Edge& edge : graph.edges)
	{
		const auto sourceIt = byId.find(edge.sourceId);
		const auto destIt = byId.find(edge.destId);
		if (sourceIt == byId.end() || destIt == byId.end())
		{
			error = "stargate endpoint is not in the known-space catalogue";
			return false;
		}
		const System& source = *sourceIt->second;
		const System& dest = *destIt->second;
		if (!std::isfinite(source.sceneX) || !std::isfinite(source.sceneY) || !std::isfinite(source.sceneZ) ||
			!std::isfinite(dest.sceneX) || !std::isfinite(dest.sceneY) || !std::isfinite(dest.sceneZ))
		{
			error = "stargate endpoint has a non-finite scene coordinate";
			return false;
		}
	}

	if (byId.find(kNiarjaId) == byId.end())
	{
		error = "Pochven anchor Niarja is missing from the systems catalogue";
		return false;
	}

	const Adjacency adj = BuildAdjacency(graph);
	if (!ValidateNeighbors(catalog, adj, kJitaId, "Jita", kJitaNeighbors, sizeof(kJitaNeighbors) / sizeof(kJitaNeighbors[0]), error) ||
		!ValidateNeighbors(catalog, adj, kAmarrId, "Amarr", kAmarrNeighbors, sizeof(kAmarrNeighbors) / sizeof(kAmarrNeighbors[0]), error) ||
		!ValidateNeighbors(catalog, adj, 30002659u, "Dodixie", kDodixieNeighbors, sizeof(kDodixieNeighbors) / sizeof(kDodixieNeighbors[0]), error) ||
		!ValidateNeighbors(catalog, adj, 30002510u, "Rens", kRensNeighbors, sizeof(kRensNeighbors) / sizeof(kRensNeighbors[0]), error) ||
		!ValidateNeighbors(catalog, adj, 30002053u, "Hek", kHekNeighbors, sizeof(kHekNeighbors) / sizeof(kHekNeighbors[0]), error) ||
		!ValidateNeighbors(catalog, adj, 30100000u, "Zarzakh", kZarzakhNeighbors, sizeof(kZarzakhNeighbors) / sizeof(kZarzakhNeighbors[0]), error))
	{
		return false;
	}

	const int hops = HopDistance(graph, kJitaId, kAmarrId);
	if (hops != int(kExpectedJitaAmarrHops))
	{
		std::ostringstream oss;
		oss << "Jita-Amarr hop count is " << hops << ", expected " << kExpectedJitaAmarrHops;
		error = oss.str();
		return false;
	}
	if (HopDistance(graph, kJitaId, kNiarjaId) != -1)
	{
		error = "Niarja (Pochven) must not be reachable from Jita on the static gate graph";
		return false;
	}

	std::unordered_set<uint32_t> seen;
	seen.reserve(kExpectedJitaReachable);
	std::queue<uint32_t> queue;
	seen.insert(kJitaId);
	queue.push(kJitaId);
	while (!queue.empty())
	{
		const uint32_t current = queue.front();
		queue.pop();
		const auto it = adj.find(current);
		if (it == adj.end())
		{
			continue;
		}
		for (uint32_t nxt : it->second)
		{
			if (seen.insert(nxt).second)
			{
				queue.push(nxt);
			}
		}
	}
	if (seen.size() != kExpectedJitaReachable)
	{
		std::ostringstream oss;
		oss << "Jita reachable count is " << seen.size() << ", expected " << kExpectedJitaReachable;
		error = oss.str();
		return false;
	}
	return true;
}
}
