#include "Game/Chunk.hpp"

#include "Game/Block.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/World.hpp"
#include "Game/BlockIter.hpp"

#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"
#include "ThirdParty/Squirrel/RawNoise.hpp"


constexpr int SEA_LEVEL = 64;
constexpr int RIVER_MAX_DEPTH = 5;
constexpr int RIVER_BED = SEA_LEVEL - RIVER_MAX_DEPTH;
constexpr int MAX_MOUNTAIN_HEIGHT = CHUNK_SIZE_Z;
constexpr int MAX_OCEAN_DEPTH = 30;

Chunk::~Chunk()
{
	if (m_needsSaving)
	{
		m_state = ChunkState::DEACTIVATING_SAVING;
		SaveToFile();
	}

	if (m_eastNeighbor)
	{
		m_eastNeighbor->m_westNeighbor = nullptr;
	}
	if (m_westNeighbor)
	{
		m_westNeighbor->m_eastNeighbor = nullptr;
	}
	if (m_northNeighbor)
	{
		m_northNeighbor->m_southNeighbor = nullptr;
	}
	if (m_southNeighbor)
	{
		m_southNeighbor->m_northNeighbor = nullptr;
	}

	delete m_vertexBuffer;
	m_vertexBuffer = nullptr;
	delete m_blocks;
	m_blocks = nullptr;

	m_state = ChunkState::DEACTIVATING_SAVE_COMPLETE;
}

Chunk::Chunk(World* world, IntVec2 const& chunkCoords)
	: m_world(world)
	, m_coords(chunkCoords)
	, m_worldPosition((float)(chunkCoords.x * CHUNK_SIZE_X), float(chunkCoords.y * CHUNK_SIZE_Y), 0.f)
	, m_worldBounds(m_worldPosition, m_worldPosition + Vec3::EAST * CHUNK_SIZE_X + Vec3::NORTH * CHUNK_SIZE_Y + Vec3::SKYWARD * CHUNK_SIZE_Z)
{
	m_blocks = new Block[CHUNK_BLOCKS_TOTAL];
}

bool Chunk::LoadFromFile()
{
	m_state = ChunkState::ACTIVATING_LOADING;

	std::string chunkFilePath = m_world->GetSaveFilePath() + Stringf("/Chunk(%i,%i).chunk", m_coords.x, m_coords.y);
	std::vector<uint8_t> chunkFileContents;
	int numBytesRead = FileReadToBuffer(chunkFileContents, chunkFilePath);

	if (numBytesRead < 8)
	{
		return false;
	}

	GUARANTEE_OR_DIE(chunkFileContents[0] == 'G', Stringf("Invalid chunk file provided for chunk(%d, %d)", m_coords.x, m_coords.y));
	GUARANTEE_OR_DIE(chunkFileContents[1] == 'C', Stringf("Invalid chunk file provided for chunk(%d, %d)", m_coords.x, m_coords.y));
	GUARANTEE_OR_DIE(chunkFileContents[2] == 'H', Stringf("Invalid chunk file provided for chunk(%d, %d)", m_coords.x, m_coords.y));
	GUARANTEE_OR_DIE(chunkFileContents[3] == 'K', Stringf("Invalid chunk file provided for chunk(%d, %d)", m_coords.x, m_coords.y));

	int chunkFileVersion = chunkFileContents[4];
	int fileChunkBitsX = chunkFileContents[5];
	int fileChunkBitsY = chunkFileContents[6];
	int fileChunkBitsZ = chunkFileContents[7];

	uint8_t worldSeed0 = chunkFileContents[8];
	uint8_t worldSeed1 = chunkFileContents[9];
	uint8_t worldSeed2 = chunkFileContents[10];
	uint8_t worldSeed3 = chunkFileContents[11];

	int worldSeedInChunkFile = (int)(worldSeed0 | (worldSeed1 << 8) | (worldSeed2 << 16) | (worldSeed3 << 24));

	if (chunkFileVersion != m_world->GetChunkFileVersion())
	{
		g_console->AddLine(Stringf("Version mismatch for chunk file (%d, %d). File will be ignored!", m_coords.x, m_coords.y));
		return false;
	}

	if (worldSeedInChunkFile != m_world->m_worldSeed)
	{
		g_console->AddLine(Stringf("World seed mismatch for chunk file (%d, %d). Chunk file world seed was %d, current world seed is %d", m_coords.x, m_coords.y, worldSeedInChunkFile, m_world->m_worldSeed));
		return false;
	}

	if (fileChunkBitsX != CHUNK_XBITS || fileChunkBitsY != CHUNK_YBITS || fileChunkBitsZ != CHUNK_ZBITS)
	{
		g_console->AddLine(Stringf("Chunk size mismatch in code and chunk file. File will be ignored!"));
		return false;
	}

	int currentBlockIndex = 0;
	for (int chunkFileContentIndex = 12; chunkFileContentIndex < (int)chunkFileContents.size(); chunkFileContentIndex += 2)
	{
		uint8_t blockType = chunkFileContents[chunkFileContentIndex];
		int numBlocks = chunkFileContents[chunkFileContentIndex + 1];

		while (numBlocks--)
		{
			m_blocks[currentBlockIndex] = Block(blockType);
			currentBlockIndex++;
		}
	}

	m_state = ChunkState::ACTIVATING_LOAD_COMPLETE;
	return true;
}

