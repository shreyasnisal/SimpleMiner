#pragma once

#include "Game/Block.hpp"
#include "Game/BlockIter.hpp"
#include "Game/BlockTemplate.hpp"

#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"

#include <queue>
#include <vector>


struct Block;
class World;

#include "Engine/Math/IntVec3.hpp"


constexpr int CHUNK_XBITS = 4;
constexpr int CHUNK_YBITS = 4;
constexpr int CHUNK_ZBITS = 7;

constexpr int CHUNK_SIZE_X = 1 << CHUNK_XBITS;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_YBITS;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_ZBITS;

constexpr int CHUNK_BITMASK_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_BITMASK_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_BITMASK_Z = CHUNK_SIZE_Z - 1;

constexpr int CHUNK_BLOCKS_PER_LAYER = 1 << (CHUNK_XBITS + CHUNK_YBITS);
constexpr int CHUNK_BLOCKS_TOTAL = 1 << (CHUNK_XBITS + CHUNK_YBITS + CHUNK_ZBITS);

constexpr int CHUNK_GENERATION_NEIGHBORHOOD_OFFSET = 5;

class Chunk;


enum class ChunkState
{
	MISSING,
	ON_DISK,
	CONSTRUCTING,
	ACTIVATING_QUEUED_LOAD,
	ACTIVATING_LOADING,
	ACTIVATING_LOAD_COMPLETE,
	ACTIVATING_QUEUED_GENERATE,
	ACTIVATING_GENERATING,
	ACTIVATING_GENERATE_COMPLETE,
	ACTIVE,
	DEACTIVATING_QUEUED_SAVE,
	DEACTIVATING_SAVING,
	DEACTIVATING_SAVE_COMPLETE,

	NUM_CHUNK_STATES
};

class ChunkGenerateJob : public Job
{
public:
	ChunkGenerateJob(Chunk* chunk) : m_chunk(chunk) {}
	virtual void Execute() override;

public:
	Chunk* m_chunk = nullptr;
};


class Chunk
{
public:
	~Chunk();
	Chunk() = default;
	Chunk(World* world, IntVec2 const& chunkCoords);

	bool LoadFromFile();
	bool SaveToFile() const;
	void GenerateChunkBlocks();
	void PlaceBlockTemplates();
	void RebuildMesh();

	void Update();
	void Render() const;
	void RenderDebug() const;
	unsigned char ComputeBlockTypeAtIndex(IntVec3 const& neighboorhoodColCoords, int terrainHeights[], float humidityArr[], float temperatureArr[], float forestness[], float treeRawNoise[]);
	IntVec3 GetBlockCoordsFromIndex(int blockIndex) const;
	int GetBlockIndexFromCoords(IntVec3 const& blockCoords) const;
	int GetBlockIndexFromCoords(int blockX, int blockY, int blockZ) const;
	IntVec3 GetBlockCoordsFromWorldPosition(Vec3 const& worldPosition) const;
	bool AreBlockCoordsInChunk(IntVec3 const& blockCoords) const;
	bool AddBlockAtWorldPosition(Vec3 const& worldPosition, BlockDefinitionID type);
	bool DigBlockAtWorldPosition(Vec3 const& worldPosition);
	void AddVertsForBlock(int blockIndex);
	bool IsBlockOpaque(Block const* block) const;
	bool IsLocalMaximum(IntVec2 const& blockCoords, float rawNoise[], int range) const;

public:
	World* m_world = nullptr;
	IntVec2 m_coords;
	Vec3 m_worldPosition;
	AABB3 m_worldBounds;
	Block* m_blocks = nullptr;
	std::vector<Vertex_PCU> m_vertexes;
	VertexBuffer* m_vertexBuffer = nullptr;
	std::vector<Vertex_PCU> m_debugVertexes;
	bool m_isCpuMeshDirty = true;
	bool m_needsSaving = false;
	Chunk* m_eastNeighbor = nullptr;
	Chunk* m_westNeighbor = nullptr;
	Chunk* m_northNeighbor = nullptr;
	Chunk* m_southNeighbor = nullptr;
	int m_chunkRenderedVerts = 0;
	std::vector<BlockTemplateToDo> m_blockTemplateSpawnToDo;
	std::atomic<ChunkState> m_state = ChunkState::CONSTRUCTING;
};
