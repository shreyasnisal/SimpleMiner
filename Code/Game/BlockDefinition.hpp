#pragma once

#include "Engine/Core/XMLUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

#include <string>
#include <vector>

class BlockDefinition
{
public:
	std::string m_name = "";
	AABB2 m_topTextureUVs;
	AABB2 m_bottomTextureUVs;
	AABB2 m_sideTextureUVs;
	bool m_isVisible = false;
	bool m_isSolid = false;
	bool m_isOpaque = false;
	int m_lightInfluence = 0;
	bool m_isWater = false;

	static std::vector<BlockDefinition> s_blockDefs;

public:
	~BlockDefinition() = default;
	BlockDefinition() = default;
	BlockDefinition(XmlElement const* element);

	AABB2 GetTopTextureUVs() const;

	
	static void InitializeBlockDefinitions();
	static void CreateNewBlockDef(std::string blockTypeName, bool visible, bool solid, bool opaque, bool isWater, IntVec2 const& topSpriteCoords, IntVec2 const& sideSpriteCoords, IntVec2 const& bottomSpriteCoords, int lightInfluence);
	static unsigned char GetBlockIDByName(std::string blockName);
};

