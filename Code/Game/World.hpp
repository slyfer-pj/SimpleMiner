#pragma once
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include "Game/BlockIterator.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

class Game;
class Chunk;
class ConstantBuffer;
class Shader;
class Entity;
class GameCamera;

struct GameRaycastResult3D : public RaycastHit3D
{
	BlockIterator m_blockImpacted;
};

struct ChunkSort
{
public:
	ChunkSort(Vec2 camPos)
		:m_camPos(camPos)
	{
	}

	bool operator()(Chunk* a, Chunk* b);

private:
	Vec2 m_camPos = Vec2::ZERO;
};

class World
{
public:
	World(Game* game);
	~World();
	void Update(float deltaSeconds);
	void Render() const;
	int GetNumberOfChunks() const { return (int)m_activeChunks.size(); }
	int GetTotalNumberOfVerticesInChunks() const { return m_totalChunkMeshVertices; }
	float GetWorldTime() const { return m_worldTime; }
	void AddToTotalNumberOfVerticesInChunks(int verts);
	void DigBlock();
	void AddBlock();
	void MarkLightingDirty(const BlockIterator& blockIter);
	void MarkLightingDirtyIfNotOpaque(const BlockIterator& blockIter);
	void MarkLightingDirtyIfNotSkyAndNotOpaque(const BlockIterator& blockIter);
	Entity* GetPlayer() const { return m_player; }
	Chunk* GetChunk(IntVec2 chunkCoords) const;
	GameRaycastResult3D RaycastVsWorld(const Vec3& start, const Vec3& direction, float distance);
	Game* GetGame() const { return m_game; }

public:
	int m_blockTypeToAdd = 1;
	int m_worldSeed = 0;

	//threading queues and mutexes
	std::map<IntVec2, Chunk*> m_chunksQueuedForGeneration;

private:
	Game* m_game = nullptr;

	//entities
	std::vector<Entity*> m_allEntities = {};
	Entity* m_player = nullptr;

	//chunk data
	std::map<IntVec2, Chunk*> m_activeChunks;
	std::deque<BlockIterator> m_dirtyLightBlocks;
	int m_totalChunkMeshVertices = 0;
	float m_chunkActivationRange = 0.f;
	float m_chunkDeactivationRange = 0.f;
	int m_maxChunkRadiusX = 0;
	int m_maxChunkRadiusY = 0;
	int m_maxChunks = 0;

	//debug
	bool m_debugStepLighting = false;
	bool m_performNextLightingStep = false;
	bool m_placeBlockAtCurrentPos = false;

	//lighting
	Shader* m_shader = nullptr;
	ConstantBuffer* m_gameCBO = nullptr;
	Rgba8 m_indoorLightColor;
	Rgba8 m_dayOutdoorLightColor;
	Rgba8 m_nightOutdoorLightColor;
	Rgba8 m_currentOutdoorLightColor;
	Rgba8 m_daySkyColor;
	Rgba8 m_nightSkyColor;
	Rgba8 m_currentSkyColor;
	float m_glowStrength = 0.f;
	float m_lightingStrength = 0.f;

	//fog
	float m_fogStartDistance = 0.f;
	float m_fogEndDistance = 0.f;
	float m_fogMaxAlpha = 0.f;

	//raycast
	Vec3 m_cameraStart = Vec3::ZERO;
	Vec3 m_cameraForward = Vec3::ZERO;
	GameRaycastResult3D m_raycastResult;
	bool m_freezeRaycastStart = false;

	//world time
	float m_worldTime = 0.5f;
	float m_worldTimeScale = 0.f;
	float m_currentWorldTimeScaleAccelerationFactor = 1.f;

	//cameras
	GameCamera* m_playerWorldCamera = nullptr;

private:
	void HandleDebugInput();
	void AddDebugVertsForChunk(std::vector<Vertex_PCU>& verts) const;
	void AddDebugVertsForLighting(std::vector<Vertex_PCU>& verts) const;
	void InstantiateChunk();
	bool ActivateChunk();
	void DeactivateChunk();
	//void SpawnChunk(const IntVec2& chunkCoords);
	void UpdateChunks(float deltaSeconds);
	void UpdateEntities(float deltaSeconds);
	void UpdateCameras(float deltaSeconds);
	void RenderChunks() const;
	void RenderEntities() const;
	void RenderRaycastImpact() const;
	void UpdateDayCycle(float deltaSeconds);

	void ProcessDirtyLighting();
	uint8_t ComputeIndoorLightInfluence(const BlockIterator& blockIter) const;
	uint8_t ComputeOutdoorLightInfluence(const BlockIterator& blockIter) const;
	uint8_t GetHighestOutdoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const;
	uint8_t GetHighestIndoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const;
	void MarkNeighbouringChunksAndBlocksAsDirty(const BlockIterator& blockIter);
	void CopyDataToCBOAndBindIt() const;

	void PerformRaycast();

};