bool Chunk::SaveToFile() const
{
	uint8_t currentBlockType = m_blocks[0].m_type;
	std::vector<uint8_t> fileBuffer;

	fileBuffer.push_back('G');
	fileBuffer.push_back('C');
	fileBuffer.push_back('H');
	fileBuffer.push_back('K');
	fileBuffer.push_back((uint8_t)m_world->GetChunkFileVersion());
	fileBuffer.push_back((uint8_t)CHUNK_XBITS);
	fileBuffer.push_back((uint8_t)CHUNK_YBITS);
	fileBuffer.push_back((uint8_t)CHUNK_ZBITS);

	fileBuffer.push_back((uint8_t)(m_world->m_worldSeed));
	fileBuffer.push_back((uint8_t)(m_world->m_worldSeed >> 8));
	fileBuffer.push_back((uint8_t)(m_world->m_worldSeed >> 16));
	fileBuffer.push_back((uint8_t)(m_world->m_worldSeed >> 24));

	int numBlocksWritten = 0;
	uint8_t currentNumBlocks = 1;
	for (int blockIndex = 1; blockIndex < CHUNK_BLOCKS_TOTAL; blockIndex++)
	{
		if (currentNumBlocks == UINT8_MAX || m_blocks[blockIndex].m_type != currentBlockType)
		{
			fileBuffer.push_back(currentBlockType);
			fileBuffer.push_back(currentNumBlocks);
			numBlocksWritten += currentNumBlocks;
			currentNumBlocks = 1;
			currentBlockType = m_blocks[blockIndex].m_type;
		}
		else
		{
			currentNumBlocks++;
		}
	}
	fileBuffer.push_back(currentBlockType);
	fileBuffer.push_back(currentNumBlocks);

	numBlocksWritten += currentNumBlocks;
	GUARANTEE_OR_DIE(numBlocksWritten == CHUNK_BLOCKS_TOTAL, "Could not write all blocks to file");

	std::string fileName = m_world->GetSaveFilePath() + Stringf("Chunk(%d,%d).chunk", m_coords.x, m_coords.y);
	FileWriteBuffer(fileName, fileBuffer);

	return true;
}

