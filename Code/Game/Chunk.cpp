#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "ThirdParty/Squirrel/RawNoise.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Game/Chunk.hpp"
#include "Game/World.hpp"
#include "Game/BlockIterator.hpp"

extern Renderer* g_theRenderer;
extern DevConsole* g_theConsole;
extern JobSystem* g_theJobSystem;

constexpr float MAX_SAND_BLOCKS = 4;
constexpr float MAX_ICE_BLOCKS = 4;
constexpr float MAX_SNOW_BLOCKS = 3;
constexpr int SEA_LEVEL = CHUNK_SIZE_Z / 2;
constexpr int MAX_OCEAN_DEPTH = 55;
constexpr int FREEZING_LEVEL = 87;
constexpr int CLOUD_LEVEL = 110;

Chunk::Chunk(World* world, const IntVec2& chunkCoordinates)
	:m_world(world), m_chunkCoords(chunkCoordinates)
{
	Vec3 mins = Vec3(float(CHUNK_SIZE_X * m_chunkCoords.x), float(CHUNK_SIZE_Y * m_chunkCoords.y), 0.f);
	m_worldBounds = AABB3(mins, mins + Vec3(CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z));
	m_cpuMeshOpaqueVertices.reserve(3000);
	m_cpuMeshOpaqueIndicies.reserve(3000);
	m_cpuMeshTranslucentVertices.reserve(3000);
	m_cpuMeshTranslucentIndicies.reserve(3000);
}

Chunk::~Chunk()
{
 	GUARANTEE_OR_DIE(this != nullptr, "Trying to delete chunk that does not exist");

	if (m_needsSaving)
	{
		float startTime = (float)GetCurrentTimeSeconds();
		SaveBlockToFile();
		float endTime = (float)GetCurrentTimeSeconds();
		g_theConsole->AddLine(g_theConsole->INFO_MAJOR, Stringf("Chunk [%d, %d] took %f time to save to disk", m_chunkCoords.x, m_chunkCoords.y, (endTime - startTime) * 1000.f));
		m_needsSaving = false;
	}
	delete[] m_blocks;
	m_blocks = nullptr;
	delete m_gpuMeshOpaqueVBO;
	m_gpuMeshOpaqueVBO = nullptr;
	delete m_gpuMeshOpaqueIBO;
	m_gpuMeshOpaqueIBO = nullptr;
}

void Chunk::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
}

void Chunk::Render() const
{
	if (!m_gpuMeshOpaqueVBO)
		return;

 	g_theRenderer->BindVertexBuffer(m_gpuMeshOpaqueVBO);
	g_theRenderer->BindIndexBuffer(m_gpuMeshOpaqueIBO);
	g_theRenderer->DrawIndexed((int)m_cpuMeshOpaqueIndicies.size());

	if (m_gpuMeshTranslucentVBO)
	{
		g_theRenderer->BindVertexBuffer(m_gpuMeshTranslucentVBO);
		g_theRenderer->BindIndexBuffer(m_gpuMeshTranslucentIBO);
		g_theRenderer->DrawIndexed((int)m_cpuMeshTranslucentIndicies.size());
	}
}

IntVec2 Chunk::GetChunkCoordinatedForWorldPosition(const Vec3& position)
{
	int x = int(floorf(position.x)) >> CHUNK_BITS_X;
	int y = int(floorf(position.y)) >> CHUNK_BITS_Y;
	return IntVec2(x, y);
}

Vec2 Chunk::GetChunkCenterXYForGlobalChunkCoords(const IntVec2& chunkCoords)
{
	return Vec2(float(CHUNK_SIZE_X * chunkCoords.x), float(CHUNK_SIZE_Y * chunkCoords.y)) + Vec2(float(CHUNK_SIZE_X / 2.f), float(CHUNK_SIZE_Y / 2.f));
}

int Chunk::GetBlockIndexFromLocalCoords(const IntVec3& localCoords)
{
	return localCoords.x | (localCoords.y << CHUNK_BITS_X) | (localCoords.z << (CHUNK_BITS_X + CHUNK_BITS_Y));
}

IntVec3 Chunk::GetLocalCoordsFromBlockIndex(int blockIndex) const
{
	int x = blockIndex & CHUNK_MAX_X;
	int y = (blockIndex >> CHUNK_BITS_X) & CHUNK_MAX_Y;
	int z = (blockIndex >> (CHUNK_BITS_X + CHUNK_BITS_Y)) & CHUNK_MAX_Z;
	return IntVec3(x, y, z);
}

