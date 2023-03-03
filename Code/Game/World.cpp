#include <algorithm>
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/ConstantBuffer.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "ThirdParty/Squirrel/SmoothNoise.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Game/World.hpp"
#include "Game/Chunk.hpp"
#include "Game/Game.hpp"
#include "Game/Player.hpp"
#include "Game/App.hpp"
#include "Game/Entity.hpp"
#include "Game/GameCamera.hpp"

extern Renderer* g_theRenderer;
extern InputSystem* g_theInput;
extern App* g_theApp;
extern DevConsole* g_theConsole;
extern JobSystem* g_theJobSystem;

constexpr int gameConstantsSlotNumber = 4;
constexpr float raycastDistance = 8.f;

struct GameConstants
{
	Vec4 cameraWorldPos;
	float indoorLightColor[4];
	float outdoorLightColor[4];
	float skyColor[4];
	float fogStartDistance;
	float fogEndDistance;
	float fogMaxAlpha;
	float worldTime;
};

World::World(Game* game)
	:m_game(game)
{
	BlockDefintion::CreateAllDefintions();
	BlockTemplate::InitializeTemplates("Data/Definitions/BlockTemplates.xml");
	m_debugStepLighting = g_gameConfigBlackboard.GetValue("debugStepLighting", m_debugStepLighting);
	bool useDefaultShader = g_gameConfigBlackboard.GetValue("debugUseDefaultShader", false);
	if (useDefaultShader)
		m_shader = g_theRenderer->CreateOrGetShader("Default");
	else
	{
		std::string shaderName = g_gameConfigBlackboard.GetValue("worldShaderName", "Default");
		m_shader = g_theRenderer->CreateOrGetShader(shaderName.c_str());
	}
	m_indoorLightColor = g_gameConfigBlackboard.GetValue("indoorLightColor", Rgba8::WHITE);
	m_dayOutdoorLightColor = g_gameConfigBlackboard.GetValue("dayOutdoorLightColor", Rgba8::WHITE);
	m_nightOutdoorLightColor = g_gameConfigBlackboard.GetValue("nightOutdoorLightColor", Rgba8::WHITE);
	m_daySkyColor = g_gameConfigBlackboard.GetValue("daySkyColor", Rgba8::WHITE);
	m_nightSkyColor = g_gameConfigBlackboard.GetValue("nightSkyColor", Rgba8::WHITE);
	m_worldTimeScale = g_gameConfigBlackboard.GetValue("worldTimeScale", m_worldTimeScale);
	m_fogStartDistance = g_gameConfigBlackboard.GetValue("fogStart", m_fogStartDistance);
	m_fogEndDistance = g_gameConfigBlackboard.GetValue("fogEnd", m_fogEndDistance);
	m_fogMaxAlpha = g_gameConfigBlackboard.GetValue("fogMaxAlpha", m_fogMaxAlpha);
	m_worldSeed = g_gameConfigBlackboard.GetValue("worldSeed", m_worldSeed);

	m_chunkActivationRange = g_gameConfigBlackboard.GetValue("chunkActivationRange", m_chunkActivationRange);
	m_chunkDeactivationRange = m_chunkActivationRange + CHUNK_SIZE_X + CHUNK_SIZE_Y;
	m_maxChunkRadiusX = 1 + int(m_chunkActivationRange) / CHUNK_SIZE_X;
	m_maxChunkRadiusY = 1 + int(m_chunkActivationRange) / CHUNK_SIZE_Y;
	m_maxChunks = (2 * m_maxChunkRadiusX) * (2 * m_maxChunkRadiusY);

	m_gameCBO = g_theRenderer->CreateConstantBuffer(sizeof(GameConstants));

	//spawn player
	m_player = new Entity(this, Vec3(8.f, 8.f, 90.f));
	m_playerWorldCamera = new GameCamera(this, m_player);
	m_player->m_gameCamera = m_playerWorldCamera;
	Player* m_playerController = new Player(this);
	m_playerController->Possess(m_player);
	m_player->m_controller = m_playerController;
	m_allEntities.push_back(m_player);
}

World::~World()
{
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		delete iter->second;
	}
	m_activeChunks.clear();

	delete m_gameCBO;
	m_gameCBO = nullptr;

	delete m_playerWorldCamera;
	m_playerWorldCamera = nullptr;
}