void Chunk::GenerateChunkBlocks()
{
	constexpr int CHUNK_COLUMNS_GRIDSIZEX = CHUNK_SIZE_X + CHUNK_GENERATION_NEIGHBORHOOD_OFFSET * 2;
	constexpr int CHUNK_COLUMNS_GRIDSIZEY = CHUNK_SIZE_Y + CHUNK_GENERATION_NEIGHBORHOOD_OFFSET * 2;

	constexpr int NEIGHBORHOOD_ARRAYSIZE = CHUNK_COLUMNS_GRIDSIZEX * CHUNK_COLUMNS_GRIDSIZEY;

	int terrainHeights[NEIGHBORHOOD_ARRAYSIZE] = {};
	float humidity[NEIGHBORHOOD_ARRAYSIZE] = {};
	float temperature[NEIGHBORHOOD_ARRAYSIZE] = {};
	float forestness[NEIGHBORHOOD_ARRAYSIZE] = {};
	float treeRawNoise[NEIGHBORHOOD_ARRAYSIZE] = {};

	int worldSeed = m_world->m_worldSeed;

	int terrainHeightSeed = worldSeed + 1;
	int humiditySeed = worldSeed + 2;
	int temperatureSeed = worldSeed + 3;
	int hillinessSeed = worldSeed + 4;
	int oceannessSeed = worldSeed + 5;
	int forestnessSeed = worldSeed + 6;
	int treeRawNoiseSeed = worldSeed + 7;

	for (int neighborhoodY = 0; neighborhoodY < CHUNK_COLUMNS_GRIDSIZEY; neighborhoodY++)
	{
		for (int neighborhoodX = 0; neighborhoodX < CHUNK_COLUMNS_GRIDSIZEX; neighborhoodX++)
		{
			int localX = neighborhoodX - CHUNK_GENERATION_NEIGHBORHOOD_OFFSET;
			int localY = neighborhoodY - CHUNK_GENERATION_NEIGHBORHOOD_OFFSET;

			int globalX = (int)m_worldPosition.x + localX;
			int globalY = (int)m_worldPosition.y + localY;
			float fGlobalX = (float)globalX;
			float fGlobalY = (float)globalY;

			int columnIndex = (neighborhoodY * CHUNK_COLUMNS_GRIDSIZEX) + neighborhoodX;

			float terrainHeightPerlinAbs = fabsf(Compute2dPerlinNoise(fGlobalX, fGlobalY, 200.f, 5, 0.5f, 2.0f, false, terrainHeightSeed));
			int rawTerrainHeight = (int)RangeMapClamped(terrainHeightPerlinAbs, 0.f, 1.f, RIVER_BED, MAX_MOUNTAIN_HEIGHT);
			
			humidity[columnIndex] = 0.5f + 0.5f * Compute2dPerlinNoise(fGlobalX, fGlobalY, 200.f, 5, 0.5f, 2.f, false, humiditySeed);
			temperature[neighborhoodY * CHUNK_COLUMNS_GRIDSIZEX + neighborhoodX] = 0.5f + 0.5f * Compute2dPerlinNoise(fGlobalX, fGlobalY, 200.f, 5, 0.5f, 2.f, false, temperatureSeed);
		
			float hilliness = 0.5f + 0.5f * Compute2dPerlinNoise(fGlobalX, fGlobalY, 500.f, 5, 0.5f, 2.f, false, hillinessSeed);
			hilliness = SmoothStep(hilliness);

			if (rawTerrainHeight > SEA_LEVEL)
			{
				int heightAboveSeaLevel = rawTerrainHeight - SEA_LEVEL;
				float hillinessAffectedHeight = (float)heightAboveSeaLevel * hilliness;
				terrainHeights[columnIndex] = SEA_LEVEL + (int)hillinessAffectedHeight;
			}
			else
			{
				terrainHeights[columnIndex] = rawTerrainHeight;
			}

			float oceanness = 0.5f + 0.5f * Compute2dPerlinNoise(fGlobalX, fGlobalY, 2000.f, 5, 0.5f, 2.f, false, oceannessSeed);
			oceanness = SmoothStep(SmoothStep(oceanness));
			terrainHeights[columnIndex] -= int((float)MAX_OCEAN_DEPTH * oceanness);

			forestness[columnIndex] = 0.5f + 0.5f * Compute2dPerlinNoise(fGlobalX, fGlobalY, 1000.f, 5, 0.5f, 2.f, false, forestnessSeed);
			forestness[columnIndex] = EaseOutQuartic(forestness[columnIndex]);
			treeRawNoise[columnIndex] = Get2dNoiseZeroToOne(globalX, globalY, treeRawNoiseSeed);
		}
	}

	for (int blockZ = 0; blockZ < CHUNK_SIZE_Z; blockZ++)
	{
		for (int neighborhoodY = 0; neighborhoodY < CHUNK_COLUMNS_GRIDSIZEY; neighborhoodY++)
		{
			for (int neighborhoodX = 0; neighborhoodX < CHUNK_COLUMNS_GRIDSIZEX; neighborhoodX++)
			{
				IntVec3 neighborhoodColCoords(neighborhoodX, neighborhoodY, blockZ);
				unsigned char blockType = ComputeBlockTypeAtIndex(neighborhoodColCoords, terrainHeights, humidity, temperature, forestness, treeRawNoise);

				int localX = neighborhoodX - CHUNK_GENERATION_NEIGHBORHOOD_OFFSET;
				int localY = neighborhoodY - CHUNK_GENERATION_NEIGHBORHOOD_OFFSET;
				IntVec3 localCoords = IntVec3(localX, localY, blockZ);

				if (AreBlockCoordsInChunk(localCoords))
				{
					int blockIndex = GetBlockIndexFromCoords(localCoords);
					m_blocks[blockIndex] = Block(blockType);
				}
			}
		}
	}
}

