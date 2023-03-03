#pragma once
#include "Engine/Math/AABB3.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Job.hpp"
#include "Game/Block.hpp"

class World;
class VertexBuffer;
class IndexBuffer;
struct IntVec3;
struct BlockIterator;

constexpr int CHUNK_BITS_X = 4;
constexpr int CHUNK_BITS_Y = 4;
constexpr int CHUNK_BITS_Z = 7;

constexpr int CHUNK_SIZE_X = 1 << CHUNK_BITS_X;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_BITS_Y;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_BITS_Z;

constexpr int CHUNK_MAX_X = CHUNK_SIZE_X - 1;
constexpr int CHUNK_MAX_Y = CHUNK_SIZE_Y - 1;
constexpr int CHUNK_MAX_Z = CHUNK_SIZE_Z - 1;

constexpr int CHUNK_TOTAL_BLOCKS = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
constexpr int CHUNK_BLOCKS_PER_LAYER = CHUNK_SIZE_X * CHUNK_SIZE_Y;

enum ChunkState
{
	MISSING,							//chunk not present yet
	ACTIVATING_QUEUED_GENERATE,			//chunk has been queued for generating blocks in a thread
	ACTIVATING_GENERATING,				//worker thread is executing its generation function
	ACTIVATING_GENERATE_COMPLETE,		//worker thread has finished its generation and now can be retrieved by main thread
	ACTIVE								//is in the active chunks list
	//DEACTIVATING						//chunk is being destroyed by main thread
};

class Chunk
{
public:
	Chunk(World* world, const IntVec2& chunkCoordinates);
	~Chunk();
	void Update(float deltaSeconds);
	void Render() const;
	const IntVec2& GetChunkCoordinates() const { return m_chunkCoords; }
	const AABB3& GetChunkWorldBounds() const { return m_worldBounds; }
	int GetChunkMeshVertices() const { return (int)m_cpuMeshOpaqueVertices.size(); }
	IntVec3 GetLocalCoordsFromBlockIndex(int blockIndex) const;
	int GetZHeightOfHighestNonAirBlock(int columnX, int columnY) const;
	void DigBlock(const BlockIterator& blockIter);
	void AddBlock(const BlockIterator& blockIter);
	void SetChunkToDirty();
	Block* GetBlock(int blockIndex) const;
	void InitializeLighting();
	void InitializeBlocks();
	void GenerateGeometry();
	bool ShouldRebuildMesh() const;

	static IntVec2 GetChunkCoordinatedForWorldPosition(const Vec3& position);
	static Vec2 GetChunkCenterXYForGlobalChunkCoords(const IntVec2& chunkCoords);
	static int GetBlockIndexFromLocalCoords(const IntVec3& localCoords);

public:
	std::atomic<ChunkState> m_status = MISSING;
	Chunk* m_northNeighbour = nullptr;
	Chunk* m_eastNeighbour = nullptr;
	Chunk* m_southNeighbour = nullptr;
	Chunk* m_westNeighbour = nullptr;

private:
	World* m_world = nullptr;
	IntVec2 m_chunkCoords = IntVec2::ZERO;
	AABB3 m_worldBounds = AABB3::ZERO_TO_ONE;
	bool m_isChunkDirty = true;
	bool m_needsSaving = false;
	Block* m_blocks = nullptr;
	VertexBuffer* m_gpuMeshOpaqueVBO = nullptr;
	IndexBuffer* m_gpuMeshOpaqueIBO = nullptr;
	VertexBuffer* m_gpuMeshTranslucentVBO = nullptr;
	IndexBuffer* m_gpuMeshTranslucentIBO = nullptr;
	std::vector<Vertex_PCU> m_cpuMeshOpaqueVertices;
	std::vector<unsigned int> m_cpuMeshOpaqueIndicies;
	std::vector<Vertex_PCU> m_cpuMeshTranslucentVertices;
	std::vector<unsigned int> m_cpuMeshTranslucentIndicies;

private:
	void AddVertsForBlock(const BlockDefintion& blockDef, const IntVec3& localCoords);
	void AddVertsForWaterBlock(const BlockDefintion& blockDef, const IntVec3& localCoords);
	//bool IsBlockAtLocalCoordsOpaque(const IntVec3& localCoords);
	bool HasAllValidNeighbours() const;
	bool LoadBlocksFromFile();
	void SaveBlockToFile();
	Rgba8 GetFaceColor(const BlockIterator& blockIterator);
	void ProcessLightingForDugBlock(const BlockIterator& blockIter);
	void ProcessLightingForAddedBlock(const BlockIterator& blockIter);
	bool IsLocalMaximaIn5x5(float refTreeNoise, IntVec2 globalCoordsXY);
	void AddBlocksForTree(const std::string& treeName, const IntVec3& baseCoords);
};

class ChunkGenerationJob : public Job
{
public:
	ChunkGenerationJob(Chunk* chunk);

public:
	Chunk* m_chunk = nullptr;

private:
	virtual void Execute() override;
	virtual void OnFinished() override;
};