#include "Game/World.hpp"

#include "Game/Chunk.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Block.hpp"
#include "Game/BlockIter.hpp"

#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Time.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"


World::~World()
{
	for (auto chunkMapIter = m_activeChunks.begin(); chunkMapIter != m_activeChunks.end(); ++chunkMapIter)
	{
		Chunk*& chunk = chunkMapIter->second;
		delete chunk;
	}
	m_activeChunks.clear();

	m_chunkCoordsQueuedForActivation.clear();

	g_jobSystem->Shutdown();
	g_jobSystem->Startup();
}

World::World(Game* game)
	: m_game(game)
{
	m_worldSeed = g_gameConfigBlackboard.GetValue("worldSeed", m_worldSeed);
	if (m_worldSeed == 0)
	{
		m_worldSeed = g_RNG->RollRandomIntLessThan(INT_MAX);
	}

	m_shader = g_renderer->CreateOrGetShader("Data/Shaders/World");
	m_shaderConstants = g_renderer->CreateConstantBuffer(sizeof(SimpleMinerConstants));
}

extern double g_worldUpdateTime;
extern double g_chunkRebuildDecisionTime;
void World::Update()
{
	double worldUpdateStartTime = GetCurrentTimeSeconds();

	float deltaSeconds = m_game->m_gameClock.GetDeltaSeconds();

	m_worldTime += (deltaSeconds * m_worldTimeScale) / (60.f * 60.f * 24.f);

	HandleChunkActivationDeactivation();
	m_totalRenderedVerts = 0;

	Chunk* nearestChunk = nullptr;
	float nearestChunkDistance = FLT_MAX;
	Chunk* secondNearestChunk = nullptr;
	float secondNearestChunkDistance = FLT_MAX;

	double chunkRebuildDecisionStartTime = GetCurrentTimeSeconds();
	for (auto chunkMapIter = m_activeChunks.begin(); chunkMapIter != m_activeChunks.end(); ++chunkMapIter)
	{
		Chunk* chunk = chunkMapIter->second;
		if (!chunk->m_isCpuMeshDirty)
		{
			continue;
		}
		if (!chunk->m_eastNeighbor || !chunk->m_westNeighbor || !chunk->m_northNeighbor || !chunk->m_southNeighbor)
		{
			continue;
		}

		Vec2 chunkCenterXY = chunk->m_worldPosition.GetXY();
		float chunkDistance = GetDistance2D(m_game->m_cameraPosition.GetXY(), chunkCenterXY);
		if (chunkDistance < nearestChunkDistance)
		{
			secondNearestChunk = nearestChunk;
			secondNearestChunkDistance = nearestChunkDistance;
			nearestChunk = chunk;
			nearestChunkDistance = chunkDistance;
		}
		else if (chunkDistance < secondNearestChunkDistance)
		{
			secondNearestChunk = chunk;
			secondNearestChunkDistance = chunkDistance;
		}
	}
	double chunkRebuildDecisionEndTime = GetCurrentTimeSeconds();
	g_chunkRebuildDecisionTime = (chunkRebuildDecisionEndTime - chunkRebuildDecisionStartTime) * 1000.f;

	if (nearestChunk)
	{
		nearestChunk->RebuildMesh();
	}
	if (secondNearestChunk)
	{
		secondNearestChunk->RebuildMesh();
	}

	for (auto chunkMapIter = m_activeChunks.begin(); chunkMapIter != m_activeChunks.end(); ++chunkMapIter)
	{
		chunkMapIter->second->Update();
	}

	UpdateSimpleMinerShaderConstants();
	if (!m_disableLighting)
	{
		ProcessDirtyLighting();
	}

	double worldUpdateEndTime = GetCurrentTimeSeconds();
	g_worldUpdateTime = (worldUpdateEndTime - worldUpdateStartTime) * 1000.f;
}

extern double g_worldRenderTime;
void World::Render() const
{
	double worldRenderStartTime = GetCurrentTimeSeconds();

	if (m_disableWorldShader)
	{
		g_renderer->BindShader(nullptr);
	}
	else
	{
		g_renderer->BindShader(m_shader);
	}

	for (auto chunkMapIter = m_activeChunks.begin(); chunkMapIter != m_activeChunks.end(); ++chunkMapIter)
	{
		chunkMapIter->second->Render();
	}

	double worldRenderEndTime = GetCurrentTimeSeconds();
	g_worldRenderTime = (worldRenderEndTime - worldRenderStartTime) * 1000.f;
}