int Chunk::GetZHeightOfHighestNonAirBlock(int columnX, int columnY) const
{
	for (int z = 0; z < CHUNK_SIZE_Z; z++)
	{
		uint8_t blockType = m_blocks[GetBlockIndexFromLocalCoords(IntVec3(columnX, columnY, z))].m_typeIndex;
		if (BlockDefintion::s_definitions[blockType].m_name == "air")
		{
			//get highest air block height and return the height of the block one below it.
			return z - 1;
		}
	}

	return CHUNK_MAX_Z;
}

void Chunk::DigBlock(const BlockIterator& blockIter)
{
	int blockIndex = blockIter.m_blockIndex;
	uint8_t dugState = blockIter.GetBlock()->GetCurrentDugState();
	if (dugState >= BlockDefintion::s_digCrackUVs.size())
	{
		m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("air");
	}
	else
	{
		blockIter.GetBlock()->IncrementDugState();
	}
	m_isChunkDirty = true;
	m_needsSaving = true;

	blockIter.GetWestNeighbour().m_chunkBlockBelongsTo->SetChunkToDirty();
	blockIter.GetEastNeighbour().m_chunkBlockBelongsTo->SetChunkToDirty();
	blockIter.GetSouthNeighbour().m_chunkBlockBelongsTo->SetChunkToDirty();
	blockIter.GetNorthNeighbour().m_chunkBlockBelongsTo->SetChunkToDirty();

	ProcessLightingForDugBlock(blockIter);
}

void Chunk::AddBlock(const BlockIterator& blockIter)
{
	int blockIndex = blockIter.m_blockIndex;
	m_blocks[blockIndex].m_typeIndex = static_cast<uint8_t>(m_world->m_blockTypeToAdd);
	m_isChunkDirty = true;
	m_needsSaving = true;

	ProcessLightingForAddedBlock(blockIter);
}

void Chunk::SetChunkToDirty()
{
	m_isChunkDirty = true;
}

Block* Chunk::GetBlock(int blockIndex) const
{
	return &m_blocks[blockIndex];
}