void World::Update(float deltaSeconds)
{
	UpdateDayCycle(deltaSeconds);
	HandleDebugInput();

	InstantiateChunk();
	bool chunkActivated = ActivateChunk();
	if (!chunkActivated)
	{
		DeactivateChunk();
	}

	static int counter = 0;
	double start = GetCurrentTimeSeconds();
	ProcessDirtyLighting();
	double end = GetCurrentTimeSeconds();
	if(counter < 20)
		g_theConsole->AddLine(g_theConsole->INFO_MAJOR, Stringf("Time taken to process dirty light each frame = %f",(end - start) * 1000.f));
	counter++;

	PerformRaycast();
	UpdateChunks(deltaSeconds);
	UpdateEntities(deltaSeconds);
	UpdateCameras(deltaSeconds);
}

void World::Render() const
{
	g_theRenderer->BeginCamera(m_playerWorldCamera->GetWorldCamera());
	{
		g_theRenderer->ClearScreen(m_currentSkyColor);
		//set pipeline state for all chunks
		CopyDataToCBOAndBindIt();
		g_theRenderer->SetBlendMode(BlendMode::ALPHA);
		g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
		g_theRenderer->SetSamplerMode(SamplerMode::POINTCLAMP);
		g_theRenderer->SetDepthStencilState(DepthTest::LESSEQUAL, true);
		g_theRenderer->BindTexture(BlockDefintion::s_blockSpriteTexture);
		g_theRenderer->SetModelColor(Rgba8::WHITE);
		g_theRenderer->SetModelMatrix(Mat44());
		g_theRenderer->BindShader(m_shader);
		RenderChunks();
		RenderEntities();
		RenderRaycastImpact();

		if (m_game->m_enableDebugDrawing)
		{
			std::vector<Vertex_PCU> verts;

			AddDebugVertsForLighting(verts);
			g_theRenderer->BindTexture(nullptr);
			g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());
			g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());

			verts.clear();
			AddDebugVertsForChunk(verts);
			g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::WIREFRAME, WindingOrder::COUNTERCLOCKWISE);
			g_theRenderer->BindTexture(nullptr);
			g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());
		}
	}
	g_theRenderer->EndCamera(m_playerWorldCamera->GetWorldCamera());
}

void World::AddToTotalNumberOfVerticesInChunks(int verts)
{
	m_totalChunkMeshVertices += verts;
}

void World::DigBlock()
{
	if (m_raycastResult.m_didImpact)
	{
		m_raycastResult.m_blockImpacted.m_chunkBlockBelongsTo->DigBlock(m_raycastResult.m_blockImpacted);
	}
}