void World::UpdateSimpleMinerShaderConstants()
{
	SimpleMinerConstants simpleMinerConstants;

	float timeOfDay = m_worldTime;
	if (m_isWorldTimeFixedToDay)
	{
		timeOfDay = 0.5f;
	}

	while (timeOfDay > 1.f)
	{
		timeOfDay -= 1.f;
	}

	if (timeOfDay <= 0.25f || timeOfDay >= 0.75f)
	{
		m_skyColor = Rgba8(20, 20, 40, 255);
	}
	else if (timeOfDay <= 0.5f)
	{
		float t = RangeMap(timeOfDay, 0.25f, 0.5f, 0.f, 1.f);
		m_skyColor = Interpolate(Rgba8(20, 20, 40, 255), Rgba8(200, 230, 255, 255), t);
	}
	else
	{
		float t = RangeMap(timeOfDay, 0.5f, 0.75f, 0.f, 1.f);
		m_skyColor = Interpolate(Rgba8(200, 230, 255, 255), Rgba8(20, 20, 40, 255), t);
	}

	if (!m_disableLightning)
	{
		// Lightning
		float lightningPerlin = Compute1dPerlinNoise(m_worldTime, 0.001f, 9, 0.5f, 2.f, true, 0);
		lightningPerlin = RangeMapClamped(lightningPerlin, 0.6f, 0.9f, 0.f, 1.f);
		m_skyColor = Interpolate(m_skyColor, Rgba8::WHITE, lightningPerlin);
	}
	m_outdoorLightColor = m_skyColor;

	// Glowstone Flickering
	float glowPerlin = Compute1dPerlinNoise(m_worldTime, 0.001f, 9, 0.5f, 2.f, true, 0);
	glowPerlin = RangeMapClamped(glowPerlin, -1.f, 1.f, 0.8f, 1.f);
	Rgba8 indoorLightColor((unsigned char)RoundDownToInt(m_indoorLightColor.r * glowPerlin), (unsigned char)RoundDownToInt(m_indoorLightColor.g * glowPerlin), (unsigned char)RoundDownToInt(m_indoorLightColor.b * glowPerlin), 255);

	Vec3 cameraPosition = m_game->m_cameraPosition;
	float indoorLightColorFloats[4];
	float outdoorLightColorFloats[4];
	float skyColorFloats[4];

	indoorLightColor.GetAsFloats(indoorLightColorFloats);
	m_outdoorLightColor.GetAsFloats(outdoorLightColorFloats);
	m_skyColor.GetAsFloats(skyColorFloats);

	simpleMinerConstants.b_camWorldPos = Vec4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.f);
	simpleMinerConstants.b_fogEndDist = g_activationRadius - 16.f;
	simpleMinerConstants.b_fogMaxAlpha = 0.f;
	simpleMinerConstants.b_fogStartDist = (g_activationRadius - 16.f) * 0.5f;
	simpleMinerConstants.b_indoorLightColor = Vec4(indoorLightColorFloats[0], indoorLightColorFloats[1], indoorLightColorFloats[2], indoorLightColorFloats[3]);
	simpleMinerConstants.b_outdoorLightColor = Vec4(outdoorLightColorFloats[0], outdoorLightColorFloats[1], outdoorLightColorFloats[2], outdoorLightColorFloats[3]);
	simpleMinerConstants.b_skyColor = Vec4(skyColorFloats[0], skyColorFloats[1], skyColorFloats[2], skyColorFloats[3]);
	simpleMinerConstants.b_time = timeOfDay;

	g_renderer->CopyCPUToGPU(&simpleMinerConstants, sizeof(simpleMinerConstants), m_shaderConstants);
	g_renderer->BindConstantBuffer(8, m_shaderConstants);
}

void World::RequestChunkActivation(IntVec2 const& chunkCoords)
{
	Chunk* chunk = new Chunk(this, chunkCoords);

	bool loaded = chunk->LoadFromFile();
	if (loaded)
	{
		ActivateChunk(chunk);
	}
	if (!loaded)
	{
		chunk->m_state = ChunkState::ACTIVATING_QUEUED_GENERATE;
		ChunkGenerateJob* generateJob = new ChunkGenerateJob(chunk);
		g_jobSystem->QueueJob(generateJob);
		m_chunkCoordsQueuedForActivation.insert(chunkCoords);
	}

	return;
}