void Chunk::InitializeBlocks()
{
	m_blocks = new Block[CHUNK_TOTAL_BLOCKS];
	RandomNumberGenerator rng;

	float startTime = (float)GetCurrentTimeSeconds();
	bool loaded = LoadBlocksFromFile();
	float endTime = (float)GetCurrentTimeSeconds();
	if (loaded)
		g_theConsole->AddLine(g_theConsole->INFO_MAJOR, Stringf("Chunk [%d, %d] took %f time to build from disk", m_chunkCoords.x, m_chunkCoords.y, (endTime - startTime) * 1000.f));
	if (!loaded)
	{
		float temperaturePerColumn[CHUNK_BLOCKS_PER_LAYER] = {};
		float humidityPerColumn[CHUNK_BLOCKS_PER_LAYER] = {};
		float terrainHeightNoisePerColumn[CHUNK_BLOCKS_PER_LAYER] = {};
		int terrainHeightPerColumn[CHUNK_BLOCKS_PER_LAYER] = {};
		float oceanessPerColumn[CHUNK_BLOCKS_PER_LAYER] = {};
		float cloudnessPerColumn[CHUNK_BLOCKS_PER_LAYER] = {};

		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				IntVec2 globalCoords = IntVec2(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y) + IntVec2(x, y);
				temperaturePerColumn[columnIndex] = RangeMapClamped(Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 800.f, 8, 0.5f, 2.f, true, m_world->m_worldSeed + 1), -1.f, 1.f, 0.f, 1.f);
				humidityPerColumn[columnIndex] = RangeMapClamped(Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 500.f, 2, 0.5f, 2.f, true, m_world->m_worldSeed + 2), -1.f, 1.f, 0.f, 1.f);
				terrainHeightNoisePerColumn[columnIndex] = Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 200.f, 3, 0.5f, 2.f, true, m_world->m_worldSeed);
				oceanessPerColumn[columnIndex] = Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 500.f, 2, 0.5f, 2.f, true, m_world->m_worldSeed + 6);
				cloudnessPerColumn[columnIndex] = RangeMapClamped(Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 20.f, 7, 0.5f, 2.f, true, m_world->m_worldSeed + 7), -1.f, 1.f, 0.f, 1.f);

				float hilliness = Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 800.f, 2, 0.5f, 2.f, true, m_world->m_worldSeed + 3);
				hilliness = RangeMapClamped(hilliness, -1.f, 1.f, 0.f, 1.f);
				float hillinessWithTerrainNoise = SmoothStep3(hilliness * fabsf(terrainHeightNoisePerColumn[columnIndex]));
				terrainHeightPerColumn[columnIndex] = int(RangeMapClamped(hillinessWithTerrainNoise, 0.f, 1.f, 63.f, CHUNK_SIZE_Z));
			}
		}

		//set block type according to simple original algorithm
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				int numDirtBlocks = rng.GetRandomIntInRange(3, 4);
				IntVec2 globalCoords = IntVec2(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y) + IntVec2(x, y);
				int terrainHeight = terrainHeightPerColumn[columnIndex];
				float oceaness = oceanessPerColumn[columnIndex];
				if (oceaness > 0.5f)
					terrainHeight = MAX_OCEAN_DEPTH;
				else if (oceaness > 0.f && oceaness < 0.5f)
				{
					terrainHeight = Interpolate(terrainHeight, MAX_OCEAN_DEPTH, oceaness);
				}
				terrainHeightPerColumn[columnIndex] = terrainHeight;
				//int terrainHeight = 64 + (30.f * Compute2dPerlinNoise(float(globalCoords.x), float(globalCoords.y), 200.f, 5, 0.5f, 2.f, true, m_world->m_worldSeed));
				for (int z = 0; z < CHUNK_SIZE_Z; z++)
				{
					std::string blockTypeName;
					int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));

					if (z > terrainHeight)
					{
						blockTypeName = "air";
						if (z <= SEA_LEVEL)
							blockTypeName = "water";
					}
					else if (z == terrainHeight)
					{
						blockTypeName = "grass";
					}
					else if (z < terrainHeight && z >= terrainHeight - numDirtBlocks)
						blockTypeName = "dirt";
					else
					{
						float randPercent = rng.GetRandomFloatInRange(0.f, 100.f);
						if (randPercent < 0.1f)
							blockTypeName = "diamond";
						else if (randPercent < 0.5f)
							blockTypeName = "gold";
						else if (randPercent < 2.f)
							blockTypeName = "iron";
						else if (randPercent < 5.f)
							blockTypeName = "coal";
						else
							blockTypeName = "stone";
					}

					m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName(blockTypeName);
				}
			}
		}

		//replace grass and dirt with sand if humidity is low
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				int sandBlocks = int(RangeMapClamped(humidityPerColumn[columnIndex], 0.f, 0.4f, float(MAX_SAND_BLOCKS), 0.f));

				for (int i = 0; i < sandBlocks; i++)
				{
					int z = terrainHeightPerColumn[columnIndex];
					int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
					m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("sand");
					z--;
				}
			}
		}

		//replace water with ice if temperature is low
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				int iceBlocks = int(RangeMapClamped(temperaturePerColumn[columnIndex], 0.f, 0.4f, float(MAX_ICE_BLOCKS), 0.f));
				int z = CHUNK_MAX_Z;
				while (z >= 0 && iceBlocks > 0)
				{
					int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
					if (m_blocks[blockIndex].m_typeIndex == BlockDefintion::GetDefinitionIndexByName("water"))
					{
						m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("ice");
						iceBlocks--;
					}
					z--;
				}
			}
		}

		//add snow on high mountains where temperatures are low
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				int terrainHeight = terrainHeightPerColumn[columnIndex];
				if (terrainHeight >= FREEZING_LEVEL)
				{
					int z = FREEZING_LEVEL;
					while (z <= terrainHeight)
					{
						int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
						if (z == FREEZING_LEVEL)
							m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("snowgrass");
						else
							m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("ice");
						z++;
					}
				}
			}
		}


		//add beaches (replace grass with sand for low humidity)
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				float humidity = humidityPerColumn[columnIndex];
				if (humidity < 0.65f)
				{
					int blockIndexAtSeaLevel = GetBlockIndexFromLocalCoords(IntVec3(x, y, SEA_LEVEL));
					if (m_blocks[blockIndexAtSeaLevel].m_typeIndex == BlockDefintion::GetDefinitionIndexByName("grass"))
					{
						m_blocks[blockIndexAtSeaLevel].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("sand");
					}
				}
			}
		}

		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				float cloudness = cloudnessPerColumn[columnIndex];
				if (cloudness > 0.7f)
				{
					int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, CLOUD_LEVEL));
					m_blocks[blockIndex].m_typeIndex = BlockDefintion::GetDefinitionIndexByName("cloud");
				}
			}
		}

		//add trees originating in current chunk
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				int columnIndex = x + (y * CHUNK_SIZE_X);
				int blockIndexAboveTerrainHeight = GetBlockIndexFromLocalCoords(IntVec3(x, y, terrainHeightPerColumn[columnIndex] + 1));
				//only non water tiles have a non air tile at the terrain height
				if (m_blocks[blockIndexAboveTerrainHeight].m_typeIndex == BlockDefintion::GetDefinitionIndexByName("air"))
				{
					IntVec2 globalCoords = IntVec2(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y) + IntVec2(x, y);
					bool makeTree = IsLocalMaximaIn5x5(/*treeNoisePerColumn[columnIndex]*/0.f, globalCoords);
					if (makeTree)
					{
						int z = terrainHeightPerColumn[columnIndex] + 1;
						AddBlocksForTree("tree", IntVec3(x, y, z));
					}
				}
			}
		}
	}
}