void Chunk::PlaceBlockTemplates()
{
	for (int blockTemplateIdx = 0; blockTemplateIdx < (int)m_blockTemplateSpawnToDo.size(); blockTemplateIdx++)
	{
		//BlockTemplateToDo const& blockTemplate = m_blockTemplateSpawnToDo[blockTemplateIdx];
		IntVec3 const& root = m_blockTemplateSpawnToDo[blockTemplateIdx].m_root;
		BlockTemplate const* blockTemplate = m_blockTemplateSpawnToDo[blockTemplateIdx].m_blockTemplate;
		for (int blockIndex = 0; blockIndex < (int)blockTemplate->m_blockTemplateEntries.size(); blockIndex++)
		{
			IntVec3 blockCoords = root + blockTemplate->m_blockTemplateEntries[blockIndex].m_offset;
			if (AreBlockCoordsInChunk(blockCoords))
			{
				m_blocks[GetBlockIndexFromCoords(blockCoords)].SetTypeID(blockTemplate->m_blockTemplateEntries[blockIndex].m_blockType);
			}
		}
	}

	m_blockTemplateSpawnToDo.clear();
}

extern double g_chunkMeshRebuildTime;
extern int g_numChunkMeshesRebuilt;
extern double g_blockVertexesAddingTime;
void Chunk::RebuildMesh()
{
	if (!m_blocks)
	{
		return;
	}

	double meshRebuildStartTime = GetCurrentTimeSeconds();
	g_numChunkMeshesRebuilt++;

	m_vertexes.clear();
	m_vertexes.reserve(CHUNK_BLOCKS_PER_LAYER * 6);
	m_chunkRenderedVerts = 0;

	double blockVertexesAddingStartTime = GetCurrentTimeSeconds();
	for (int blockIndex = 0; blockIndex < CHUNK_BLOCKS_TOTAL; blockIndex++)
	{
		if (m_blocks[blockIndex].IsVisible())
		{
			AddVertsForBlock(blockIndex);
		}
	}
	double blockVertexesAddingEndTime = GetCurrentTimeSeconds();
	g_blockVertexesAddingTime = (blockVertexesAddingEndTime - blockVertexesAddingStartTime) * 1000.f;

	//AddVertsForAABB3(m_debugVertexes, m_worldBounds, Rgba8::MAGENTA);

	if (!m_vertexBuffer)
	{
		m_vertexBuffer = g_renderer->CreateVertexBuffer(m_vertexes.size() * sizeof(Vertex_PCU));
	}

	g_renderer->CopyCPUToGPU(m_vertexes.data(), m_vertexes.size() * sizeof(Vertex_PCU), m_vertexBuffer);
	m_isCpuMeshDirty = false;

	double meshRebuildEndTime = GetCurrentTimeSeconds();
	g_chunkMeshRebuildTime = (meshRebuildEndTime - meshRebuildStartTime) * 1000.f;
}

void Chunk::Update()
{
	m_world->m_totalRenderedVerts += m_chunkRenderedVerts;
}