void World::DeactivateChunk(IntVec2 const& chunkCoords)
{
	m_activeChunks[chunkCoords]->m_state = ChunkState::DEACTIVATING_QUEUED_SAVE;
	IntVec2 chunkCoordinates(chunkCoords);
	delete m_activeChunks[chunkCoordinates];
	m_activeChunks[chunkCoordinates] = nullptr;
	m_activeChunks.erase(chunkCoordinates);
}

extern double g_chunkActivationDeactivationDecisionTime;
void World::HandleChunkActivationDeactivation()
{
	double chunkActivationDeactivationDecisionStartTime = GetCurrentTimeSeconds();

	int numChunks = (int)m_activeChunks.size() + (int)m_chunkCoordsQueuedForActivation.size();

	if (numChunks < MAX_CHUNKS)
	{
		bool didActivate = RequestActivationOfNearestChunkInRange(g_activationRadius);
		if (!didActivate)
		{
			DeactivateFarthestChunkOutOfRange(g_deactivationRadius);
		}
	}
	else
	{
		DeactivateFarthestChunkOutOfRange(0.f);
	}

	Job* completedJob = g_jobSystem->GetCompletedJob();
	ChunkGenerateJob* generateJob = dynamic_cast<ChunkGenerateJob*>(completedJob);
	if (generateJob)
	{
		m_chunkCoordsQueuedForActivation.erase(generateJob->m_chunk->m_coords);
		ActivateChunk(generateJob->m_chunk);
	}

	double chunkActivationDeactivationDecisionEndTime = GetCurrentTimeSeconds();
	g_chunkActivationDeactivationDecisionTime = (chunkActivationDeactivationDecisionEndTime - chunkActivationDeactivationDecisionStartTime) * 1000.f;
}

bool World::RequestActivationOfNearestChunkInRange(float range)
{
	Vec2 playerPosition2D = m_game->m_cameraPosition.GetXY();
	IntVec2 playerChunkCoords = IntVec2(RoundDownToInt(playerPosition2D.x / (float)CHUNK_SIZE_X), RoundDownToInt(playerPosition2D.y / (float)CHUNK_SIZE_Y));

	IntVec2 startCoords = playerChunkCoords - IntVec2((int)ceilf(range / (float)CHUNK_SIZE_X), (int)ceilf(range / (float)CHUNK_SIZE_Y));
	IntVec2 endCoords = playerChunkCoords + IntVec2((int)ceilf(range / (float)CHUNK_SIZE_X), (int)ceilf(range / (float)CHUNK_SIZE_Y));

	IntVec2 nearestInactiveChunkCoords = IntVec2(INT_MAX, INT_MAX);
	float nearestInactiveChunkDistanceSq = FLT_MAX;

	for (int y = startCoords.y; y < endCoords.y; y++)
	{
		for (int x = startCoords.x; x < endCoords.x; x++)
		{
			IntVec2 chunkCoords = IntVec2(x, y);
			Vec2 chunkCenterXY = chunkCoords.GetAsVec2() * Vec2(CHUNK_SIZE_X, CHUNK_SIZE_Y) + Vec2(CHUNK_SIZE_X * 0.5f, CHUNK_SIZE_Y * 0.5f);
			if (IsPointInsideDisc2D(chunkCenterXY, playerPosition2D, range))
			{
				float chunkDistanceSq = GetDistanceSquared2D(chunkCenterXY, playerPosition2D);
				if (chunkDistanceSq >= nearestInactiveChunkDistanceSq)
				{
					continue;
				}

				if (GetChunkAtCoords(chunkCoords))
				{
					// Chunk already active, don't request activation
					continue;
				}

				if (m_chunkCoordsQueuedForActivation.find(chunkCoords) != m_chunkCoordsQueuedForActivation.end())
				{
					// Chunk already requested, don't request activation again
					continue;
				}

				nearestInactiveChunkCoords = chunkCoords;
				nearestInactiveChunkDistanceSq = chunkDistanceSq;
			}


			//auto chunkMapIter = m_activeChunks.find(chunkCoords);
			//if (chunkMapIter == m_activeChunks.end())
			//{
			//	// Chunk is inactive
			//	// Check within range
			//	Vec2 chunkCenter = chunkCoords.GetAsVec2() * Vec2(CHUNK_SIZE_X, CHUNK_SIZE_Y) + Vec2(CHUNK_SIZE_X * 0.5f, CHUNK_SIZE_Y * 0.5f);
			//	if (IsPointInsideDisc2D(chunkCenter, playerPosition2D, range))
			//	{
			//		// Chunk is within range
			//		// Check if nearest
			//		float chunkDistanceSq = GetDistanceSquared2D(chunkCenter, playerPosition2D);
			//		if (chunkDistanceSq < nearestInactiveChunkDistanceSq)
			//		{
			//			nearestInactiveChunkCoords = chunkCoords;
			//			nearestInactiveChunkDistanceSq = chunkDistanceSq;
			//		}
			//	}
			//}
		}
	}

	if (nearestInactiveChunkDistanceSq < range * range)
	{
		RequestChunkActivation(nearestInactiveChunkCoords);
		return true;
	}

	return false;
}