bool Chunk::ShouldRebuildMesh() const
{
	return m_isChunkDirty && HasAllValidNeighbours();
}

void Chunk::InitializeLighting()
{
	//mark non opaque blocks at the edge of a chunk as dirty
	for (int i = 0; i < CHUNK_TOTAL_BLOCKS; i++)
	{
		Block& block = m_blocks[i];
		if (!BlockDefintion::IsBlockTypeOpaque(block.m_typeIndex))
		{
			IntVec3 localCoords = GetLocalCoordsFromBlockIndex(i);
			if (localCoords.x == 0 || localCoords.x == CHUNK_MAX_X || 
				localCoords.y == 0 || localCoords.y == CHUNK_MAX_Y ||
				localCoords.z == 0 || localCoords.z == CHUNK_MAX_Z)
			{
				BlockIterator iter = { this, i };
				m_world->MarkLightingDirty(iter);
			}
		}
	}

	//make blocks as sky which are in direct line of sight of the sky
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int x = 0; x < CHUNK_SIZE_X; x++)
		{
			int z = CHUNK_MAX_Z;
			int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
			Block* block = &m_blocks[blockIndex];
			while (!BlockDefintion::IsBlockTypeOpaque(block->m_typeIndex))
			{
				block->SetIsBlockSky(true);
				z--;
				blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
				block = &m_blocks[blockIndex];
			}
		}
	}

	//set each sky blocks outdoor lighting influence to max and dirty it's neighbouring blocks
	for (int y = 0; y < CHUNK_SIZE_Y; y++)
	{
		for (int x = 0; x < CHUNK_SIZE_X; x++)
		{
			int z = CHUNK_MAX_Z;
			int blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
			Block* block = &m_blocks[blockIndex];
			while (!BlockDefintion::IsBlockTypeOpaque(block->m_typeIndex))
			{
				block->SetOutdoorLightInfluence(15);

				BlockIterator blockIter = { this, blockIndex };
				m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetNorthNeighbour());
				m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetEastNeighbour());
				m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetSouthNeighbour());
				m_world->MarkLightingDirtyIfNotSkyAndNotOpaque(blockIter.GetWestNeighbour());

				z--;
				blockIndex = GetBlockIndexFromLocalCoords(IntVec3(x, y, z));
				block = &m_blocks[blockIndex];
			}
		}
	}

	for (int i = 0; i < CHUNK_TOTAL_BLOCKS; i++)
	{
		Block& block = m_blocks[i];
		if (BlockDefintion::DoesBlockTypeEmitLight(block.m_typeIndex))
		{
			BlockIterator iter = { this, i };
			m_world->MarkLightingDirty(iter);
		}
	}

}