void World::AddBlock()
{
	if (m_placeBlockAtCurrentPos)
	{
		Vec3 cameraPos = m_player->m_position;
		IntVec2 chunkCoords = Chunk::GetChunkCoordinatedForWorldPosition(cameraPos);
		std::map<IntVec2, Chunk*>::const_iterator iter = m_activeChunks.find(chunkCoords);
		if (iter != m_activeChunks.end())
		{
			Chunk* currentChunk = iter->second;
			AABB3 m_worldBounds = currentChunk->GetChunkWorldBounds();
			IntVec2 columnXY = IntVec2(int(cameraPos.x - m_worldBounds.m_mins.x), int(cameraPos.y - m_worldBounds.m_mins.y));
			int blockIndex = currentChunk->GetBlockIndexFromLocalCoords(IntVec3(columnXY.x, columnXY.y, (int)cameraPos.z));
			currentChunk->AddBlock({ currentChunk, blockIndex });
		}
	}
	else if (m_raycastResult.m_didImpact)
	{
		BlockIterator blockToAdd;
		if (m_raycastResult.m_impactNormal.x > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetEastNeighbour();
		else if (m_raycastResult.m_impactNormal.x < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetWestNeighbour();
		else if (m_raycastResult.m_impactNormal.y > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetNorthNeighbour();
		else if (m_raycastResult.m_impactNormal.y < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetSouthNeighbour();
		else if (m_raycastResult.m_impactNormal.z > 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetAboveNeighbour();
		else if (m_raycastResult.m_impactNormal.z < 0.f)
			blockToAdd = m_raycastResult.m_blockImpacted.GetBelowNeighbour();

		blockToAdd.m_chunkBlockBelongsTo->AddBlock(blockToAdd);
	}
}

void World::HandleDebugInput()
{
	if (g_theInput->WasKeyJustPressed('H'))
		m_placeBlockAtCurrentPos = !m_placeBlockAtCurrentPos;
	if (g_theInput->WasKeyJustPressed('L'))
		m_performNextLightingStep = true;
	if (g_theInput->WasKeyJustPressed('R'))
		m_freezeRaycastStart = !m_freezeRaycastStart;

	if (g_theInput->IsKeyDown('Y'))
		m_currentWorldTimeScaleAccelerationFactor = 50.f;
	else
		m_currentWorldTimeScaleAccelerationFactor = 1.f;

	if (g_theInput->WasKeyJustPressed('1'))
		m_blockTypeToAdd = 1;
	if (g_theInput->WasKeyJustPressed('2'))
		m_blockTypeToAdd = 2;
	if (g_theInput->WasKeyJustPressed('3'))
		m_blockTypeToAdd = 3;
	if (g_theInput->WasKeyJustPressed('4'))
		m_blockTypeToAdd = 4;
	if (g_theInput->WasKeyJustPressed('5'))
		m_blockTypeToAdd = 5;
	if (g_theInput->WasKeyJustPressed('6'))
		m_blockTypeToAdd = 6;
	if (g_theInput->WasKeyJustPressed('7'))
		m_blockTypeToAdd = 7;
	if (g_theInput->WasKeyJustPressed('8'))
		m_blockTypeToAdd = 8;
	if (g_theInput->WasKeyJustPressed('9'))
		m_blockTypeToAdd = 9;

	if (g_theInput->WasKeyJustPressed(KEYCODE_F2))
	{
		m_playerWorldCamera->CycleToNextMode();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F3))
	{
		m_player->CyclePhysicsMode();
	}
}

void World::AddDebugVertsForChunk(std::vector<Vertex_PCU>& verts) const
{
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		AABB3 bounds = iter->second->GetChunkWorldBounds();
		AddVertsForAABB3D(verts, bounds);
	}
}

void World::AddDebugVertsForLighting(std::vector<Vertex_PCU>& verts) const
{
	constexpr float sideHalfLength = 0.05f;
	for (auto iter = m_dirtyLightBlocks.begin(); iter != m_dirtyLightBlocks.end(); ++iter)
	{
		Vec3 center = iter->GetWorldCenter();
		Vec3 translateCenterVector = Vec3(sideHalfLength, sideHalfLength, sideHalfLength);
		AABB3 bounds = AABB3(center - translateCenterVector, center + translateCenterVector);
		AddVertsForAABB3D(verts, bounds, Rgba8::YELLOW);
	}
}

void World::InstantiateChunk()
{
	if (m_activeChunks.size() >= m_maxChunks)
		return;

	Vec3 cameraPos = m_player->m_position;
	IntVec2 currentChunkCoords = Chunk::GetChunkCoordinatedForWorldPosition(cameraPos);
	bool instantiateChunk = false;
	IntVec2 coordsOfChunkToActivate = IntVec2::ZERO;
	float shortestSquaredDistance = 999999;
	IntVec2 startCoords = IntVec2(currentChunkCoords.x - m_maxChunkRadiusX, currentChunkCoords.y - m_maxChunkRadiusY);
	for (int y = 0; y < 2 * m_maxChunkRadiusY; y++)
	{
		for (int x = 0; x < 2 * m_maxChunkRadiusX; x++)
		{
			IntVec2 chunkCoord = startCoords + IntVec2(x, y);
			Vec2 cameraPosXY(cameraPos.x, cameraPos.y);
			Vec2 chunkCenterPositon = Chunk::GetChunkCenterXYForGlobalChunkCoords(chunkCoord);
			if (GetDistanceSquared2D(cameraPosXY, chunkCenterPositon) < m_chunkActivationRange * m_chunkActivationRange)
			{
				std::map<IntVec2, Chunk*>::const_iterator iter = m_chunksQueuedForGeneration.find(chunkCoord);
				if (iter == m_chunksQueuedForGeneration.end())
				{
					float sqDistance = GetDistanceSquared2D(cameraPosXY, chunkCenterPositon);
					if (sqDistance < shortestSquaredDistance)
					{
						coordsOfChunkToActivate = chunkCoord;
						shortestSquaredDistance = sqDistance;
						instantiateChunk = true;
					}
				}
			}
		}
	}

	//instantiate the chunk and then create a generation job and queue it up
	if (instantiateChunk)
	{
		Chunk* newChunk = new Chunk(this, coordsOfChunkToActivate);
		ChunkGenerationJob* job = new ChunkGenerationJob(newChunk);
		g_theJobSystem->QueueJobs(job);
		newChunk->m_status = ACTIVATING_QUEUED_GENERATE;
		m_chunksQueuedForGeneration[coordsOfChunkToActivate] = newChunk;
	}
}

bool World::ActivateChunk()
{
	ChunkGenerationJob* finishedGenerationJob = dynamic_cast<ChunkGenerationJob*>(g_theJobSystem->RetrieveFinishedJob());
	if (finishedGenerationJob)
	{
		Chunk* chunk = finishedGenerationJob->m_chunk;
		IntVec2 chunkCoords = chunk->GetChunkCoordinates();
		m_activeChunks[chunkCoords] = chunk;

		std::map<IntVec2, Chunk*>::const_iterator iter = m_activeChunks.find(chunkCoords + IntVec2(0, 1));
		if (iter != m_activeChunks.end())
		{
			chunk->m_northNeighbour = iter->second;
			iter->second->m_southNeighbour = chunk;
		}

		iter = m_activeChunks.find(chunkCoords + IntVec2(1, 0));
		if (iter != m_activeChunks.end())
		{
			chunk->m_eastNeighbour = iter->second;
			iter->second->m_westNeighbour = chunk;
		}

		iter = m_activeChunks.find(chunkCoords + IntVec2(0, -1));
		if (iter != m_activeChunks.end())
		{
			chunk->m_southNeighbour = iter->second;
			iter->second->m_northNeighbour = chunk;
		}

		iter = m_activeChunks.find(chunkCoords + IntVec2(-1, 0));
		if (iter != m_activeChunks.end())
		{
			chunk->m_westNeighbour = iter->second;
			iter->second->m_eastNeighbour = chunk;
		}

		chunk->InitializeLighting();
		chunk->m_status = ACTIVE;

		//delete the finished job
		delete finishedGenerationJob;

		return true;
	}

	return false;
}

void World::DeactivateChunk()
{
	Vec3 cameraPos = m_player->m_position;
	Vec2 cameraPosXY(cameraPos.x, cameraPos.y);
	Chunk* chunkToDeactivate = nullptr;
	float maxDistanceSquared = 0.f;
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		Chunk& chunk = *iter->second;
		Vec3 chunkCenter = chunk.GetChunkWorldBounds().GetCenter();
		Vec2 chunkCenterXY(chunkCenter.x, chunkCenter.y);
		float sqDistance = GetDistanceSquared2D(cameraPosXY, chunkCenterXY);
		if (sqDistance > m_chunkDeactivationRange * m_chunkDeactivationRange && sqDistance > maxDistanceSquared)
		{
			chunkToDeactivate = &chunk;
			maxDistanceSquared = sqDistance;
		}
	}

	if (chunkToDeactivate)
	{
		if (chunkToDeactivate->m_northNeighbour)
		{
			chunkToDeactivate->m_northNeighbour->m_southNeighbour = nullptr;
			chunkToDeactivate->m_northNeighbour = nullptr;
		}
		if (chunkToDeactivate->m_eastNeighbour)
		{
			chunkToDeactivate->m_eastNeighbour->m_westNeighbour = nullptr;
			chunkToDeactivate->m_eastNeighbour = nullptr;
		}
		if (chunkToDeactivate->m_southNeighbour)
		{
			chunkToDeactivate->m_southNeighbour->m_northNeighbour = nullptr;
			chunkToDeactivate->m_southNeighbour = nullptr;
		}
		if (chunkToDeactivate->m_westNeighbour)
		{
			chunkToDeactivate->m_westNeighbour->m_eastNeighbour = nullptr;
			chunkToDeactivate->m_westNeighbour = nullptr;
		}

		//remove from active chunk list
		auto iter = m_activeChunks.find(chunkToDeactivate->GetChunkCoordinates());
		m_activeChunks.erase(iter);

		//remove this chunks job from the generation queue;
		std::map<IntVec2, Chunk*>::iterator queuedGenerationListIter = m_chunksQueuedForGeneration.find(chunkToDeactivate->GetChunkCoordinates());
		if (queuedGenerationListIter != m_chunksQueuedForGeneration.end())
		{
			m_chunksQueuedForGeneration.erase(queuedGenerationListIter);
		}

		//remove from 
		delete chunkToDeactivate;
	}
}

void World::UpdateChunks(float deltaSeconds)
{
	constexpr int numchunksToRebuild = 2;
	std::vector<Chunk*> chunksToRebuild;
	chunksToRebuild.reserve(m_activeChunks.size());
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		iter->second->Update(deltaSeconds);
		if (iter->second->ShouldRebuildMesh())
			chunksToRebuild.push_back(iter->second);
	}

	//get closest 2 chunks and then rebuild them
	Vec2 camXY = Vec2(m_player->m_position.x, m_player->m_position.y);
	for (int i = 0; i < numchunksToRebuild; i++)
	{
		float closestDistance = FLT_MAX;
		Chunk* closestChunk = nullptr;
		for (int j = 0; j < chunksToRebuild.size(); j++)
		{
			float distance = GetDistanceSquared2D(Chunk::GetChunkCenterXYForGlobalChunkCoords(chunksToRebuild[j]->GetChunkCoordinates()), camXY);
			if (distance < closestDistance)
			{
				closestChunk = chunksToRebuild[j];
				closestDistance = distance;
			}
		}

		if (closestChunk)
		{
			closestChunk->GenerateGeometry();
			closestChunk = nullptr;
			closestDistance = FLT_MAX;
		}
	}

}

void World::UpdateEntities(float deltaSeconds)
{
	for (int i = 0; i < m_allEntities.size(); i++)
	{
		m_allEntities[i]->Update(deltaSeconds);
	}
}

void World::UpdateCameras(float deltaSeconds)
{
	m_playerWorldCamera->Update(deltaSeconds);
}

void World::RenderChunks() const
{
	Vec2 camPos = Vec2(m_player->GetEntityEyePosition().x, m_player->GetEntityEyePosition().y);
	std::vector<Chunk*> chunkList;
	chunkList.reserve(m_activeChunks.size());
	for (auto iter = m_activeChunks.begin(); iter != m_activeChunks.end(); ++iter)
	{
		chunkList.push_back(iter->second);
	}

	std::sort(chunkList.begin(), chunkList.end(), ChunkSort(camPos));
	for (auto iter = chunkList.rbegin(); iter != chunkList.rend(); ++iter)
	{
		(*iter)->Render();
	}
}

void World::RenderEntities() const
{
	for (int i = 0; i < m_allEntities.size(); i++)
	{
		m_allEntities[i]->Render();
	}
}

void World::RenderRaycastImpact() const
{
	std::vector<Vertex_PCU> verts;
	if (m_raycastResult.m_didImpact)
	{
		AddVertsForSphere(verts, 8, 4, 0.05f, m_raycastResult.m_impactPosition, Rgba8::BLACK);
		AddVertsForArrow3D(verts, m_raycastResult.m_impactPosition, m_raycastResult.m_impactPosition + 0.2f * m_raycastResult.m_impactNormal, 0.02f, Rgba8::BLUE);
		g_theRenderer->BindShaderByName("Default");
		g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(int(verts.size()), verts.data());
	}

	if (m_freezeRaycastStart)
	{
		verts.clear();
		AddVertsForCylinder3D(verts, m_cameraStart, m_cameraStart + m_cameraForward * raycastDistance,
			0.02f, m_raycastResult.m_didImpact ? Rgba8::GREEN : Rgba8::RED);
		g_theRenderer->BindShaderByName("Default");
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray(int(verts.size()), verts.data());
	}
}

void World::UpdateDayCycle(float deltaSeconds)
{
	m_worldTime += (deltaSeconds * m_worldTimeScale * m_currentWorldTimeScaleAccelerationFactor) / (60.f * 60.f * 24.f);
	//m_worldTime = 0.5f;

	float glowPerlin = Compute1dPerlinNoise(m_worldTime * 1000.f, 2.f, 5);
	m_glowStrength = RangeMapClamped(glowPerlin, -1.f, 1.f, 0.8f, 1.f);

	float lightningPerlin = Compute1dPerlinNoise(m_worldTime * 1000.f, 2.f, 9);
	m_lightingStrength = RangeMapClamped(lightningPerlin, 0.6f, 0.9f, 0.f, 1.f);

	float fractionPartOfTime = m_worldTime - float(RoundDownToInt(m_worldTime));
	if (fractionPartOfTime < 0.25f || fractionPartOfTime > 0.75f)
	{
		m_currentSkyColor = m_nightSkyColor;
		m_currentOutdoorLightColor = m_nightOutdoorLightColor;
	}
	else
	{
		Rgba8 startSkyColor, targetSkyColor, startOutdoorLight, targetOutdoorLight;
		float input = 0.f;
		if (fractionPartOfTime < 0.5f)
		{
			startSkyColor = m_nightSkyColor;
			targetSkyColor = m_daySkyColor;
			startOutdoorLight = m_nightOutdoorLightColor;
			targetOutdoorLight = m_dayOutdoorLightColor;
			input = RangeMap(fractionPartOfTime, 0.25f, 0.5f, 0.f, 1.f);
		}
		else
		{
			startSkyColor = m_daySkyColor;
			targetSkyColor = m_nightSkyColor;
			startOutdoorLight = m_dayOutdoorLightColor;
			targetOutdoorLight = m_nightOutdoorLightColor;
			input = RangeMap(fractionPartOfTime, 0.5f, 0.75f, 0.f, 1.f);
		}
		m_currentSkyColor = Interpolate(startSkyColor, targetSkyColor, input);
		m_currentSkyColor = Interpolate(m_currentSkyColor, Rgba8::WHITE, m_lightingStrength);
		m_currentOutdoorLightColor = Interpolate(startOutdoorLight, targetOutdoorLight, input);
	}

	g_theApp->SetClearColor(m_currentSkyColor);

	
}

void World::ProcessDirtyLighting()
{
	//for (auto iter = m_dirtyLightBlocks.begin(); iter != m_dirtyLightBlocks.end(); ++iter)
	while(!m_dirtyLightBlocks.empty())
	{
		BlockIterator front = m_dirtyLightBlocks.front();
		m_dirtyLightBlocks.pop_front();
		Block* block = front.GetBlock();
		block->SetIsBlockLightDirty(false);
		//to-do: compute blocks indoor lighting influences
		uint8_t currentIndoorLightInfluence = block->GetIndoorLightInfluence();
		uint8_t computedIndoorLightInfluence = ComputeIndoorLightInfluence(front);
		uint8_t currentOutdoorLightInfluence = block->GetOutdoorLightInfluence();
		uint8_t computedOutdoorLightInfluence = ComputeOutdoorLightInfluence(front);
		if ((currentOutdoorLightInfluence != computedOutdoorLightInfluence) || (currentIndoorLightInfluence != computedIndoorLightInfluence))
		{
			block->SetOutdoorLightInfluence(computedOutdoorLightInfluence);
			block->SetIndoorLightInfluence(computedIndoorLightInfluence);
			front.m_chunkBlockBelongsTo->SetChunkToDirty();
			MarkNeighbouringChunksAndBlocksAsDirty(front);
		}
	}
}

uint8_t World::ComputeIndoorLightInfluence(const BlockIterator& blockIter) const
{
	uint8_t maxComputedLightInfluence = 0;
	uint8_t computedLightInfluence = 0;
	Block& block = *blockIter.GetBlock();
	if (BlockDefintion::DoesBlockTypeEmitLight(block.m_typeIndex))
	{
		computedLightInfluence = BlockDefintion::s_definitions[block.m_typeIndex].m_indoorLightInfluence;
		if (computedLightInfluence > maxComputedLightInfluence)
			maxComputedLightInfluence = computedLightInfluence;
	}
	if (!BlockDefintion::IsBlockTypeOpaque(block.m_typeIndex))
	{
		computedLightInfluence = GetHighestIndoorLightInfluenceAmongNeighbours(blockIter);
		if (computedLightInfluence > 0)
			computedLightInfluence--;
		if (computedLightInfluence > maxComputedLightInfluence)
			maxComputedLightInfluence = computedLightInfluence;
	}

	return maxComputedLightInfluence;
}

uint8_t World::ComputeOutdoorLightInfluence(const BlockIterator& blockIter) const
{
	uint8_t maxComputedLightInfluence = 0;
	uint8_t computedLightInfluence = 0;
	Block& block = *blockIter.GetBlock();
	if (block.IsBlockSky())
	{
		computedLightInfluence = 15;
		if (computedLightInfluence > maxComputedLightInfluence)
			maxComputedLightInfluence = computedLightInfluence;
	}
	if (!BlockDefintion::IsBlockTypeOpaque(block.m_typeIndex))
	{
		computedLightInfluence = GetHighestOutdoorLightInfluenceAmongNeighbours(blockIter);
		if (computedLightInfluence > 0)
			computedLightInfluence--;
		if (computedLightInfluence > maxComputedLightInfluence)
			maxComputedLightInfluence = computedLightInfluence;
	}

	return maxComputedLightInfluence;
}

uint8_t World::GetHighestOutdoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const
{
	uint8_t maxOutdoorLightInfluence = 0;
	uint8_t outdoorLightInfluence = 0;
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		outdoorLightInfluence = neighbour.GetBlock()->GetOutdoorLightInfluence();
		if (outdoorLightInfluence > maxOutdoorLightInfluence)
			maxOutdoorLightInfluence = outdoorLightInfluence;
	}

	return maxOutdoorLightInfluence;
}