void Chunk::Render() const
{
	if (!m_vertexBuffer)
	{
		return;
	}

	g_renderer->SetBlendMode(BlendMode::OPAQUE);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetModelConstants();
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(g_spritesheet->GetTexture());
	g_renderer->DrawVertexBuffer(m_vertexBuffer, (int)m_vertexes.size());
	
	if (m_world->m_game->m_drawDebug)
	{
		RenderDebug();
	}
}

void Chunk::RenderDebug() const
{
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::ENABLED);
	g_renderer->SetModelConstants();
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::WIREFRAME);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(m_debugVertexes);
}

IntVec3 Chunk::GetBlockCoordsFromIndex(int blockIndex) const
{
	int x = (blockIndex & CHUNK_BITMASK_X);
	int y = (blockIndex >> CHUNK_XBITS) & CHUNK_BITMASK_Y;
	int z = (blockIndex >> (CHUNK_XBITS + CHUNK_YBITS));
	return IntVec3(x, y, z);
}

int Chunk::GetBlockIndexFromCoords(IntVec3 const& blockCoords) const
{
	return (blockCoords.x | (blockCoords.y << CHUNK_XBITS) | (blockCoords.z << (CHUNK_XBITS + CHUNK_YBITS)));
}

int Chunk::GetBlockIndexFromCoords(int blockX, int blockY, int blockZ) const
{
	return (blockX | (blockY << CHUNK_XBITS) | (blockZ << (CHUNK_XBITS + CHUNK_YBITS)));;
}