void Chunk::GenerateGeometry()
{
	m_cpuMeshOpaqueVertices.clear();
	m_cpuMeshOpaqueIndicies.clear();
	m_cpuMeshTranslucentVertices.clear();
	m_cpuMeshTranslucentIndicies.clear();
	double startTime = GetCurrentTimeSeconds();
	for (int z = 0; z < CHUNK_SIZE_Z; z++)
	{
		for (int y = 0; y < CHUNK_SIZE_Y; y++)
		{
			for (int x = 0; x < CHUNK_SIZE_X; x++)
			{
				IntVec3 localCoords(x, y, z);
				int index = GetBlockIndexFromLocalCoords(localCoords);
				BlockDefintion& blockDef = BlockDefintion::s_definitions[m_blocks[index].m_typeIndex];
				if (blockDef.m_name != "water")
					AddVertsForBlock(blockDef, localCoords);
				else
					AddVertsForWaterBlock(blockDef, localCoords);
			}
		}
	}
	double endTime = GetCurrentTimeSeconds();
	g_theConsole->AddLine(g_theConsole->INFO_MAJOR, Stringf("Chunk [%d, %d] took %f time to build its cpu mesh", m_chunkCoords.x, m_chunkCoords.y, (endTime - startTime) * 1000.f));

	size_t sizeOfOpaqueVBO = sizeof(*m_cpuMeshOpaqueVertices.data()) * m_cpuMeshOpaqueVertices.size();
	size_t sizeOfOpaqueIBO = sizeof(*m_cpuMeshOpaqueIndicies.data()) * m_cpuMeshOpaqueIndicies.size();
	size_t sizeOfTranslucentVBO = sizeof(*m_cpuMeshTranslucentVertices.data()) * m_cpuMeshTranslucentVertices.size();
	size_t sizeOfTranslucentIBO = sizeof(*m_cpuMeshTranslucentIndicies.data()) * m_cpuMeshTranslucentIndicies.size();
	if (!m_gpuMeshOpaqueVBO)
	{
		m_gpuMeshOpaqueVBO = g_theRenderer->CreateVertexBuffer(sizeOfOpaqueVBO, sizeof(m_cpuMeshOpaqueVertices[0]));
	}
	if (!m_gpuMeshOpaqueIBO)
	{
		m_gpuMeshOpaqueIBO = g_theRenderer->CreateIndexBuffer(sizeOfOpaqueIBO);
	}
	if (!m_gpuMeshTranslucentVBO && sizeOfTranslucentVBO > 0)
	{
		m_gpuMeshTranslucentVBO = g_theRenderer->CreateVertexBuffer(sizeOfTranslucentVBO, sizeof(m_cpuMeshTranslucentVertices[0]));
	}
	if (!m_gpuMeshTranslucentIBO && sizeOfTranslucentIBO > 0)
	{
		m_gpuMeshTranslucentIBO = g_theRenderer->CreateIndexBuffer(sizeOfTranslucentIBO);
	}

	g_theRenderer->CopyCPUToGPU(m_cpuMeshOpaqueVertices.data(), sizeOfOpaqueVBO, m_gpuMeshOpaqueVBO);
	g_theRenderer->CopyCPUToGPU(m_cpuMeshOpaqueIndicies.data(), sizeOfOpaqueIBO, m_gpuMeshOpaqueIBO);
	if(m_gpuMeshTranslucentVBO)
		g_theRenderer->CopyCPUToGPU(m_cpuMeshTranslucentVertices.data(), sizeOfTranslucentVBO, m_gpuMeshTranslucentVBO);
	if(m_gpuMeshTranslucentIBO)
		g_theRenderer->CopyCPUToGPU(m_cpuMeshTranslucentIndicies.data(), sizeOfTranslucentIBO, m_gpuMeshTranslucentIBO);
	m_world->AddToTotalNumberOfVerticesInChunks((int)m_cpuMeshOpaqueVertices.size());
	m_world->AddToTotalNumberOfVerticesInChunks((int)m_cpuMeshTranslucentVertices.size());
	m_isChunkDirty = false;
}