uint8_t World::GetHighestIndoorLightInfluenceAmongNeighbours(const BlockIterator& blockIter) const
{
	uint8_t maxIndoorLightInfluence = 0;
	uint8_t indoorLightInfluence = 0;
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunkBlockBelongsTo)
	{
		indoorLightInfluence = neighbour.GetBlock()->GetIndoorLightInfluence();
		if (indoorLightInfluence > maxIndoorLightInfluence)
			maxIndoorLightInfluence = indoorLightInfluence;
	}

	return maxIndoorLightInfluence;
}

void World::MarkNeighbouringChunksAndBlocksAsDirty(const BlockIterator& blockIter)
{
	BlockIterator neighbour = blockIter.GetNorthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}

	neighbour = blockIter.GetEastNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}

	neighbour = blockIter.GetSouthNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}

	neighbour = blockIter.GetWestNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}

	neighbour = blockIter.GetAboveNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}

	neighbour = blockIter.GetBelowNeighbour();
	if (neighbour.m_chunkBlockBelongsTo != nullptr)
	{
		neighbour.m_chunkBlockBelongsTo->SetChunkToDirty();
		if (!BlockDefintion::IsBlockTypeOpaque(neighbour.GetBlock()->m_typeIndex))
			MarkLightingDirty(neighbour);
	}
}