unsigned char Chunk::ComputeBlockTypeAtIndex(IntVec3 const& neighborCoords, int terrainHeights[], float humidityArr[], float temperatureArr[], float forestness[], float treeRawNoise[])
{
	static BlockDefinitionID airBlockID = BlockDefinition::GetBlockIDByName("air");
	static BlockDefinitionID waterBlockID = BlockDefinition::GetBlockIDByName("water");
	static BlockDefinitionID grassBlockID = BlockDefinition::GetBlockIDByName("grass");
	static BlockDefinitionID dirtBlockID = BlockDefinition::GetBlockIDByName("dirt");
	static BlockDefinitionID stoneBlockID = BlockDefinition::GetBlockIDByName("stone");
	static BlockDefinitionID coalBlockID = BlockDefinition::GetBlockIDByName("coal");
	static BlockDefinitionID ironBlockID = BlockDefinition::GetBlockIDByName("iron");
	static BlockDefinitionID goldBlockID = BlockDefinition::GetBlockIDByName("gold");
	static BlockDefinitionID diamondBlockID = BlockDefinition::GetBlockIDByName("diamond");
	static BlockDefinitionID sandBlockID = BlockDefinition::GetBlockIDByName("sand");
	static BlockDefinitionID iceBlockID = BlockDefinition::GetBlockIDByName("ice");

	constexpr int MIN_TREE_SEPARATION = 2;

	constexpr int CHUNK_COLUMN_GRIDSIZEX = CHUNK_SIZE_X + CHUNK_GENERATION_NEIGHBORHOOD_OFFSET * 2;
	int neighborhoodColumnIndex = (CHUNK_COLUMN_GRIDSIZEX * neighborCoords.y) + neighborCoords.x;

	//std::string blockType = "air";
	BlockDefinitionID blockType = airBlockID;

	IntVec3 localCoords = neighborCoords - IntVec3(CHUNK_GENERATION_NEIGHBORHOOD_OFFSET, CHUNK_GENERATION_NEIGHBORHOOD_OFFSET, 0);
	int terrainHeight = terrainHeights[neighborhoodColumnIndex];
	IntVec3 globalCoords = localCoords + IntVec3(int(m_worldPosition.x), int(m_worldPosition.y), int(m_worldPosition.z));

	if (globalCoords.z == terrainHeight)
	{
		blockType = grassBlockID;
	}
	else if (globalCoords.z < terrainHeight && globalCoords.z >= terrainHeight - 3)
	{
		blockType = dirtBlockID;
	}
	else if (globalCoords.z == terrainHeight - 4)
	{
		if (g_RNG->RollRandomChance(0.5f))
		{
			blockType = dirtBlockID;
		}
		else
		{
			blockType = stoneBlockID;
		}
	}
	else if (globalCoords.z < terrainHeight - 4)
	{
		blockType = stoneBlockID;
	}

	// turn air blocks below surface level to water
	if (blockType == airBlockID && globalCoords.z <= CHUNK_SIZE_Z / 2)
	{
		blockType = waterBlockID;
	}

	// randomly convert stone blocks to ores
	if (blockType == stoneBlockID)
	{
		if (g_RNG->RollRandomChance(0.05f))
		{
			blockType = coalBlockID;
		}
		else if (g_RNG->RollRandomChance(0.02f))
		{
			blockType = ironBlockID;
		}
		else if (g_RNG->RollRandomChance(0.005f))
		{
			blockType = goldBlockID;
		}
		else if (g_RNG->RollRandomChance(0.001f))
		{
			blockType = diamondBlockID;
		}
	}

	// Humidity check
	float humidity = humidityArr[neighborhoodColumnIndex];
	if (humidity < 0.7f && globalCoords.z == SEA_LEVEL && blockType == grassBlockID)
	{
		blockType = sandBlockID;
	}
	if (humidity < 0.4f)
	{
		if (blockType == grassBlockID || blockType == dirtBlockID)
		{
			blockType = sandBlockID;
		}
	}

	// Temperature check
	float temperature = temperatureArr[neighborhoodColumnIndex];
	if (temperature < 0.4f)
	{
		if (blockType == waterBlockID)
		{
			blockType = iceBlockID;
		}
	}

	// Tree placement
	if (globalCoords.z == (terrainHeight + 1) && terrainHeight > SEA_LEVEL)
	{
		//IntVec2 columnCoords = IntVec2(localCoords.x + CHUNK_GENERATION_PERIPHERAL_OFFSET, localCoords.y + CHUNK_GENERATION_PERIPHERAL_OFFSET);
		//int columnIndex = columnCoords.y * CHUNK_COLUMN_GRIDSIZEX + columnCoords.x;

		if (IsLocalMaximum(IntVec2(neighborCoords.x, neighborCoords.y), treeRawNoise, MIN_TREE_SEPARATION) && treeRawNoise[neighborhoodColumnIndex] > forestness[neighborhoodColumnIndex])
		{
			if (humidity < 0.3f)
			{
				auto cactusTemplateIter = BlockTemplate::s_blockTemplates.find("cactusTemplate");
				if (cactusTemplateIter != BlockTemplate::s_blockTemplates.end())
				{
					BlockTemplateToDo cactusTree(localCoords, &cactusTemplateIter->second);
					m_blockTemplateSpawnToDo.push_back(cactusTree);
				}
			}
			else if (temperature < 0.5f)
			{
				auto spruceTemplateIter = BlockTemplate::s_blockTemplates.find("spruceTemplate");
				if (spruceTemplateIter != BlockTemplate::s_blockTemplates.end())
				{
					BlockTemplateToDo spruceTree(localCoords, &spruceTemplateIter->second);
					m_blockTemplateSpawnToDo.push_back(spruceTree);
				}
			}
			else
			{
				auto oakTemplateIter = BlockTemplate::s_blockTemplates.find("oakTemplate");
				if (oakTemplateIter != BlockTemplate::s_blockTemplates.end())
				{
					BlockTemplateToDo oakTree(localCoords, &oakTemplateIter->second);
					m_blockTemplateSpawnToDo.push_back(oakTree);
				}
			}
		}
	}

	return blockType;
}

bool Chunk::IsLocalMaximum(IntVec2 const& neighborhoodColCoords, float rawNoise[], int range) const
{
	constexpr int CHUNK_COLUMN_GRIDSIZEX = CHUNK_SIZE_X + (CHUNK_GENERATION_NEIGHBORHOOD_OFFSET * 2);
	int neighborhoodColIndex = (neighborhoodColCoords.y * CHUNK_COLUMN_GRIDSIZEX) + neighborhoodColCoords.x;

	for (int yDist = neighborhoodColCoords.y - range; yDist <= neighborhoodColCoords.y + range; yDist++)
	{
		for (int xDist = neighborhoodColCoords.x - range; xDist <= neighborhoodColCoords.x + range; xDist++)
		{
			if (xDist == neighborhoodColCoords.x && yDist == neighborhoodColCoords.y)
			{
				continue;
			}

			IntVec2 nearbyColCoords(xDist, yDist);
			int nearbyColIndex = nearbyColCoords.y * CHUNK_COLUMN_GRIDSIZEX + nearbyColCoords.x;
			if (rawNoise[nearbyColIndex] >= rawNoise[neighborhoodColIndex])
			{
				return false;
			}
		}
	}

	return true;
}