void Chunk::AddVertsForBlock(const BlockDefintion& blockDef, const IntVec3& localCoords)
{
	if (blockDef.m_visible)
	{
		Vec3 mins = m_worldBounds.m_mins + Vec3(localCoords);
		AABB3 bounds = AABB3(mins, mins + Vec3::ONE);

		Vec3 near_topLeft(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);
		Vec3 near_bottomLeft(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);
		Vec3 near_bottomRight(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
		Vec3 near_topRight(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
		Vec3 far_topLeft(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);
		Vec3 far_bottomLeft(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);
		Vec3 far_bottomRight(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
		Vec3 far_topRight(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);

		BlockIterator blockIter = { this, GetBlockIndexFromLocalCoords(localCoords) };
		uint8_t currentDugState = blockIter.GetBlock()->GetCurrentDugState();
		//bottom face
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetBelowNeighbour().GetBlock()->m_typeIndex))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetBelowNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_bottomLeft, far_bottomLeft, far_bottomRight, near_bottomRight, tileColor, blockDef.m_bottomUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(0.f, 0.f, -0.01f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_bottomLeft + offset, far_bottomLeft + offset,
					far_bottomRight + offset, near_bottomRight + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
		//top face
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetAboveNeighbour().GetBlock()->m_typeIndex))
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetAboveNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topLeft, near_topLeft, near_topRight, far_topRight, tileColor, blockDef.m_topUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(0.f, 0.f, 0.01f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topLeft + offset, near_topLeft + offset,
					near_topRight + offset, far_topRight + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
		//side faces
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetWestNeighbour().GetBlock()->m_typeIndex))	//-x face
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetWestNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_topLeft, near_bottomLeft, near_bottomRight, near_topRight, tileColor, blockDef.m_sideUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(-0.01f, 0.f, 0.f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_topLeft + offset, near_bottomLeft + offset,
					near_bottomRight + offset, near_topRight + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetSouthNeighbour().GetBlock()->m_typeIndex))	//-y face
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetSouthNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_topRight, near_bottomRight, far_bottomRight, far_topRight, tileColor, blockDef.m_sideUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(0.f, -0.01f, 0.f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, near_topRight + offset, near_bottomRight + offset,
					far_bottomRight + offset, far_topRight + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetEastNeighbour().GetBlock()->m_typeIndex))		//x face
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetEastNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topRight, far_bottomRight, far_bottomLeft, far_topLeft, tileColor, blockDef.m_sideUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(0.01f, 0.f, 0.f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topRight + offset, far_bottomRight + offset,
					far_bottomLeft + offset, far_topLeft + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
		if (!BlockDefintion::IsBlockTypeOpaque(blockIter.GetNorthNeighbour().GetBlock()->m_typeIndex))		//y face
		{
			Rgba8 tileColor = GetFaceColor(blockIter.GetNorthNeighbour());
			AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topLeft, far_bottomLeft, near_bottomLeft, near_topLeft, tileColor, blockDef.m_sideUVs);
			if (currentDugState > 0)
			{
				Vec3 offset = Vec3(0.f, 0.01f, 0.f);
				AddVertsForQuad3D(m_cpuMeshOpaqueVertices, m_cpuMeshOpaqueIndicies, far_topLeft + offset, far_bottomLeft + offset,
					near_bottomLeft + offset, near_topLeft + offset, tileColor, BlockDefintion::s_digCrackUVs[currentDugState - 1]);
			}
		}
	}
}

void Chunk::AddVertsForWaterBlock(const BlockDefintion& blockDef, const IntVec3& localCoords)
{
	if (blockDef.m_visible)
	{
		Vec3 mins = m_worldBounds.m_mins + Vec3(localCoords);
		AABB3 bounds = AABB3(mins, mins + Vec3::ONE);

		Vec3 near_topLeft(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_maxs.z);
		Vec3 near_bottomLeft(bounds.m_mins.x, bounds.m_maxs.y, bounds.m_mins.z);
		Vec3 near_bottomRight(bounds.m_mins.x, bounds.m_mins.y, bounds.m_mins.z);
		Vec3 near_topRight(bounds.m_mins.x, bounds.m_mins.y, bounds.m_maxs.z);
		Vec3 far_topLeft(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_maxs.z);
		Vec3 far_bottomLeft(bounds.m_maxs.x, bounds.m_maxs.y, bounds.m_mins.z);
		Vec3 far_bottomRight(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_mins.z);
		Vec3 far_topRight(bounds.m_maxs.x, bounds.m_mins.y, bounds.m_maxs.z);

		BlockIterator blockIter = { this, GetBlockIndexFromLocalCoords(localCoords) };

		//top face
		Rgba8 tileColor = GetFaceColor(blockIter.GetAboveNeighbour());
		//set face alpha to half
		tileColor.a = 127;
		//if this is the top most water block, set its blue channel to max to do vertex animations
		if (localCoords.z == SEA_LEVEL)
			tileColor.b = 255;

		AddVertsForQuad3D(m_cpuMeshTranslucentVertices, m_cpuMeshTranslucentIndicies, far_topLeft, near_topLeft, near_topRight, far_topRight, tileColor, blockDef.m_topUVs);
	}
}

