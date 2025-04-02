#include "Game/BlockIter.hpp"

#include "Game/Block.hpp"
#include "Game/Chunk.hpp"

#include "Engine/Math/IntVec3.hpp"


BlockIter::BlockIter(Chunk* chunk, int blockIndex)
	: m_chunk(chunk)
	, m_blockIndex(blockIndex)
{
}

Block* BlockIter::GetBlock() const
{
	if (!m_chunk)
	{
		return nullptr;
	}
	
	return &m_chunk->m_blocks[m_blockIndex];
}

BlockIter BlockIter::GetEastBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.x == CHUNK_SIZE_X - 1)
	{
		return BlockIter(m_chunk->m_eastNeighbor, m_blockIndex & ~CHUNK_BITMASK_X);
	}

	return BlockIter(m_chunk, m_blockIndex + 1);
}

BlockIter BlockIter::GetWestBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.x == 0)
	{
		return BlockIter(m_chunk->m_westNeighbor, m_blockIndex | CHUNK_BITMASK_X);
	}

	return BlockIter(m_chunk, m_blockIndex - 1);
}

BlockIter BlockIter::GetNorthBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.y == CHUNK_SIZE_Y - 1)
	{
		return BlockIter(m_chunk->m_northNeighbor, m_blockIndex & ~(CHUNK_BITMASK_Y << CHUNK_XBITS));
	}

	return BlockIter(m_chunk, m_blockIndex + CHUNK_SIZE_X);
}

BlockIter BlockIter::GetSouthBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.y == 0)
	{
		return BlockIter(m_chunk->m_southNeighbor, m_blockIndex | (CHUNK_BITMASK_Y << CHUNK_XBITS));
	}

	return BlockIter(m_chunk, m_blockIndex - CHUNK_SIZE_X);
}

BlockIter BlockIter::GetSkywardBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.z == CHUNK_SIZE_Z - 1)
	{
		return BlockIter(nullptr, -1);
	}

	return BlockIter(m_chunk, m_blockIndex + CHUNK_BLOCKS_PER_LAYER);
}

BlockIter BlockIter::GetGroundwardBlock() const
{
	if (m_chunk == nullptr)
	{
		return BlockIter(m_chunk, -1);
	}

	IntVec3 blockCoords = m_chunk->GetBlockCoordsFromIndex(m_blockIndex);
	if (blockCoords.z == 0)
	{
		return BlockIter(nullptr, -1);
	}

	return BlockIter(m_chunk, m_blockIndex - CHUNK_BLOCKS_PER_LAYER);
}

Vec3 BlockIter::GetWorldCenter() const
{
	if (!m_chunk)
	{
		return Vec3::ZERO;
	}

	Vec3 blockCoordsInChunkSpace = m_chunk->GetBlockCoordsFromIndex(m_blockIndex).GetAsVec3();
	return blockCoordsInChunkSpace + m_chunk->m_worldPosition + Vec3(0.5f, 0.5f, 0.5f);
}

Rgba8 BlockIter::GetFaceTintForLightInfluenceValues(Direction direction) const
{
	Rgba8 color = Rgba8::WHITE;
	int indoorLightInfluence = 0;
	int outdoorLightInfluence = 0;

	switch (direction)
	{
		case Direction::EAST:
		{
			color = Rgba8(230, 230, 230, 255);
			Block* eastBlock = GetEastBlock().GetBlock();
			indoorLightInfluence = eastBlock ? eastBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = eastBlock ? eastBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
		case Direction::WEST:
		{
			color = Rgba8(230, 230, 230, 255);
			Block* westBlock = GetWestBlock().GetBlock();
			indoorLightInfluence = westBlock ? westBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = westBlock ? westBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
		case Direction::NORTH:
		{
			color = Rgba8(200, 200, 200, 255);
			Block* northBlock = GetNorthBlock().GetBlock();
			indoorLightInfluence = northBlock ? northBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = northBlock ? northBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
		case Direction::SOUTH:
		{
			color = Rgba8(200, 200, 200, 255);
			Block* southBlock = GetSouthBlock().GetBlock();
			indoorLightInfluence = southBlock ? southBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = southBlock ? southBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
		case Direction::SKYWARD:
		{
			color = Rgba8::WHITE;
			Block* skywardBlock = GetSkywardBlock().GetBlock();
			indoorLightInfluence = skywardBlock ? skywardBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = skywardBlock ? skywardBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
		case Direction::GROUNDWARD:
		{
			color = Rgba8::WHITE;
			Block* groundwardBlock = GetGroundwardBlock().GetBlock();
			indoorLightInfluence = groundwardBlock ? groundwardBlock->GetIndoorLightInfluence() : 0;
			outdoorLightInfluence = groundwardBlock ? groundwardBlock->GetOutdoorLightInfluence() : 0;
			break;
		}
	}

	unsigned char red = (unsigned char)RangeMapClamped((float)outdoorLightInfluence, 0.f, (float)OUTDOOR_LIGHTING_BITMASK, 0.f, (float)color.r);
	unsigned char green = (unsigned char)RangeMapClamped((float)indoorLightInfluence, 0.f, (float)INDOOR_LIGHTING_BITMASK, 0.f, (float)color.g);
	unsigned char blue = 0;

	return Rgba8(red, green, blue, 255);
}
