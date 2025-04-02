#pragma once

#include "Game/BlockDefinition.hpp"

#include "Engine/Core/Vertex_PCU.hpp"

typedef unsigned char BlockDefinitionID;

constexpr BlockDefinitionID BLOCKTYPE_INVALID = 0xFF;

constexpr int INDOOR_LIGHTING_BITS = 4;
constexpr int OUTDOOR_LIGHTING_BITS = 4;
constexpr unsigned char INDOOR_LIGHTING_BITMASK = (1 << INDOOR_LIGHTING_BITS) - 1;
constexpr unsigned char OUTDOOR_LIGHTING_BITMASK = (1 << OUTDOOR_LIGHTING_BITS) - 1;
constexpr unsigned char INDOOR_LIGHTINFLUENCE_MAX = INDOOR_LIGHTING_BITMASK;
constexpr unsigned char OUTDOOR_LIGHTINFLUENCE_MAX = OUTDOOR_LIGHTING_BITMASK;

constexpr unsigned char SOLID_BITMASK = 1;
constexpr unsigned char OPAQUE_BITMASK = 2;
constexpr unsigned char SKY_BITMASK = 4;
constexpr unsigned char LIGHTDIRTY_BITMASK = 8;
constexpr unsigned char VISIBLE_BITMASK = 16;
constexpr unsigned char WATER_BITMASK = 32;


struct Block
{
public:
	BlockDefinitionID m_type = BLOCKTYPE_INVALID;
	unsigned char m_lightInfluence = 0;
	unsigned char m_bitFlags = 0;

public:
	~Block() = default;
	Block() = default;
	explicit Block(unsigned char blockType);
	void SetTypeID(BlockDefinitionID newType);
	BlockDefinitionID GetTypeID() const;
	BlockDefinition GetDefinition() const;
	
	bool IsVisible() const;
	bool IsSolid() const;
	bool IsSky() const;
	bool IsOpaque() const;
	bool IsLightDirty() const;
	bool IsWater() const;
	
	void SetVisible(bool visible);
	void SetSolid(bool solid);
	void SetSky(bool isSky);
	void SetOpaque(bool opaque);
	void SetLightDirty(bool isDirty);
	void SetWater(bool isWater);
	
	int GetOutdoorLightInfluence() const;
	int GetIndoorLightInfluence() const;
	void SetOutdoorLightInfluence(int outdoorLightInfluence);
	void SetIndoorLightInfluence(int indoorLightInfluence);
};