void World::CopyDataToCBOAndBindIt() const
{
	GameConstants gameConstants;
	Vec3 camPos = m_player->m_position;
	gameConstants.cameraWorldPos = Vec4(camPos.x, camPos.y, camPos.z, 0.f);
	(m_indoorLightColor * m_glowStrength).GetAsFloats(gameConstants.indoorLightColor);
	m_currentOutdoorLightColor.GetAsFloats(gameConstants.outdoorLightColor);
	m_currentSkyColor.GetAsFloats(gameConstants.skyColor);
	gameConstants.fogStartDistance = m_fogStartDistance;
	gameConstants.fogEndDistance = m_fogEndDistance;
	gameConstants.fogMaxAlpha = m_fogMaxAlpha;
	gameConstants.worldTime = m_worldTime;

	g_theRenderer->CopyCPUToGPU(&gameConstants, sizeof(GameConstants), m_gameCBO);
	g_theRenderer->BindConstantBuffer(gameConstantsSlotNumber, m_gameCBO);
}

void World::PerformRaycast()
{
	if (!m_freezeRaycastStart)
	{
		m_cameraStart = m_player->GetEntityEyePosition();
		m_cameraForward = m_player->GetForwardVector();
	}

	m_raycastResult = RaycastVsWorld(m_cameraStart, m_cameraForward, raycastDistance);
}