void Chunk::AddVertsForBlock(int blockIndex)
{
	IntVec3 blockCoords = GetBlockCoordsFromIndex(blockIndex);
	BlockIter blockIter = BlockIter(this, blockIndex);

	Block* southBlock = blockIter.GetSouthBlock().GetBlock();
	Block* northBlock = blockIter.GetNorthBlock().GetBlock();
	Block* eastBlock = blockIter.GetEastBlock().GetBlock();
	Block* westBlock = blockIter.GetWestBlock().GetBlock();
	Block* skywardBlock = blockIter.GetSkywardBlock().GetBlock();
	Block* groundwardBlock = blockIter.GetGroundwardBlock().GetBlock();

	bool addSouthFace = southBlock && !southBlock->IsVisible();
	bool addNorthFace = northBlock && !northBlock->IsVisible();
	bool addEastFace = eastBlock && !eastBlock->IsVisible();
	bool addWestFace = westBlock && !westBlock->IsVisible();
	bool addSkywardFace = skywardBlock && !skywardBlock->IsVisible();
	bool addGroundwardFace = groundwardBlock && !groundwardBlock->IsVisible();

	if (blockIter.GetBlock()->IsWater())
	{
		addSouthFace = true;
		addNorthFace = true;
		addEastFace = true;
		addWestFace = true;
		addSkywardFace = true;
		addGroundwardFace = true;
	}

	blockCoords += IntVec3((int)m_worldPosition.x, (int)m_worldPosition.y, 0);
	AABB2 const& topSpriteUVs = m_blocks[blockIndex].GetDefinition().m_topTextureUVs;
	AABB2 const& sideSpriteUVs = m_blocks[blockIndex].GetDefinition().m_sideTextureUVs;
	AABB2 const& bottomSpriteUVs = m_blocks[blockIndex].GetDefinition().m_bottomTextureUVs;

	AABB3 blockBounds(blockCoords.GetAsVec3(), blockCoords.GetAsVec3() + Vec3::EAST + Vec3::NORTH + Vec3::SKYWARD);
	Vec3 const& mins = blockBounds.m_mins;
	Vec3 const& maxs = blockBounds.m_maxs;

	Vec3 BLF(mins.x, maxs.y, mins.z);
	Vec3 BRF(mins.x, mins.y, mins.z);
	Vec3 TRF(mins.x, mins.y, maxs.z);
	Vec3 TLF(mins.x, maxs.y, maxs.z);
	Vec3 BLB(maxs.x, maxs.y, mins.z);
	Vec3 BRB(maxs.x, mins.y, mins.z);
	Vec3 TRB(maxs.x, mins.y, maxs.z);
	Vec3 TLB(maxs.x, maxs.y, maxs.z);

	if (addEastFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::EAST);
		AddVertsForQuad3D(m_vertexes, BRB, BLB, TLB, TRB, tint, sideSpriteUVs); // +X
		m_chunkRenderedVerts += 6;
	}

	if (addWestFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::WEST);
		AddVertsForQuad3D(m_vertexes, BLF, BRF, TRF, TLF, tint, sideSpriteUVs); // -X
		m_chunkRenderedVerts += 6;
	}

	if (addNorthFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::NORTH);
		AddVertsForQuad3D(m_vertexes, BLB, BLF, TLF, TLB, tint, sideSpriteUVs); // +Y
		m_chunkRenderedVerts += 6;
	}

	if (addSouthFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::SOUTH);
		AddVertsForQuad3D(m_vertexes, BRF, BRB, TRB, TRF, tint, sideSpriteUVs); // -Y
		m_chunkRenderedVerts += 6;
	}

	if (addSkywardFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::SKYWARD);
		AddVertsForQuad3D(m_vertexes, TLF, TRF, TRB, TLB, tint, topSpriteUVs); // +Z
		m_chunkRenderedVerts += 6;
	}

	if (addGroundwardFace)
	{
		Rgba8 tint = blockIter.GetFaceTintForLightInfluenceValues(Direction::GROUNDWARD);
		AddVertsForQuad3D(m_vertexes, BLB, BRB, BRF, BLF, tint, bottomSpriteUVs); // -Z
		m_chunkRenderedVerts += 6;
	}
}

