#include "Game/Block.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"


Block::Block(unsigned char blockType)
	: m_type(blockType)
{
	BlockDefinition definition = BlockDefinition::s_blockDefs[blockType];

	SetSolid(definition.m_isSolid);
	SetOpaque(definition.m_isOpaque);
	SetVisible(definition.m_isVisible);
	SetWater(definition.m_isWater);
}

void Block::SetTypeID(BlockDefinitionID newType)
{	
	m_type = newType;
	BlockDefinition definition = BlockDefinition::s_blockDefs[newType];
	SetSolid(definition.m_isSolid);
	SetOpaque(definition.m_isOpaque);
	SetVisible(definition.m_isVisible);
	SetWater(definition.m_isWater);
}

BlockDefinitionID Block::GetTypeID() const
{
	return m_type;
}

BlockDefinition Block::GetDefinition() const
{
	return BlockDefinition::s_blockDefs[m_type];
}

bool Block::IsVisible() const
{
	return (m_bitFlags & VISIBLE_BITMASK);
}

bool Block::IsSolid() const
{
	return (m_bitFlags & SOLID_BITMASK);
}

bool Block::IsSky() const
{
	return (m_bitFlags & SKY_BITMASK);
}

bool Block::IsOpaque() const
{
	return (m_bitFlags & OPAQUE_BITMASK);
}

bool Block::IsLightDirty() const
{
	return (m_bitFlags & LIGHTDIRTY_BITMASK);
}

bool Block::IsWater() const
{
	return (m_bitFlags & WATER_BITMASK);
}

void Block::SetVisible(bool visible)
{
	if (visible)
	{
		m_bitFlags |= VISIBLE_BITMASK;
		return;
	}

	m_bitFlags &= ~VISIBLE_BITMASK;
}

void Block::SetSolid(bool solid)
{
	if (solid)
	{
		m_bitFlags |= SOLID_BITMASK;
		return;
	}

	m_bitFlags &= ~SOLID_BITMASK;
}

void Block::SetSky(bool isSky)
{
	if (isSky)
	{
		m_bitFlags |= SKY_BITMASK;
		return;
	}

	m_bitFlags &= ~SKY_BITMASK;
}

void Block::SetOpaque(bool opaque)
{
	if (opaque)
	{
		m_bitFlags |= OPAQUE_BITMASK;
		return;
	}

	m_bitFlags &= ~OPAQUE_BITMASK;
}

void Block::SetLightDirty(bool isLightDirty)
{
	if (isLightDirty)
	{
		m_bitFlags |= LIGHTDIRTY_BITMASK;
		return;
	}

	m_bitFlags &= ~LIGHTDIRTY_BITMASK;
}

void Block::SetWater(bool isWater)
{
	if (isWater)
	{
		m_bitFlags |= WATER_BITMASK;
		return;
	}

	m_bitFlags &= ~WATER_BITMASK;
}

int Block::GetOutdoorLightInfluence() const
{
	return (m_lightInfluence >> INDOOR_LIGHTING_BITS) & OUTDOOR_LIGHTING_BITMASK;
}

int Block::GetIndoorLightInfluence() const
{
	return (m_lightInfluence & INDOOR_LIGHTING_BITMASK);
}

void Block::SetOutdoorLightInfluence(int outdoorLightInfluence)
{
	m_lightInfluence &= ~(OUTDOOR_LIGHTING_BITMASK << INDOOR_LIGHTING_BITS);
	m_lightInfluence |= (outdoorLightInfluence & OUTDOOR_LIGHTING_BITMASK) << INDOOR_LIGHTING_BITS;
}

void Block::SetIndoorLightInfluence(int indoorLightInfluence)
{
	m_lightInfluence &= ~INDOOR_LIGHTING_BITMASK;
	m_lightInfluence |= indoorLightInfluence & INDOOR_LIGHTING_BITMASK;
}

