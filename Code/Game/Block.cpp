#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Renderer/SpriteSheet.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Game/Block.hpp"

extern Renderer* g_theRenderer;

std::vector<BlockDefintion> BlockDefintion::s_definitions = {};
std::vector<AABB2> BlockDefintion::s_digCrackUVs = {};
Texture* BlockDefintion::s_blockSpriteTexture = nullptr;

std::vector<BlockTemplate> BlockTemplate::s_templates = {};

const BlockDefintion& BlockDefintion::GetDefinitionByName(const std::string& name)
{
	for (int i = 0; i < s_definitions.size(); i++)
	{
		if (s_definitions[i].m_name == name)
			return s_definitions[i];
	}

	std::string message = "Invalid block name ";
	message.append(name);
	ERROR_AND_DIE(message);
}

uint8_t BlockDefintion::GetDefinitionIndexByName(const std::string& name)
{
	for (int i = 0; i < s_definitions.size(); i++)
	{
		if (s_definitions[i].m_name == name)
			return static_cast<uint8_t>(i);
	}

	std::string message = "Invalid block name ";
	message.append(name);
	ERROR_AND_DIE(message);
}

void BlockDefintion::CreateAllDefintions()
{
	s_definitions.reserve(8);
	s_blockSpriteTexture = g_theRenderer->CreateOrGetTextureFromFile("Data/Images/BasicSprites_64x64.png");
	SpriteSheet spriteSheet = SpriteSheet(*s_blockSpriteTexture, IntVec2(64, 64));
	CreateDefinition("air", false, false, false, 0, spriteSheet, IntVec2::ZERO, IntVec2::ZERO, IntVec2::ZERO);
	CreateDefinition("grass", true, true, true, 0, spriteSheet, IntVec2(32, 33), IntVec2(32, 34), IntVec2(33, 33));
	CreateDefinition("dirt", true, true, true, 0, spriteSheet, IntVec2(32, 34), IntVec2(32, 34), IntVec2(32, 34));
	CreateDefinition("stone", true, true, true, 0, spriteSheet, IntVec2(33, 32), IntVec2(33, 32), IntVec2(33, 32));
	CreateDefinition("brick", true, true, true, 0, spriteSheet, IntVec2(34, 32), IntVec2(34, 32), IntVec2(34, 32));
	CreateDefinition("glowstone", true, true, true, 15, spriteSheet, IntVec2(46, 34), IntVec2(46, 34), IntVec2(46, 34));
	CreateDefinition("water", true, false, false, 0, spriteSheet, IntVec2(32, 44), IntVec2(32, 44), IntVec2(32, 44));
	CreateDefinition("coal", true, true, true, 0, spriteSheet, IntVec2(63, 34), IntVec2(63, 34), IntVec2(63, 34));
	CreateDefinition("cobblestone", true, true, true, 0, spriteSheet, IntVec2(63, 34), IntVec2(63, 34), IntVec2(63, 34));
	CreateDefinition("iron", true, true, true, 0, spriteSheet, IntVec2(63, 35), IntVec2(63, 35), IntVec2(63, 35));
	CreateDefinition("gold", true, true, true, 0, spriteSheet, IntVec2(63, 36), IntVec2(63, 36), IntVec2(63, 36));
	CreateDefinition("diamond", true, true, true, 0, spriteSheet, IntVec2(63, 37), IntVec2(63, 37), IntVec2(63, 37));
	CreateDefinition("sand", true, true, true, 0, spriteSheet, IntVec2(34, 34), IntVec2(34, 34), IntVec2(34, 34));
	CreateDefinition("ice", true, true, true, 0, spriteSheet, IntVec2(36, 35), IntVec2(36, 35), IntVec2(36, 35));
	CreateDefinition("oak log", true, true, true, 0, spriteSheet, IntVec2(38, 33), IntVec2(38, 33), IntVec2(38, 33));
	CreateDefinition("leaves", true, true, true, 0, spriteSheet, IntVec2(32, 35), IntVec2(32, 35), IntVec2(32, 35));
	CreateDefinition("snowgrass", true, true, true, 0, spriteSheet, IntVec2(36, 35), IntVec2(32, 34), IntVec2(33, 35));
	CreateDefinition("cloud", true, true, true, 0, spriteSheet, IntVec2(0, 4), IntVec2(0, 4), IntVec2(0, 4));

	IntVec2 crackBaseCoords = IntVec2(32, 46);
	for (int i = 0; i < 6; i++)
	{
		IntVec2 crackCoords = crackBaseCoords + IntVec2(i, 0);
		int index = crackCoords.x + (64 * crackCoords.y);
		s_digCrackUVs.push_back(spriteSheet.GetSpriteDefinition(index).GetUVs());
	}
}