IntVec3 Chunk::GetBlockCoordsFromWorldPosition(Vec3 const& worldPosition) const
{
	Vec3 chunkLocalSpaceCoords = worldPosition - m_worldPosition;
	IntVec3 blockCoords = IntVec3(RoundDownToInt(chunkLocalSpaceCoords.x), RoundDownToInt(chunkLocalSpaceCoords.y), RoundDownToInt(chunkLocalSpaceCoords.z));
	return blockCoords;
}

bool Chunk::AreBlockCoordsInChunk(IntVec3 const& blockCoords) const
{
	return (blockCoords.x >= 0 && blockCoords.x < CHUNK_SIZE_X && blockCoords.y >= 0 && blockCoords.y < CHUNK_SIZE_Y && blockCoords.z >= 0 && blockCoords.z < CHUNK_SIZE_Z);
}

bool Chunk::AddBlockAtWorldPosition(Vec3 const& worldPosition, BlockDefinitionID blockType)
{
	IntVec3 blockCoords = GetBlockCoordsFromWorldPosition(worldPosition);
	int blockIndex = GetBlockIndexFromCoords(blockCoords);
	m_blocks[blockIndex].SetTypeID(blockType);
	BlockIter blockIter = BlockIter(this, blockIndex);
	m_world->MarkBlockLightingDirty(blockIter);

	if (m_blocks[blockIndex].IsSky() && m_blocks[blockIndex].IsOpaque())
	{
		m_blocks[blockIndex].SetSky(false);
		blockIter = blockIter.GetGroundwardBlock();
		Block* currentBlock = blockIter.GetBlock();
		while (currentBlock && !currentBlock->IsOpaque())
		{
			currentBlock->SetSky(false);
			m_world->MarkBlockLightingDirty(blockIter);

			blockIter = blockIter.GetGroundwardBlock();
			currentBlock = blockIter.GetBlock();
		}
	}

	m_isCpuMeshDirty = true;
	m_needsSaving = true;
	return true;
}

bool Chunk::DigBlockAtWorldPosition(Vec3 const& worldPosition)
{
	BlockDefinitionID airBlockID = BlockDefinition::GetBlockIDByName("air");

	IntVec3 blockCoords = GetBlockCoordsFromWorldPosition(worldPosition);
	int blockIndex = GetBlockIndexFromCoords(blockCoords);
	m_blocks[blockIndex].SetTypeID(airBlockID);
	BlockIter blockIter = BlockIter(this, blockIndex);
	m_world->MarkBlockLightingDirty(blockIter);

	if (blockIter.GetSkywardBlock().GetBlock() && blockIter.GetSkywardBlock().GetBlock()->IsSky())
	{
		m_blocks[blockIndex].SetSky(true);
		blockIter = blockIter.GetGroundwardBlock();
		Block* currentBlock = blockIter.GetBlock();
		while (currentBlock && !currentBlock->IsOpaque())
		{
			currentBlock->SetSky(true);
			m_world->MarkBlockLightingDirty(blockIter);

			blockIter = blockIter.GetGroundwardBlock();
			currentBlock = blockIter.GetBlock();
		}
	}

	m_isCpuMeshDirty = true;
	m_needsSaving = true;
	return true;
}

bool Chunk::IsBlockOpaque(Block const* block) const
{
	if (!block)
	{
		return false;
	}

	return block->IsOpaque();
}

void ChunkGenerateJob::Execute()
{
	m_chunk->m_state = ChunkState::ACTIVATING_GENERATING;
	m_chunk->GenerateChunkBlocks();
	m_chunk->PlaceBlockTemplates();
	m_chunk->m_state = ChunkState::ACTIVATING_GENERATE_COMPLETE;
}