bool World::DeactivateFarthestChunkOutOfRange(float range)
{
	Vec2 playerPosition2D = m_game->m_cameraPosition.GetXY();
	IntVec2 playerChunkCoords = IntVec2(RoundDownToInt(playerPosition2D.x / (float)CHUNK_SIZE_X), RoundDownToInt(playerPosition2D.y / (float)CHUNK_SIZE_Y));

	IntVec2 startCoords = playerChunkCoords - IntVec2((int)ceilf(range / (float)CHUNK_SIZE_X), (int)ceilf(range / (float)CHUNK_SIZE_Y));
	IntVec2 endCoords = playerChunkCoords + IntVec2((int)ceilf(range / (float)CHUNK_SIZE_X), (int)ceilf(range / (float)CHUNK_SIZE_Y));

	Chunk* farthestActiveChunk = nullptr;
	float farthestActiveChunkDistanceSq = FLT_MIN;

	for (auto activeChunksMapIter = m_activeChunks.begin(); activeChunksMapIter != m_activeChunks.end(); ++activeChunksMapIter)
	{
		Chunk* chunk = activeChunksMapIter->second;
		Vec2 chunkCenter = chunk->m_worldPosition.GetXY() + Vec2(CHUNK_SIZE_X * 0.5f, CHUNK_SIZE_Y * 0.5f);
		if (!IsPointInsideDisc2D(chunkCenter, playerPosition2D, range))
		{
			float chunkDistanceSq = GetDistanceSquared2D(chunkCenter, playerPosition2D);
			if (chunkDistanceSq > farthestActiveChunkDistanceSq)
			{
				farthestActiveChunk = chunk;
				farthestActiveChunkDistanceSq = chunkDistanceSq;
			}
		}
	}

	if (farthestActiveChunk)
	{
		DeactivateChunk(farthestActiveChunk->m_coords);
		return true;
	}

	return false;
}

extern double g_chunkActivationTime;
void World::ActivateChunk(Chunk* chunk)
{
	double activateChunkStartTime = GetCurrentTimeSeconds();

	IntVec2 chunkCoords = chunk->m_coords;

	// Hook up neighbors
	chunk->m_eastNeighbor = GetChunkAtCoords(chunkCoords + IntVec2::EAST);
	chunk->m_westNeighbor = GetChunkAtCoords(chunkCoords + IntVec2::WEST);
	chunk->m_northNeighbor = GetChunkAtCoords(chunkCoords + IntVec2::NORTH);
	chunk->m_southNeighbor = GetChunkAtCoords(chunkCoords + IntVec2::SOUTH);

	if (chunk->m_eastNeighbor)
	{
		chunk->m_eastNeighbor->m_westNeighbor = chunk;
	}
	if (chunk->m_westNeighbor)
	{
		chunk->m_westNeighbor->m_eastNeighbor = chunk;
	}
	if (chunk->m_northNeighbor)
	{
		chunk->m_northNeighbor->m_southNeighbor = chunk;
	}
	if (chunk->m_southNeighbor)
	{
		chunk->m_southNeighbor->m_northNeighbor = chunk;
	}

	DirtyChunkLighting(chunk);
	m_activeChunks[chunkCoords] = chunk;
	chunk->m_state = ChunkState::ACTIVE;

	double activateChunkEndTime = GetCurrentTimeSeconds();
	g_chunkActivationTime = (activateChunkEndTime - activateChunkStartTime) * 1000.f;
}

