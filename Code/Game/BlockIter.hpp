#pragma once

#include "Game/GameCommon.hpp"

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"

class Chunk;
struct Block;

class BlockIter
{
public:
	~BlockIter() = default;
	BlockIter() = default;
	BlockIter(Chunk* chunk, int blockIndex);

	Block* GetBlock() const;
	BlockIter GetEastBlock() const;
	BlockIter GetWestBlock() const;
	BlockIter GetNorthBlock() const;
	BlockIter GetSouthBlock() const;
	BlockIter GetSkywardBlock() const;
	BlockIter GetGroundwardBlock() const;
	Vec3 GetWorldCenter() const;

	Rgba8 GetFaceTintForLightInfluenceValues(Direction direction) const;

public:
	Chunk* m_chunk = nullptr;
	int m_blockIndex = -1;
};

