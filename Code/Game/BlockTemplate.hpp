#pragma once

#include "Game/Block.hpp"
#include "Game/BlockDefinition.hpp"

#include "Engine/Math/IntVec3.hpp"

#include <map>
#include <string>
#include <vector>


struct BlockTemplateEntry
{
public:
	IntVec3 m_offset;
	BlockDefinitionID m_blockType;
};

class BlockTemplate
{
public:
	BlockTemplate() = default;
	BlockTemplate(BlockTemplate const& copyFrom) = default;
	BlockTemplate(std::vector<BlockTemplateEntry> const& blockTemplateEntries);

public:
	std::vector<BlockTemplateEntry> m_blockTemplateEntries;

public:
	static void InitializeBlockTemplates();

public:
	static std::map<std::string, BlockTemplate> s_blockTemplates;
};

struct BlockTemplateToDo
{
public:
	BlockTemplate const* m_blockTemplate = nullptr;
	IntVec3 m_root = IntVec3::ZERO;

public:
	BlockTemplateToDo(IntVec3 const& root, BlockTemplate const* blockTemplate);
};