void World::DirtyChunkLighting(Chunk* chunk)
{
	//--------------------------------------------------------------------------------
	// Lighting for chunk

	// Mark all WEST face blocks as having dirty lighting
	// Allows propagation of light from other chunks into this chunk and vice-versa
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int z = 0; z < CHUNK_SIZE_Z; z++)
		{
			IntVec3 blockCoords = IntVec3(0, y, z);
			int blockIndex = chunk->GetBlockIndexFromCoords(blockCoords);

			if (chunk->m_blocks[blockIndex].IsOpaque())
			{
				continue;
			}

			BlockIter blockIter = BlockIter(chunk, blockIndex);;
			MarkBlockLightingDirty(blockIter);
		}
	}
	// Mark all EAST face blocks as having dirty lighting
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int z = 0; z < CHUNK_SIZE_Z; z++)
		{
			IntVec3 blockCoords = IntVec3(CHUNK_SIZE_X - 1, y, z);
			int blockIndex = chunk->GetBlockIndexFromCoords(blockCoords);

			if (chunk->m_blocks[blockIndex].IsOpaque())
			{
				continue;
			}

			BlockIter blockIter = BlockIter(chunk, blockIndex);
			MarkBlockLightingDirty(blockIter);
		}
	}
	// Mark all SOUTH face blocks as having dirty lighting
	for (int x = 0; x < CHUNK_SIZE_X; x++)
	{
		for (int z = 0; z < CHUNK_SIZE_Z; z++)
		{
			IntVec3 blockCoords = IntVec3(x, 0, z);
			int blockIndex = chunk->GetBlockIndexFromCoords(blockCoords);

			if (chunk->m_blocks[blockIndex].IsOpaque())
			{
				continue;
			}

			BlockIter blockIter = BlockIter(chunk, blockIndex);
			MarkBlockLightingDirty(blockIter);
		}
	}
	// Mark all NORTH face blocks as having dirty lighting
	for (int x = 0; x < CHUNK_SIZE_X; x++)
	{
		for (int z = 0; z < CHUNK_SIZE_Z; z++)
		{
			IntVec3 blockCoords = IntVec3(x, CHUNK_SIZE_Y - 1, z);
			int blockIndex = chunk->GetBlockIndexFromCoords(blockCoords);

			if (chunk->m_blocks[blockIndex].IsOpaque())
			{
				continue;
			}

			BlockIter blockIter = BlockIter(chunk, blockIndex);
			MarkBlockLightingDirty(blockIter);
		}
	}

	for (int blockIndex = 0; blockIndex < CHUNK_BLOCKS_TOTAL; blockIndex++)
	{
		if (chunk->m_blocks[blockIndex].GetDefinition().m_lightInfluence != 0)
		{
			BlockIter blockIter = BlockIter(chunk, blockIndex);
			MarkBlockLightingDirty(blockIter);
		}
	}

	for (int x = 0; x < CHUNK_SIZE_X; x++)
	{
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			IntVec3 skywardBlockCoords = IntVec3(x, y, CHUNK_SIZE_Z - 1);
			int blockIndex = chunk->GetBlockIndexFromCoords(skywardBlockCoords);
			BlockIter currentBlockIter = BlockIter(chunk, blockIndex);
			Block* block = currentBlockIter.GetBlock();
			while (block && !block->IsOpaque())
			{
				block->SetSky(true);

				currentBlockIter = currentBlockIter.GetGroundwardBlock();
				block = currentBlockIter.GetBlock();
			}
		}
	}

	for (int x = 0; x < CHUNK_SIZE_X; x++)
	{
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			IntVec3 skywardBlockCoords = IntVec3(x, y, CHUNK_SIZE_Z - 1);
			int blockIndex = chunk->GetBlockIndexFromCoords(skywardBlockCoords);
			BlockIter currentBlockIter = BlockIter(chunk, blockIndex);
			Block* block = currentBlockIter.GetBlock();
			while (block && !block->IsOpaque())
			{
				BlockIter eastBlockIter = currentBlockIter.GetEastBlock();
				BlockIter westBlockIter = currentBlockIter.GetWestBlock();
				BlockIter northBlockIter = currentBlockIter.GetNorthBlock();
				BlockIter southBlockIter = currentBlockIter.GetSouthBlock();

				Block* eastBlock = eastBlockIter.GetBlock();
				Block* westBlock = westBlockIter.GetBlock();
				Block* northBlock = northBlockIter.GetBlock();
				Block* southBlock = southBlockIter.GetBlock();

				if (eastBlock && !eastBlock->IsOpaque() && !eastBlock->IsSky() && !eastBlock->IsLightDirty())
				{
					MarkBlockLightingDirty(eastBlockIter);
				}
				if (westBlock && !westBlock->IsOpaque() && !westBlock->IsSky() && !westBlock->IsLightDirty())
				{
					MarkBlockLightingDirty(westBlockIter);
				}
				if (northBlock && !northBlock->IsOpaque() && !northBlock->IsSky() && !northBlock->IsLightDirty())
				{
					MarkBlockLightingDirty(northBlockIter);
				}
				if (southBlock && !southBlock->IsOpaque() && !southBlock->IsSky() && !southBlock->IsLightDirty())
				{
					MarkBlockLightingDirty(southBlockIter);
				}

				currentBlockIter = currentBlockIter.GetGroundwardBlock();
				block = currentBlockIter.GetBlock();
			}
		}
	}
}