GameRaycastResult3D World::RaycastVsWorld(const Vec3& start, const Vec3& direction, float distance)
{
	GameRaycastResult3D hit;
	Chunk* currentChunk = nullptr;
	int blockIndex = 0;
	BlockIterator blockIter = { currentChunk, blockIndex };

	if (start.z < 0 || start.z > CHUNK_MAX_Z)
		return hit;

	IntVec2 chunkCoords = Chunk::GetChunkCoordinatedForWorldPosition(start);
	std::map<IntVec2, Chunk*>::iterator iter = m_activeChunks.find(chunkCoords);
	if (iter != m_activeChunks.end())
	{
		currentChunk = iter->second;
	}
	else
	{
		return hit;
	}
	Vec3 chunkBoundsMins = currentChunk->GetChunkWorldBounds().m_mins;
	IntVec3 localCoords = IntVec3(int(start.x - chunkBoundsMins.x), int(start.y - chunkBoundsMins.y), int(start.z));
	blockIndex = Chunk::GetBlockIndexFromLocalCoords(localCoords);
	blockIter = { currentChunk, blockIndex };

	if (BlockDefintion::IsBlockTypeOpaque(blockIter.GetBlock()->m_typeIndex))
	{
		hit.m_didImpact = true;
		hit.m_impactDistance = 0.f;
		hit.m_impactPosition = start;
		hit.m_impactNormal = -direction;
		hit.m_blockImpacted = blockIter;
		return hit;
	}

	float fwdDistPerXCrossing = 1.0f / fabsf(direction.x);
	int tileStepDirectionX = direction.x <= 0 ? -1 : 1;
	float xAtFirstXCrossing = (float)( RoundDownToInt(start.x) + (tileStepDirectionX + 1) / 2);
	float xDistToFirstXCrossing = xAtFirstXCrossing - start.x;
	float fwdDistAtNextXCrossing = fabsf(xDistToFirstXCrossing) * fwdDistPerXCrossing;

	float fwdDistPerYCrossing = 1.0f / fabsf(direction.y);
	int tileStepDirectionY = direction.y <= 0 ? -1 : 1;
	float yAtFirstYCrossing = (float)(RoundDownToInt(start.y) + (tileStepDirectionY + 1) / 2);
	float yDistToFirstYCrossing = yAtFirstYCrossing - start.y;
	float fwdDistAtNextYCrossing = fabsf(yDistToFirstYCrossing) * fwdDistPerYCrossing;

	float fwdDistPerZCrossing = 1.0f / fabsf(direction.z);
	int tileStepDirectionZ = direction.z <= 0 ? -1 : 1;
	float zAtFirstZCrossing = (float)(localCoords.z + (tileStepDirectionZ + 1) / 2);
	float zDistToFirstZCrossing = zAtFirstZCrossing - start.z;
	float fwdDistAtNextZCrossing = fabsf(zDistToFirstZCrossing) * fwdDistPerZCrossing;

	while (true)
	{
		if (fwdDistAtNextXCrossing <= fwdDistAtNextYCrossing && fwdDistAtNextXCrossing <= fwdDistAtNextZCrossing)
		{
			blockIter = tileStepDirectionX > 0 ? blockIter.GetEastNeighbour() : blockIter.GetWestNeighbour();
			if (BlockDefintion::IsBlockTypeOpaque(blockIter.GetBlock()->m_typeIndex))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextXCrossing);
				hit.m_didImpact = true;
				hit.m_impactDistance = fwdDistAtNextXCrossing;
				hit.m_impactPosition = impactPos;
				hit.m_impactNormal = Vec3(start.x - hit.m_impactPosition.x, 0.f, 0.f).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextXCrossing += fwdDistPerXCrossing;
		}
		else if(fwdDistAtNextYCrossing <= fwdDistAtNextXCrossing && fwdDistAtNextYCrossing <= fwdDistAtNextZCrossing)
		{
			blockIter = tileStepDirectionY > 0 ? blockIter.GetNorthNeighbour() : blockIter.GetSouthNeighbour();
			if (BlockDefintion::IsBlockTypeOpaque(blockIter.GetBlock()->m_typeIndex))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextYCrossing);
				hit.m_didImpact = true;
				hit.m_impactDistance = fwdDistAtNextYCrossing;
				hit.m_impactPosition = start + (direction * fwdDistAtNextYCrossing);
				hit.m_impactNormal = Vec3(0.f, start.y - hit.m_impactPosition.y, 0.f).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextYCrossing += fwdDistPerYCrossing;
		}
		else
		{
			blockIter = tileStepDirectionZ > 0 ? blockIter.GetAboveNeighbour() : blockIter.GetBelowNeighbour();
			if (BlockDefintion::IsBlockTypeOpaque(blockIter.GetBlock()->m_typeIndex))
			{
				Vec3 impactPos = start + (direction * fwdDistAtNextZCrossing);
				hit.m_didImpact = true;
				hit.m_impactDistance = fwdDistAtNextZCrossing;
				hit.m_impactPosition = start + (direction * fwdDistAtNextZCrossing);
				hit.m_impactNormal = Vec3(0.f, 0.f, start.z - hit.m_impactPosition.z).GetNormalized();
				hit.m_blockImpacted = blockIter;
				return hit;
			}

			fwdDistAtNextZCrossing += fwdDistPerZCrossing;
		}

		//past max distance, then return
		if (fwdDistAtNextXCrossing > distance && fwdDistAtNextYCrossing > distance && fwdDistAtNextZCrossing > distance)
		{
			return hit;
		}
	}

	//return null block iterator
	hit.m_blockImpacted = { nullptr, 0 };
	return hit;
}

