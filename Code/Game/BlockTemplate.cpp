#include "BlockTemplate.hpp"


std::map<std::string, BlockTemplate> BlockTemplate::s_blockTemplates;

void BlockTemplate::InitializeBlockTemplates()
{
	//------------------------------------------------------------------------------------------
	// Cactus
	BlockDefinitionID cactusBlockID = BlockDefinition::GetBlockIDByName("cactus");
	std::vector<BlockTemplateEntry> cactusTemplateEntries;
	BlockTemplateEntry cactus0;
	cactus0.m_offset = IntVec3(0, 0, 0);
	cactus0.m_blockType = cactusBlockID;
	cactusTemplateEntries.push_back(cactus0);
	BlockTemplateEntry cactus1;
	cactus1.m_offset = IntVec3(0, 0, 1);
	cactus1.m_blockType = cactusBlockID;
	cactusTemplateEntries.push_back(cactus1);
	BlockTemplateEntry cactus2;
	cactus2.m_offset = IntVec3(0, 0, 2);
	cactus2.m_blockType = cactusBlockID;
	cactusTemplateEntries.push_back(cactus2);
	s_blockTemplates["cactusTemplate"] = BlockTemplate(cactusTemplateEntries);
	//------------------------------------------------------------------------------------------
	
	//------------------------------------------------------------------------------------------
	// Oak
	BlockDefinitionID oakLogBlockID = BlockDefinition::GetBlockIDByName("oakLog");
	BlockDefinitionID oakLeafBlockID = BlockDefinition::GetBlockIDByName("oakLeaf");
	std::vector<BlockTemplateEntry> oakTemplateEntries;
	for (int logIdx = 0; logIdx < 7; logIdx++)
	{
		BlockTemplateEntry oakLog;
		oakLog.m_offset = IntVec3(0, 0, logIdx);
		oakLog.m_blockType = oakLogBlockID;
		oakTemplateEntries.push_back(oakLog);
	}

	// 1 block away from log
	BlockTemplateEntry oakLeaf;
	oakLeaf.m_offset = IntVec3(1, 0, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);
	
	oakLeaf.m_offset = IntVec3(1, 1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(1, -1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-1, -1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);
	
	oakLeaf.m_offset = IntVec3(-1, 0, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-1, 1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(0, 1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(0, -1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	// 2 blocks away from log
	oakLeaf.m_offset = IntVec3(2, 0, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-2, 0, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(0, 2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(0, -2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);
	//
	oakLeaf.m_offset = IntVec3(2, 1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);
	
	oakLeaf.m_offset = IntVec3(2, -1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-2, 1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-2, -1, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-1, -2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(-1, 2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(1, -2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	oakLeaf.m_offset = IntVec3(1, 2, 3);
	oakLeaf.m_blockType = oakLeafBlockID;
	oakTemplateEntries.push_back(oakLeaf);

	for (int zOffset = 4; zOffset < 7; zOffset++)
	{
		oakLeaf.m_offset = IntVec3(1, 0, zOffset);
		oakLeaf.m_blockType = oakLeafBlockID;
		oakTemplateEntries.push_back(oakLeaf);

		oakLeaf.m_offset = IntVec3(-1, 0, zOffset);
		oakLeaf.m_blockType = oakLeafBlockID;
		oakTemplateEntries.push_back(oakLeaf);

		oakLeaf.m_offset = IntVec3(0, 1, zOffset);
		oakLeaf.m_blockType = oakLeafBlockID;
		oakTemplateEntries.push_back(oakLeaf);

		oakLeaf.m_offset = IntVec3(0, -1, zOffset);
		oakLeaf.m_blockType = oakLeafBlockID;
		oakTemplateEntries.push_back(oakLeaf);
	}

	s_blockTemplates["oakTemplate"] = BlockTemplate(oakTemplateEntries);
	//------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------
	// Spruce
	BlockDefinitionID spruceLogBlockID = BlockDefinition::GetBlockIDByName("spruceLog");
	BlockDefinitionID spruceLeafBlockID = BlockDefinition::GetBlockIDByName("spruceLeaf");
	std::vector<BlockTemplateEntry> spruceTemplateEntries;
	for (int logIdx = 0; logIdx < 8; logIdx++)
	{
		BlockTemplateEntry spruceLog;
		spruceLog.m_offset = IntVec3(0, 0, logIdx);
		spruceLog.m_blockType = spruceLogBlockID;
		spruceTemplateEntries.push_back(spruceLog);
	}

	BlockTemplateEntry spruceLeaf;
	for (int zOffset = 3; zOffset < 8; zOffset++)
	{
		spruceLeaf.m_offset = IntVec3(1, 0, zOffset);
		spruceLeaf.m_blockType = spruceLeafBlockID;
		spruceTemplateEntries.push_back(spruceLeaf);

		spruceLeaf.m_offset = IntVec3(-1, 0, zOffset);
		spruceLeaf.m_blockType = spruceLeafBlockID;
		spruceTemplateEntries.push_back(spruceLeaf);

		spruceLeaf.m_offset = IntVec3(0, 1, zOffset);
		spruceLeaf.m_blockType = spruceLeafBlockID;
		spruceTemplateEntries.push_back(spruceLeaf);

		spruceLeaf.m_offset = IntVec3(0, -1, zOffset);
		spruceLeaf.m_blockType = spruceLeafBlockID;
		spruceTemplateEntries.push_back(spruceLeaf);
	}

	s_blockTemplates["spruceTemplate"] = BlockTemplate(spruceTemplateEntries);
}

BlockTemplate::BlockTemplate(std::vector<BlockTemplateEntry> const& blockTemplateEntries)
	: m_blockTemplateEntries(blockTemplateEntries)
{
	//m_blockTemplateEntries.resize(blockTemplateEntries.size());
	//for (int blockTemplateEntryIdx = 0; blockTemplateEntryIdx < (int)m_blockTemplateEntries.size(); blockTemplateEntryIdx++)
	//{
	//	m_blockTemplateEntries[blockTemplateEntryIdx] = blockTemplateEntries[blockTemplateEntryIdx];
	//}
}

BlockTemplateToDo::BlockTemplateToDo(IntVec3 const& root, BlockTemplate const* blockTemplate)
	: m_root(root)
	, m_blockTemplate(blockTemplate)
{
}