Chunk* World::GetChunkAtCoords(IntVec2 const& chunkCoords) const
{
	auto chunkMapIter = m_activeChunks.find(chunkCoords);
	if (chunkMapIter != m_activeChunks.end())
	{
		return chunkMapIter->second;
	}

	return nullptr;
}

Chunk* World::GetChunkForWorldPosition(Vec3 const& worldPosition) const
{
	IntVec2 chunkCoords = IntVec2(RoundDownToInt(worldPosition.x / CHUNK_SIZE_X), RoundDownToInt(worldPosition.y / CHUNK_SIZE_Y));
	auto chunkMapIter = m_activeChunks.find(chunkCoords);
	if (chunkMapIter != m_activeChunks.end())
	{
		return chunkMapIter->second;
	}

	return nullptr;
}

SimpleMinerRaycastResult World::RaycastVsBlocks(Vec3 const& startPosition, Vec3 const& direction, float maxDistance) const
{
	SimpleMinerRaycastResult raycastResult;

	if (startPosition.z < 0.f || startPosition.z > (float)CHUNK_SIZE_Z)
	{
		return raycastResult;
	}

	if (maxDistance == 0)
	{
		return raycastResult;
	}

	Chunk* startChunk = GetChunkForWorldPosition(startPosition);
	if (!startChunk)
	{
		return raycastResult;
	}

	int startBlockIndex = startChunk->GetBlockIndexFromCoords(startChunk->GetBlockCoordsFromWorldPosition(startPosition));
	BlockIter currentBlockIter = BlockIter(startChunk, startBlockIndex);

	IntVec3 currentBlockCoords = IntVec3(RoundDownToInt(startPosition.x), RoundDownToInt(startPosition.y), RoundDownToInt(startPosition.z));
	Vec3 rayStepSize = Vec3(direction.x != 0 ? 1.f / fabsf(direction.x) : 99999.f, direction.y != 0.f ? 1.f / fabsf(direction.y) : 99999.f, direction.z != 0.f ? 1.f / fabsf(direction.z) : 99999.f);
	Vec3 cumulativeRayLengthIn1D;
	IntVec3 directionXYZ;
	float totalRayLength = 0.f;

	float deltaRayLength = 0.f;

	if (direction.x < 0.f)
	{
		directionXYZ.x = -1;
		cumulativeRayLengthIn1D.x = (startPosition.x - static_cast<float>(currentBlockCoords.x)) * rayStepSize.x;
	}
	else
	{
		directionXYZ.x = 1;
		cumulativeRayLengthIn1D.x = (static_cast<float>(currentBlockCoords.x) + 1.f - startPosition.x) * rayStepSize.x;
	}

	if (direction.y < 0.f)
	{
		directionXYZ.y = -1;
		cumulativeRayLengthIn1D.y = (startPosition.y - static_cast<float>(currentBlockCoords.y)) * rayStepSize.y;
	}
	else
	{
		directionXYZ.y = 1;
		cumulativeRayLengthIn1D.y = (static_cast<float>(currentBlockCoords.y) + 1.f - startPosition.y) * rayStepSize.y;
	}

	if (direction.z < 0.f)
	{
		directionXYZ.z = -1;
		cumulativeRayLengthIn1D.z = (startPosition.z - (float)(currentBlockCoords.z)) * rayStepSize.z;
	}
	else
	{
		directionXYZ.z = 1;
		cumulativeRayLengthIn1D.z = ((float)(currentBlockCoords.z) + 1.f - startPosition.z) * rayStepSize.z;
	}

	while (totalRayLength < maxDistance)
	{
		Block* currentBlock = currentBlockIter.GetBlock();
	
		if (!currentBlock)
		{
			return raycastResult;
		}

		if (currentBlock->IsSolid())
		{
			Vec3 impactPosition = startPosition + direction * (totalRayLength - deltaRayLength);
			raycastResult.m_didImpact = true;
			raycastResult.m_impactDistance = totalRayLength - deltaRayLength;
			raycastResult.m_impactPosition = impactPosition;
			raycastResult.m_impactBlockIter = currentBlockIter;
			return raycastResult;
		}

		if (cumulativeRayLengthIn1D.x < cumulativeRayLengthIn1D.y && cumulativeRayLengthIn1D.x < cumulativeRayLengthIn1D.z)
		{
			currentBlockIter = directionXYZ.x == 1 ? currentBlockIter.GetEastBlock() : currentBlockIter.GetWestBlock();
			totalRayLength = cumulativeRayLengthIn1D.x;
			cumulativeRayLengthIn1D.x += rayStepSize.x;
			raycastResult.m_impactNormal = -Vec3::EAST * (float)directionXYZ.x;
		}
		else if (cumulativeRayLengthIn1D.y < cumulativeRayLengthIn1D.z)
		{
			currentBlockIter = directionXYZ.y == 1 ? currentBlockIter.GetNorthBlock() : currentBlockIter.GetSouthBlock();
			totalRayLength = cumulativeRayLengthIn1D.y;
			cumulativeRayLengthIn1D.y += rayStepSize.y;
			raycastResult.m_impactNormal = -Vec3::NORTH * (float)directionXYZ.y;
		}
		else
		{
			currentBlockIter = directionXYZ.z == 1 ? currentBlockIter.GetSkywardBlock() : currentBlockIter.GetGroundwardBlock();
			totalRayLength = cumulativeRayLengthIn1D.z;
			cumulativeRayLengthIn1D.z += rayStepSize.z;
			raycastResult.m_impactNormal = -Vec3::SKYWARD * (float)directionXYZ.z;
		}
	}

	raycastResult.m_didImpact = false;
	raycastResult.m_impactNormal = Vec3::ZERO;
	raycastResult.m_impactPosition = startPosition + direction * maxDistance;
	raycastResult.m_impactDistance = maxDistance;
	return raycastResult;
}

