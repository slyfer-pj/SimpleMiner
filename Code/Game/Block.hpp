#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Core/EngineCommon.hpp"

class Texture;
class SpriteSheet;

constexpr uint8_t BLOCK_BIT_IS_SKY = 0x01;
constexpr uint8_t BLOCK_BIT_IS_LIGHT_DIRTY = 0x02;
constexpr uint8_t BLOCK_BIT_CLEAR_CURRENT_DIG_STATE = 0b11100011;

struct Block
{
public:
	uint8_t m_typeIndex = 0;
	uint8_t m_lightInfluence = 0; //higher 4 bits are outdoor lighting influence (max 15), lower 4 bits are indoor lighting influence (max 15)
	uint8_t m_bitflags = 0; // [111(unused)111(digstate)1(light state)1(sky state)]

public:
	bool IsBlockSky() const;
	void SetIsBlockSky(bool isSky);
	bool IsBlockLightDirty() const;
	void SetIsBlockLightDirty(bool isLightDirty);
	uint8_t GetIndoorLightInfluence() const;
	void SetIndoorLightInfluence(int lightInfluence);
	uint8_t GetOutdoorLightInfluence() const;
	void SetOutdoorLightInfluence(int lightInfluence);
	bool IsBlockWater() const;
	uint8_t GetCurrentDugState() const;
	void IncrementDugState();
};

class BlockDefintion
{
public:
	std::string m_name;
	bool m_visible = false;
	bool m_solid = false;
	bool m_opaque = false;
	uint8_t m_indoorLightInfluence = 0;
	AABB2 m_topUVs = AABB2::ZERO_TO_ONE;
	AABB2 m_bottomUVs = AABB2::ZERO_TO_ONE;
	AABB2 m_sideUVs = AABB2::ZERO_TO_ONE;

	static Texture* s_blockSpriteTexture;
	static std::vector<BlockDefintion> s_definitions;
	static std::vector<AABB2> s_digCrackUVs;

public:
	static const BlockDefintion& GetDefinitionByName(const std::string& name);
	static uint8_t GetDefinitionIndexByName(const std::string& name);
	static void CreateAllDefintions();
	static void CreateDefinition(const std::string& name, bool isVisible, bool isSolid, bool isOpaque, uint8_t indoorLightInfluence, const SpriteSheet& blockTextureSheet,
		const IntVec2& topfaceSpriteCoords, const IntVec2& botfaceSpriteCoords, const IntVec2& sidefaceSpriteCoords);
	static bool IsBlockTypeOpaque(int blockDefIndex);
	static bool DoesBlockTypeEmitLight(int blockDefIndex);
};

struct BlockTemplateEntry
{
public:
	uint8_t m_blockTypeIndex = 0;
	IntVec3 m_relativeOffset = IntVec3::ZERO;
};

struct BlockTemplate
{
public:
	std::string m_name;
	std::vector<BlockTemplateEntry> m_blockTemplateEntries;

	static std::vector<BlockTemplate> s_templates;

public:
	bool LoadFromXmlElement(const XmlElement& element);
	static void InitializeTemplates(const char* path);
	static BlockTemplate GetTemplateFromName(const std::string& name);
};