bool Chunk::HasAllValidNeighbours() const
{
	return m_northNeighbour && m_eastNeighbour && m_southNeighbour && m_westNeighbour;
}

bool Chunk::LoadBlocksFromFile()
{
	std::string filePath = Stringf("Saves/Chunk(%d,%d).chunk", m_chunkCoords.x, m_chunkCoords.y);
	if (DoesFileExist(filePath))
	{
		std::vector<uint8_t> buffer;
		FileReadToBuffer(buffer, filePath);
		//check if file signature matches what we expect it to
		if (buffer[0] == 'G' && buffer[1] == 'C' && buffer[2] == 'H' && buffer[3] == 'K' &&
			buffer[4] == 1 && buffer[5] == CHUNK_BITS_X && buffer[6] == CHUNK_BITS_Y && buffer[7] == CHUNK_BITS_Z)
		{
			int blockIndex = 0;
			for (int i = 8; i < buffer.size(); i += 2)
			{
				uint8_t blockTypeIndex =  static_cast<int>(buffer[i]);
				int numberOfBlocks = static_cast<int>(buffer[i + 1]);
				for (int j = 0; j < numberOfBlocks; j++)
				{
					m_blocks[blockIndex].m_typeIndex = blockTypeIndex;
					blockIndex++;
				}
			}
			//GUARANTEE_OR_DIE(blockIndex == CHUNK_TOTAL_BLOCKS, "Total blocks wrong");
			return true;
		}
		else
		{
			ERROR_AND_DIE(Stringf("Loaded file signature didnt not match for chunk (%d, %d)", m_chunkCoords.x, m_chunkCoords.y));
			return false;
		}
	}

	return false;
}

void Chunk::SaveBlockToFile()
{
	std::vector<uint8_t> buffer;
	buffer.reserve(CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Y);
	//write file signature
	buffer.push_back('G');
	buffer.push_back('C');
	buffer.push_back('H');
	buffer.push_back('K');
	buffer.push_back(1);
	buffer.push_back(CHUNK_BITS_X);
	buffer.push_back(CHUNK_BITS_Y);
	buffer.push_back(CHUNK_BITS_Z);

	int totalBlockwritten = 0;
	//write rest of the block data using run length encoding
	for (int i = 0; i < CHUNK_TOTAL_BLOCKS - 1; )
	{
		uint8_t currentBlockType = m_blocks[i].m_typeIndex;
		uint8_t numberOfBlockOfSameTypeTogether = 0;
		while (m_blocks[i + numberOfBlockOfSameTypeTogether].m_typeIndex == currentBlockType && 
			numberOfBlockOfSameTypeTogether < 255 && 
			(i + numberOfBlockOfSameTypeTogether) < (CHUNK_TOTAL_BLOCKS - 1))
		{
			numberOfBlockOfSameTypeTogether++;
		}

		buffer.push_back(currentBlockType);
		buffer.push_back(numberOfBlockOfSameTypeTogether);
		i += numberOfBlockOfSameTypeTogether;
		totalBlockwritten = i;
	}

	std::string filePath = Stringf("Saves/Chunk(%d,%d).chunk", m_chunkCoords.x, m_chunkCoords.y);
	BufferWriteToFile(buffer, filePath);
}

Rgba8 Chunk::GetFaceColor(const BlockIterator& blockIterator)
{
	Rgba8 color;
	Block& block = *blockIterator.GetBlock();
	color.r = unsigned char(RangeMap(block.GetOutdoorLightInfluence(), 0.f, 15.f, 0.f, 255.f));
	color.g = unsigned char(RangeMap(block.GetIndoorLightInfluence(), 0.f, 15.f, 0.f, 255.f));
	color.b = 0;
	color.a = 255;
	return color;
}

