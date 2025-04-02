#pragma once

#include "Game/BlockIter.hpp"

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/RaycastUtils.hpp"

#include <map>
#include <set>
#include <string>
#include <queue>

class Chunk;
class Game;


struct SimpleMinerRaycastResult : public RaycastResult3D
{
public:
	BlockIter m_impactBlockIter = BlockIter(nullptr, -1);
};


class World
{
public:
	~World();
	World() = default;
	World(Game* game);

	void Update();
	void Render() const;

	void UpdateSimpleMinerShaderConstants();


	std::string GetSaveFilePath() const;
	int GetChunkFileVersion() const;

	void HandleChunkActivationDeactivation();
	void DeactivateChunk(IntVec2 const& chunkCoords);
	void RequestChunkActivation(IntVec2 const& chunkCoords);
	bool RequestActivationOfNearestChunkInRange(float range);
	bool DeactivateFarthestChunkOutOfRange(float range);
	void ActivateChunk(Chunk* chunk);
	void DirtyChunkLighting(Chunk* chunk);

	Chunk* GetChunkAtCoords(IntVec2 const& chunkCoords) const;
	Chunk* GetChunkForWorldPosition(Vec3 const& worldPosition) const;

	SimpleMinerRaycastResult RaycastVsBlocks(Vec3 const& startPosition, Vec3 const& direction, float maxDistance) const;

	void MarkBlockLightingDirty(BlockIter blockIter);
	void ProcessDirtyLighting();
	void ProcessNextDirtyLightBlock();

public:
	Game* m_game = nullptr;
	int m_worldSeed = 0;
	std::map<IntVec2, Chunk*> m_activeChunks;
	std::set<IntVec2> m_chunkCoordsQueuedForActivation;
	std::queue<BlockIter> m_dirtyLightingQueue;
	Shader* m_shader = nullptr;
	ConstantBuffer* m_shaderConstants = nullptr;
	float m_worldTime = 0.5f;
	float m_worldTimeScale = 200.f;
	Rgba8 m_skyColor = Rgba8::BLACK;
	Rgba8 m_indoorLightColor = Rgba8(255, 230, 204, 255);
	Rgba8 m_outdoorLightColor = Rgba8::WHITE;
	int m_totalRenderedVerts = 0;
	bool m_disableLighting = false;;
	bool m_disableLightning = false;
	bool m_isWorldTimeFixedToDay = false;
	bool m_disableWorldShader = false;
};