void BlockDefintion::CreateDefinition(const std::string& name, bool isVisible, bool isSolid, bool isOpaque, uint8_t indoorLightInfluence, const SpriteSheet& blockTextureSheet, const IntVec2& topfaceSpriteCoords, const IntVec2& botfaceSpriteCoords, const IntVec2& sidefaceSpriteCoords)
{
	BlockDefintion def = {name, isVisible, isSolid, isOpaque, indoorLightInfluence};

	bool useWhiteBlocks = g_gameConfigBlackboard.GetValue("debugUseWhiteBlocks", false);
	if (!useWhiteBlocks)
	{
		int topfaceSpriteIndex = topfaceSpriteCoords.x + (blockTextureSheet.GetSize().x * topfaceSpriteCoords.y);
		def.m_topUVs = blockTextureSheet.GetSpriteDefinition(topfaceSpriteIndex).GetUVs();
		int botfaceSpriteIndex = botfaceSpriteCoords.x + (blockTextureSheet.GetSize().x * botfaceSpriteCoords.y);
		def.m_bottomUVs = blockTextureSheet.GetSpriteDefinition(botfaceSpriteIndex).GetUVs();
		int sidefaceSpriteIndex = sidefaceSpriteCoords.x + (blockTextureSheet.GetSize().x * sidefaceSpriteCoords.y);
		def.m_sideUVs = blockTextureSheet.GetSpriteDefinition(sidefaceSpriteIndex).GetUVs();
	}
	else
	{ 
		IntVec2 whiteBlockSpriteCoords = g_gameConfigBlackboard.GetValue("whiteBlockSpriteCoords", IntVec2::ZERO);
		int whiteBlockSpriteIndex = whiteBlockSpriteCoords.x + (blockTextureSheet.GetSize().x * whiteBlockSpriteCoords.y);
		def.m_topUVs = blockTextureSheet.GetSpriteDefinition(whiteBlockSpriteIndex).GetUVs();
		def.m_bottomUVs = blockTextureSheet.GetSpriteDefinition(whiteBlockSpriteIndex).GetUVs();
		def.m_sideUVs = blockTextureSheet.GetSpriteDefinition(whiteBlockSpriteIndex).GetUVs();
	}
	s_definitions.push_back(def);
}

bool BlockDefintion::IsBlockTypeOpaque(int blockDefIndex)
{
	return s_definitions[blockDefIndex].m_opaque;
}

bool BlockDefintion::DoesBlockTypeEmitLight(int blockDefIndex)
{
	return s_definitions[blockDefIndex].m_indoorLightInfluence > 0;
}

bool Block::IsBlockSky() const
{
	return ((m_bitflags & BLOCK_BIT_IS_SKY) == BLOCK_BIT_IS_SKY);
}

void Block::SetIsBlockSky(bool isSky)
{
	if (isSky)
		m_bitflags |= BLOCK_BIT_IS_SKY;
	else
		m_bitflags &= ~BLOCK_BIT_IS_SKY;
}

bool Block::IsBlockLightDirty() const
{
	return ((m_bitflags & BLOCK_BIT_IS_LIGHT_DIRTY) == BLOCK_BIT_IS_LIGHT_DIRTY);
}

void Block::SetIsBlockLightDirty(bool isLightDirty)
{
	if (isLightDirty)
		m_bitflags |= BLOCK_BIT_IS_LIGHT_DIRTY;
	else
		m_bitflags &= ~BLOCK_BIT_IS_LIGHT_DIRTY;
}

uint8_t Block::GetIndoorLightInfluence() const
{
	return (m_lightInfluence & 0x0f);
}

void Block::SetIndoorLightInfluence(int lightInfluence)
{
	m_lightInfluence &= 0xf0;	//clear current indoor light influence
	m_lightInfluence |= lightInfluence;	//set the lower 4 bits with the light influence value
}

uint8_t Block::GetOutdoorLightInfluence() const
{
	return m_lightInfluence >> 4;
}

void Block::SetOutdoorLightInfluence(int lightInfluence)
{
	m_lightInfluence &= 0x0f;	//clear current outdoor light influence
	m_lightInfluence |= (lightInfluence << 4);	//set the higher 4 bits with the light influence value
}

bool Block::IsBlockWater() const
{
	return m_typeIndex == BlockDefintion::GetDefinitionIndexByName("water");
}

uint8_t Block::GetCurrentDugState() const
{
	return m_bitflags >> 2;
}

void Block::IncrementDugState()
{
	uint8_t currentState = GetCurrentDugState();
	currentState++;
	m_bitflags &= BLOCK_BIT_CLEAR_CURRENT_DIG_STATE;	//clear dig state
	m_bitflags |= (currentState << 2);					//set the new dig state value
}

bool BlockTemplate::LoadFromXmlElement(const XmlElement& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);
	
	const XmlElement* templateEntry = element.FirstChildElement("TemplateEntry");
	while (templateEntry)
	{
		BlockTemplateEntry entry;
		entry.m_blockTypeIndex = static_cast<uint8_t>(ParseXmlAttribute(*templateEntry, "blockIndex", entry.m_blockTypeIndex));
		entry.m_relativeOffset = ParseXmlAttribute(*templateEntry, "offset", entry.m_relativeOffset);
		m_blockTemplateEntries.push_back(entry);
		templateEntry = templateEntry->NextSiblingElement();
	}

	return true;
}

void BlockTemplate::InitializeTemplates(const char* path)
{
	tinyxml2::XMLDocument doc;
	tinyxml2::XMLError status = doc.LoadFile(path);
	GUARANTEE_OR_DIE(status == tinyxml2::XML_SUCCESS, "Failed to load block templates data file");
	tinyxml2::XMLElement* rootElement = doc.RootElement();
	tinyxml2::XMLElement* childElement = rootElement->FirstChildElement();

	while (childElement)
	{
		BlockTemplate blockTemplate;
		blockTemplate.LoadFromXmlElement(*childElement);
		BlockTemplate::s_templates.push_back(blockTemplate);
		childElement = childElement->NextSiblingElement();
	}
}

BlockTemplate BlockTemplate::GetTemplateFromName(const std::string& name)
{
	for (int i = 0; i < s_templates.size(); i++)
	{
		if (s_templates[i].m_name == name)
			return s_templates[i];
	}

	std::string message = "Invalid block template name ";
	message.append(name);
	ERROR_AND_DIE(message);
}