void Chunk::ProcessLightingForDugBlock(const BlockIterator& blockIter)
{
	m_world->MarkLightingDirty(blockIter);

	IntVec3 localCoords = GetLocalCoordsFromBlockIndex(blockIter.m_blockIndex);
	bool isSkyBlock = blockIter.GetAboveNeighbour().GetBlock()->IsBlockSky();
	if (isSkyBlock)
	{
		BlockIterator neighbour = blockIter;
		while (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
		{
			neighbour.GetBlock()->SetIsBlockSky(true);
			m_world->MarkLightingDirty(neighbour);
			neighbour = neighbour.GetBelowNeighbour();
		}
	}
}

void Chunk::ProcessLightingForAddedBlock(const BlockIterator& blockIter)
{
	Block* block = blockIter.GetBlock();
	m_world->MarkLightingDirty(blockIter);
	if (block->IsBlockSky() && BlockDefintion::IsBlockTypeOpaque(block->m_typeIndex))
	{
		block->SetIsBlockSky(false);
		BlockIterator neighbour = blockIter.GetBelowNeighbour();
		while (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
		{
			neighbour.GetBlock()->SetIsBlockSky(false);
			m_world->MarkLightingDirty(neighbour);
			neighbour = neighbour.GetBelowNeighbour();
		}
	}
}

bool Chunk::IsLocalMaximaIn5x5(float refTreeNoise, IntVec2 globalCoordsXY)
{
	IntVec2 startCoords = IntVec2(globalCoordsXY) - IntVec2(2, 2);
	float refTreeDensity = Compute2dPerlinNoise(float(globalCoordsXY.x), float(globalCoordsXY.y), 50.f, 3, 0.5f, 2.f, true, m_world->m_worldSeed + 4);
	refTreeNoise = Compute2dPerlinNoise(float(globalCoordsXY.x), float(globalCoordsXY.y), 800.f, 4, refTreeDensity, 2.f, true, m_world->m_worldSeed + 5);
	for (int y = 0; y < 5; y++)
	{
		for (int x = 0; x < 5; x++)
		{
			IntVec2 coords = startCoords + IntVec2(x, y);
			//use the same perlin noise arguments that were used to generate the tree noise and tree density for the reference column
			float columnTreeDensity = Compute2dPerlinNoise(float(coords.x), float(coords.y), 50.f, 3, 0.5f, 2.f, true, m_world->m_worldSeed + 4);
			float columnTreeNoise = Compute2dPerlinNoise(float(coords.x), float(coords.y), 800.f, 4, columnTreeDensity, 2.f, true, m_world->m_worldSeed + 5);
			if (columnTreeNoise > refTreeNoise)
				return false;
		}
	}

	return true;
}

void Chunk::AddBlocksForTree(const std::string& treeName, const IntVec3& baseCoords)
{
	BlockTemplate treeTemplate = BlockTemplate::GetTemplateFromName(treeName);
	int baseBlockIndex = GetBlockIndexFromLocalCoords(baseCoords);
	for (int i = 0; i < treeTemplate.m_blockTemplateEntries.size(); i++)
	{
		//reset to base blockiter for every block to be placed
		BlockIterator blockIter = { this, baseBlockIndex };
		IntVec3 blockOffset = treeTemplate.m_blockTemplateEntries[i].m_relativeOffset;
		for (int x = 0; x < abs(blockOffset.x); x++)
		{
			if (blockOffset.x < 0)
				blockIter = blockIter.GetEastNeighbour();
			else
				blockIter = blockIter.GetWestNeighbour();
		}

		for (int y = 0; y < abs(blockOffset.y); y++)
		{
			if (blockOffset.y < 0)
				blockIter = blockIter.GetSouthNeighbour();
			else
				blockIter = blockIter.GetNorthNeighbour();
		}

		for (int z = 0; z < abs(blockOffset.z); z++)
		{
			if (blockOffset.z < 0)
				blockIter = blockIter.GetBelowNeighbour();
			else
				blockIter = blockIter.GetAboveNeighbour();
		}

		if(blockIter.GetBlock())
			blockIter.GetBlock()->m_typeIndex = treeTemplate.m_blockTemplateEntries[i].m_blockTypeIndex;
	}
}

ChunkGenerationJob::ChunkGenerationJob(Chunk* chunk)
	:m_chunk(chunk)
{
}

void ChunkGenerationJob::Execute()
{
	m_chunk->m_status = ACTIVATING_GENERATING;
	m_chunk->InitializeBlocks();
}

void ChunkGenerationJob::OnFinished()
{
	m_chunk->m_status = ACTIVATING_GENERATE_COMPLETE;
}