void World::MarkLightingDirty(const BlockIterator& blockIter)
{
	Block& block = *(blockIter.m_chunkBlockBelongsTo->GetBlock(blockIter.m_blockIndex));
	if (block.IsBlockLightDirty())
		return;

	block.SetIsBlockLightDirty(true);
	m_dirtyLightBlocks.push_back(blockIter);
}

void World::MarkLightingDirtyIfNotOpaque(const BlockIterator& blockIter)
{
	Block& block = *(blockIter.m_chunkBlockBelongsTo->GetBlock(blockIter.m_blockIndex));
	if (!BlockDefintion::IsBlockTypeOpaque(block.m_typeIndex))
	{
		MarkLightingDirty(blockIter);
	}
}

void World::MarkLightingDirtyIfNotSkyAndNotOpaque(const BlockIterator& blockIter)
{
	if (blockIter.m_chunkBlockBelongsTo)
	{
		Block& block = *(blockIter.m_chunkBlockBelongsTo->GetBlock(blockIter.m_blockIndex));
		if (!block.IsBlockSky())
		{
			MarkLightingDirtyIfNotOpaque(blockIter);
		}
	}
}

Chunk* World::GetChunk(IntVec2 chunkCoords) const
{
	std::map<IntVec2, Chunk*>::const_iterator iter = m_activeChunks.find(chunkCoords);
	if (iter != m_activeChunks.end())
		return iter->second;

	return nullptr;
}

bool ChunkSort::operator()(Chunk* a, Chunk* b)
{
	float aDistSquareFromCam = GetDistanceSquared2D(m_camPos, Chunk::GetChunkCenterXYForGlobalChunkCoords(a->GetChunkCoordinates()));
	float bDistSquareFromCam = GetDistanceSquared2D(m_camPos, Chunk::GetChunkCenterXYForGlobalChunkCoords(b->GetChunkCoordinates()));
	return aDistSquareFromCam < bDistSquareFromCam;
}
