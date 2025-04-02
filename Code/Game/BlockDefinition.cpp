#include "Game/BlockDefinition.hpp"

#include "Game/GameCommon.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"


std::vector<BlockDefinition> BlockDefinition::s_blockDefs;

void BlockDefinition::InitializeBlockDefinitions()
{
	CreateNewBlockDef("air",			false,	false,	false, false,	IntVec2::ZERO,		IntVec2::ZERO,			IntVec2::ZERO, 0);
	CreateNewBlockDef("grass",			true,	true,	true, false,	IntVec2(32, 33),	IntVec2(33, 33),		IntVec2(32, 34), 0);
	CreateNewBlockDef("dirt",			true,	true,	true, false,	IntVec2(32, 34),	IntVec2(32, 34),		IntVec2(32, 34), 0);
	CreateNewBlockDef("stone",			true,	true,	true, false,	IntVec2(33, 32),	IntVec2(33, 32),		IntVec2(33, 32), 0);
	CreateNewBlockDef("water",			true,	false,	false, true,	IntVec2(32, 44),	IntVec2(32, 44),		IntVec2(32, 44), 0);
	CreateNewBlockDef("bricks",			true,	true,	true, false,	IntVec2(34, 32),	IntVec2(34, 32),		IntVec2(34, 32), 0);
	CreateNewBlockDef("coal",			true,	true,	true, false,	IntVec2(63, 34),	IntVec2(63, 34),		IntVec2(63, 34), 0);
	CreateNewBlockDef("iron",			true,	true,	true, false,	IntVec2(63, 35),	IntVec2(63, 35),		IntVec2(63, 35), 0);
	CreateNewBlockDef("gold",			true,	true,	true, false,	IntVec2(63, 36),	IntVec2(63, 36),		IntVec2(63, 36), 0);
	CreateNewBlockDef("diamond",		true,	true,	true, false,	IntVec2(63, 37),	IntVec2(63, 37),		IntVec2(63, 37), 0);
	CreateNewBlockDef("glowstone",		true,	true,	true, false,	IntVec2(46, 34),	IntVec2(46, 34),		IntVec2(46, 34), 15);
	CreateNewBlockDef("cobblestone",	true,	true,	true, false,	IntVec2(42, 40),	IntVec2(42, 40),		IntVec2(42, 40), 0);
	CreateNewBlockDef("ice",			true,	true,	true, false,	IntVec2(36, 35),	IntVec2(36, 35),		IntVec2(36, 35), 0);
	CreateNewBlockDef("sand",			true,	true,	true, false,	IntVec2(34, 34),	IntVec2(34, 34),		IntVec2(34, 34), 0);
	CreateNewBlockDef("oakLog",			true,	true,	true, false,	IntVec2(38, 33),	IntVec2(36, 33),		IntVec2(38, 33), 0);
	CreateNewBlockDef("oakLeaf",		true,	true,	true, false,	IntVec2(32, 35),	IntVec2(32, 35),		IntVec2(32, 35), 0);
	CreateNewBlockDef("spruceLog",		true,	true,	true, false,	IntVec2(38, 33),	IntVec2(38, 33),		IntVec2(38, 33), 0);
	CreateNewBlockDef("spruceLeaf",		true,	true,	true, false,	IntVec2(34, 35),	IntVec2(34, 35),		IntVec2(34, 35), 0);
	CreateNewBlockDef("cactus",			true,	true,	true, false,	IntVec2(38, 36),	IntVec2(37, 36),		IntVec2(39, 36), 0);
	CreateNewBlockDef("snowyGrass",		true,	true,	true, false,	IntVec2(36, 35),	IntVec2(33, 35),		IntVec2(32, 34), 0);
	
}

BlockDefinition::BlockDefinition(XmlElement const* element)
{
	UNUSED(element);
}

void BlockDefinition::CreateNewBlockDef(std::string blockTypeName, bool visible, bool solid, bool opaque, bool isWater, IntVec2 const& topSpriteCoords, IntVec2 const& sideSpriteCoords, IntVec2 const& bottomSpriteCoords, int lightInfluence)
{
	BlockDefinition newBlockDef;
	newBlockDef.m_name = blockTypeName;
	newBlockDef.m_isVisible = visible;
	newBlockDef.m_isOpaque = opaque;
	newBlockDef.m_isSolid = solid;
	newBlockDef.m_isWater = isWater;
	newBlockDef.m_topTextureUVs = g_spritesheet->GetSpriteUVs(topSpriteCoords.y * 64 + topSpriteCoords.x);
	newBlockDef.m_sideTextureUVs = g_spritesheet->GetSpriteUVs(sideSpriteCoords.y * 64 + sideSpriteCoords.x);
	newBlockDef.m_bottomTextureUVs = g_spritesheet->GetSpriteUVs(bottomSpriteCoords.y * 64 + bottomSpriteCoords.x);
	newBlockDef.m_lightInfluence = lightInfluence;
	s_blockDefs.push_back(newBlockDef);
}

unsigned char BlockDefinition::GetBlockIDByName(std::string blockName)
{
	for (int blockDefIndex = 0; blockDefIndex < (int)s_blockDefs.size(); blockDefIndex++)
	{
		if (!strcmp(s_blockDefs[blockDefIndex].m_name.c_str(), blockName.c_str()))
		{
			return (unsigned char)blockDefIndex;
		}
	}

	ERROR_AND_DIE(Stringf("Attempted to get undefined block \"%s\"", blockName.c_str()));
}

AABB2 BlockDefinition::GetTopTextureUVs() const
{
	return m_topTextureUVs;
}