std::string World::GetSaveFilePath() const
{
	CreateFolder(Stringf("Saves/World_%u/", m_worldSeed).c_str());

	return Stringf("Saves/World_%u/", m_worldSeed);
}

int World::GetChunkFileVersion() const
{
	return 2;
}

void World::MarkBlockLightingDirty(BlockIter blockIter)
{
	if (m_disableLighting)
	{
		return;
	}

	blockIter.GetBlock()->SetLightDirty(true);
	m_dirtyLightingQueue.push(blockIter);
}

extern double g_lightingProcessingTime;
void World::ProcessDirtyLighting()
{
	double lightingProcessingStartTime = GetCurrentTimeSeconds();

	while (!m_dirtyLightingQueue.empty())
	{
		ProcessNextDirtyLightBlock();
	}

	double lightingProcessingEndTime = GetCurrentTimeSeconds();
	g_lightingProcessingTime = (lightingProcessingEndTime - lightingProcessingStartTime) * 1000.f;
}

void World::ProcessNextDirtyLightBlock()
{
	BlockIter blockIter = m_dirtyLightingQueue.front();
	m_dirtyLightingQueue.pop();

	Block* block = blockIter.GetBlock();
	block->SetLightDirty(false);
	int currentIndoorLightInfluence = block->GetIndoorLightInfluence();
	int currentOutdoorLightInfluence = block->GetOutdoorLightInfluence();

	int minOutdoorLightInfluence = 0;
	if (block->IsSky())
	{
		minOutdoorLightInfluence = OUTDOOR_LIGHTINFLUENCE_MAX;
	}
	int minLightInfluence = block->GetDefinition().m_lightInfluence;
	
	BlockIter eastBlockIter = blockIter.GetEastBlock();
	BlockIter westBlockIter = blockIter.GetWestBlock();
	BlockIter northBlockIter = blockIter.GetNorthBlock();
	BlockIter southBlockIter = blockIter.GetSouthBlock();
	BlockIter skywardBlockIter = blockIter.GetSkywardBlock();
	BlockIter groundwardBlockIter = blockIter.GetGroundwardBlock();

	Block* eastBlock = eastBlockIter.GetBlock();
	Block* westBlock = westBlockIter.GetBlock();
	Block* northBlock = northBlockIter.GetBlock();
	Block* southBlock = southBlockIter.GetBlock();
	Block* skywardBlock = skywardBlockIter.GetBlock();
	Block* groundwardBlock = groundwardBlockIter.GetBlock();

	// Get indoor light influences for each neighbor
	int eastILI = eastBlock ? eastBlock->GetIndoorLightInfluence() : 0;
	int westILI = westBlock ? westBlock->GetIndoorLightInfluence() : 0;
	int northILI = northBlock ? northBlock->GetIndoorLightInfluence() : 0;
	int southILI = southBlock ? southBlock->GetIndoorLightInfluence() : 0;
	int skywardILI = skywardBlock ? skywardBlock->GetIndoorLightInfluence() : 0;
	int groundwardILI = groundwardBlock ? groundwardBlock->GetIndoorLightInfluence() : 0;

	// Get outdoor light influences for each neighbor
	int eastOLI = eastBlock ? eastBlock->GetOutdoorLightInfluence() : 0;
	int westOLI = westBlock ? westBlock->GetOutdoorLightInfluence() : 0;
	int northOLI = northBlock ? northBlock->GetOutdoorLightInfluence() : 0;
	int southOLI = southBlock ? southBlock->GetOutdoorLightInfluence() : 0;
	int skywardOLI = skywardBlock ? skywardBlock->GetOutdoorLightInfluence() : 0;
	int groundwardOLI = groundwardBlock ? groundwardBlock->GetOutdoorLightInfluence() : 0;

	int indoorLightInfluences[] = {eastILI, westILI, northILI, southILI, skywardILI, groundwardILI};
	int neighborMaxIndoorLightInfluence = GetMax(6, indoorLightInfluences);
	int outdoorLightInfluences[] = {eastOLI, westOLI, northOLI, southOLI, skywardOLI, groundwardOLI};
	int neighborMaxOutdoorLightInfluence = GetMax(6, outdoorLightInfluences);
	
	int indoorLightInfluence = GetMax(minLightInfluence, neighborMaxIndoorLightInfluence - 1);
	int outdoorLightInfluence = GetMax(minOutdoorLightInfluence, neighborMaxOutdoorLightInfluence - 1);

	if (block->IsOpaque())
	{
		indoorLightInfluence = minLightInfluence;
		outdoorLightInfluence = minOutdoorLightInfluence;
	}

	if ((indoorLightInfluence != currentIndoorLightInfluence) || (outdoorLightInfluence != currentOutdoorLightInfluence))
	{
		block->SetIndoorLightInfluence(indoorLightInfluence);
		block->SetOutdoorLightInfluence(outdoorLightInfluence);

		blockIter.m_chunk->m_isCpuMeshDirty = true;
		if (blockIter.m_chunk->m_eastNeighbor)
		{
			blockIter.m_chunk->m_eastNeighbor->m_isCpuMeshDirty = true;
		}
		if (blockIter.m_chunk->m_westNeighbor)
		{
			blockIter.m_chunk->m_westNeighbor->m_isCpuMeshDirty = true;
		}
		if (blockIter.m_chunk->m_northNeighbor)
		{
			blockIter.m_chunk->m_northNeighbor->m_isCpuMeshDirty = true;
		}
		if (blockIter.m_chunk->m_southNeighbor)
		{
			blockIter.m_chunk->m_southNeighbor->m_isCpuMeshDirty = true;
		}

		if (eastBlock && !eastBlock->IsOpaque() && !eastBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(eastBlockIter);
		}

		if (westBlock && !westBlock->IsOpaque() && !westBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(westBlockIter);
		}

		if (northBlock && !northBlock->IsOpaque() && !northBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(northBlockIter);
		}

		if (southBlock && !southBlock->IsOpaque() && !southBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(southBlockIter);
		}

		if (skywardBlock && !skywardBlock->IsOpaque() && !skywardBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(skywardBlockIter);
		}

		if (groundwardBlock && !groundwardBlock->IsOpaque() && !groundwardBlock->IsLightDirty())
		{
			MarkBlockLightingDirty(groundwardBlockIter);
		}
	}
}
