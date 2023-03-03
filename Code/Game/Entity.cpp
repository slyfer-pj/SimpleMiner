#include "Game/Entity.hpp"
#include "Game/Game.hpp"
#include "Game/Controller.hpp"
#include "Game/GameCamera.hpp"
#include "Game/Chunk.hpp"
#include "Game/World.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/IntVec3.hpp"

extern Renderer* g_theRenderer;

Entity::Entity(World* world, Vec3 spawnPosition)
	:m_world(world), m_position(spawnPosition)
{
}

Entity::Entity(World* world)
	:m_world(world)
{
}

void Entity::Update(float deltaSeconds)
{
	m_controller->Update(deltaSeconds);

	UpdatePhysics(deltaSeconds);

	CollideWithWorld();
}

void Entity::Render() const
{
	std::vector<Vertex_PCU> verts;
	AABB3 bounds = GetEntityBounds();
	AddVertsForAABB3D(verts, bounds);
	Vec3 eyePos = GetEntityEyePosition();
	g_theRenderer->BindShaderByName("Default");
	g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::WIREFRAME, WindingOrder::COUNTERCLOCKWISE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());

	verts.clear();
	AddVertsForCylinder3D(verts, eyePos, eyePos + GetForwardVector() * 1.f, 0.01f, Rgba8::WHITE);
	g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());

	verts.clear();
	//if player is below sea level
	/*if (GetEntityEyePosition().z < 64.f)
	{
		const Rgba8 blueTint = Rgba8(21, 108, 153, 200);
		Camera screenCam = m_gameCamera->GetScreenCamera();
		g_theRenderer->BeginCamera(screenCam);
		AddVertsForAABB2D(verts, AABB2(screenCam.GetOrthoBottomLeft(), screenCam.GetOrthoTopRight()), blueTint);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray((int)verts.size(), verts.data());
		g_theRenderer->EndCamera(screenCam);
	}*/
}

Mat44 Entity::GetModelMatrix() const
{
	Mat44 translationMatrix = Mat44::CreateTranslation3D(m_position);
	Mat44 rotationMatrix = m_orientation3D.GetAsMatrix_XFwd_YLeft_ZUp();
	translationMatrix.Append(rotationMatrix);
	return translationMatrix;
}

Vec3 Entity::GetForwardVector() const
{
	//return Mat44::CreateZRotationDegrees(m_orientation3D.m_yawDegrees).GetIBasis3D().GetNormalized();
	return GetModelMatrix().GetIBasis3D().GetNormalized();
}

Vec3 Entity::GetLeftVector() const
{
	return GetModelMatrix().GetJBasis3D().GetNormalized();
}

void Entity::AddForces(const Vec3& force)
{
	m_acceleration += force;
}

void Entity::AddImpulse(const Vec3& impulse)
{
	m_velocity += impulse;
}

void Entity::RotateEntity(const EulerAngles& rotation)
{
	m_deltaRotation = rotation;
	/*m_orientation3D.m_yawDegrees += rotation.m_yawDegrees;
	m_orientation3D.m_pitchDegrees -= rotation.m_pitchDegrees;
	m_orientation3D.m_rollDegrees += rotation.m_rollDegrees;*/
}

void Entity::CyclePhysicsMode()
{
	int currentModeIndex = static_cast<int>(m_currentPhysicsMode);
	currentModeIndex++;

	if (currentModeIndex >= static_cast<int>(PHYSICS_MODE_NUM_MODES))
		currentModeIndex = 0;

	m_currentPhysicsMode = static_cast<PHYSICS_MODE>(currentModeIndex);
}

void Entity::Jump()
{
	constexpr float jumpForce = 5.f;
	if (m_isGrounded)
		AddImpulse(Vec3(0.f, 0.f, jumpForce));
}

Vec3 Entity::GetEntityEyePosition() const
{
	return m_position + Vec3(0.f, 0.f, 1.65f);
}

AABB3 Entity::GetEntityBounds() const
{
	Vec3 mins = Vec3(m_position.x - 0.3f, m_position.y - 0.3f, m_position.z);
	return AABB3(mins, mins + Vec3(0.6f, 0.6f, 1.8f));
}

void Entity::CollideWithWorld()
{
	//reset grounded bool
	m_isGrounded = false;
	if (m_currentPhysicsMode == PHYSICS_MODE_NOCLIP)
		return;

	IntVec2 chunkCoords = Chunk::GetChunkCoordinatedForWorldPosition(m_position);
	Chunk* currentChunk = m_world->GetChunk(chunkCoords);
	if (!currentChunk)
		return;

	Vec3 boundsMins = currentChunk->GetChunkWorldBounds().m_mins;
	IntVec3 localCoords = IntVec3(int(m_position.x - boundsMins.x), int(m_position.y - boundsMins.y), int(m_position.z + 0.6f));
	int blockIndex = Chunk::GetBlockIndexFromLocalCoords(localCoords);
	if (blockIndex > CHUNK_TOTAL_BLOCKS)
		currentChunk = nullptr;
	BlockIterator blockIter = { currentChunk, blockIndex };

	if (currentChunk)
	{
		//push out of cardinal neighbours (1 distance away)
		PushOutOfCardinalNeighbours(blockIter);

		//push out of neighbours of cardinal neighbours (2 distance away)
		PushOutOfNeighboursOfCardinalNeighbours(blockIter);

		//push out of diagonal neighbours (3 distance away)
		PushOutOfDiagonalNeighbours(blockIter);
	}
}

void Entity::UpdatePhysics(float deltaSeconds)
{
	constexpr float gravity = 10.f;
	constexpr float groundFriction = 5.f;
	constexpr float maxVelocity = 20.f;
	
	//apply gravity
	if (m_currentPhysicsMode == PHYSICS_MODE_WALKING)
	{
		AddForces(Vec3(0.f, 0.f, -gravity));
	}
	//apply friction to XYvelocity
	AddForces(Vec3(m_velocity.x, m_velocity.y, 0.f) * -groundFriction);

	m_velocity += m_acceleration * deltaSeconds;
	//clamp velocity
	if (m_velocity.GetLength() > maxVelocity)
		m_velocity = m_velocity.GetClamped(maxVelocity);
	//if (m_gameCamera->GetCurrentMode() != CAMERA_MODE_SPECTATOR)
	//{
		m_position += m_velocity * deltaSeconds;

		m_orientation3D.m_yawDegrees += m_deltaRotation.m_yawDegrees;
		m_orientation3D.m_pitchDegrees -= m_deltaRotation.m_pitchDegrees;
		m_orientation3D.m_rollDegrees += m_deltaRotation.m_rollDegrees;
		m_orientation3D.m_pitchDegrees = Clamp(m_orientation3D.m_pitchDegrees, -89.f, 89.f);
		m_orientation3D.m_rollDegrees = Clamp(m_orientation3D.m_rollDegrees, -89.f, 89.f);
	//}
	m_acceleration = Vec3::ZERO;
}

void Entity::PushOutOfCardinalNeighbours(BlockIterator currentBlockIter)
{
	//push out from block your are in currently
	if (currentBlockIter.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), currentBlockIter.GetBlockBounds()))
	{
		PushOutAboveFromBlock(currentBlockIter);
	}

	BlockIterator below = currentBlockIter.GetBelowNeighbour();
	if (below.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), below.GetBlockBounds()))
	{
		PushOutAboveFromBlock(below);
		//is grounded if pushed out of block below you
		m_isGrounded = true;
	}
	BlockIterator above = currentBlockIter.GetAboveNeighbour();
	if (above.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), above.GetBlockBounds()))
	{
		PushOutBelowFromBlock(above);
	}
	BlockIterator north = currentBlockIter.GetNorthNeighbour();
	if (north.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), north.GetBlockBounds()))
	{
		PushOutSouthFromBlock(north);
	}
	BlockIterator west = currentBlockIter.GetWestNeighbour();
	if (west.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), west.GetBlockBounds()))
	{
		PushOutEastFromBlock(west);
	}
	BlockIterator south = currentBlockIter.GetSouthNeighbour();
	if (south.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), south.GetBlockBounds()))
	{
		PushOutNorthFromBlock(south);
	}
	BlockIterator east = currentBlockIter.GetEastNeighbour();
	if (east.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), east.GetBlockBounds()))
	{
		PushOutWestFromBlock(east);
	}
}

void Entity::PushOutOfNeighboursOfCardinalNeighbours(BlockIterator currentBlockIter)
{
	//neighbours of west
	BlockIterator west = currentBlockIter.GetWestNeighbour();
	BlockIterator northOfWest = west.GetNorthNeighbour();
	if (northOfWest.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northOfWest.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northOfWest);
	}
	BlockIterator southOfWest = west.GetSouthNeighbour();
	if (southOfWest.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southOfWest.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southOfWest);
	}
	BlockIterator belowOfWest = west.GetBelowNeighbour();
	if (belowOfWest.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), belowOfWest.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(belowOfWest);
	}
	BlockIterator aboveOfWest = west.GetAboveNeighbour();
	if (aboveOfWest.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), aboveOfWest.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(aboveOfWest);
	}

	//neighbours of east
	BlockIterator east = currentBlockIter.GetEastNeighbour();
	BlockIterator northOfEast = east.GetNorthNeighbour();
	if (northOfEast.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northOfEast.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northOfEast);
	}
	BlockIterator southOfEast = east.GetSouthNeighbour();
	if (southOfEast.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southOfEast.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southOfEast);
	}
	BlockIterator belowOfEast = east.GetBelowNeighbour();
	if (belowOfEast.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), belowOfEast.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(belowOfEast);
	}
	BlockIterator aboveOfEast = east.GetAboveNeighbour();
	if (aboveOfEast.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), aboveOfEast.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(aboveOfEast);
	}

	//neighbours of north (only above and below required as other neighbours have already been processed)
	BlockIterator north = currentBlockIter.GetNorthNeighbour();
	BlockIterator belowOfNorth = north.GetBelowNeighbour();
	if (belowOfNorth.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), belowOfNorth.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(belowOfNorth);
	}
	BlockIterator aboveOfNorth = north.GetAboveNeighbour();
	if (aboveOfNorth.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), aboveOfNorth.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(aboveOfNorth);
	}

	//neighbours of south (only above and below required as other neighbours have already been processed)
	BlockIterator south = currentBlockIter.GetSouthNeighbour();
	BlockIterator belowOfSouth = south.GetBelowNeighbour();
	if (belowOfSouth.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), belowOfSouth.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(belowOfSouth);
	}
	BlockIterator aboveOfSouth = south.GetAboveNeighbour();
	if (aboveOfSouth.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), aboveOfSouth.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(aboveOfSouth);
	}
}

void Entity::PushOutOfDiagonalNeighbours(BlockIterator currentBlockIter)
{
	BlockIterator northWestUp = currentBlockIter.GetWestNeighbour().GetNorthNeighbour().GetAboveNeighbour();
	if (northWestUp.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northWestUp.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northWestUp);
	}
	BlockIterator northWestDown = currentBlockIter.GetWestNeighbour().GetNorthNeighbour().GetBelowNeighbour();
	if (northWestDown.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northWestDown.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northWestDown);
	}
	BlockIterator southWestUp = currentBlockIter.GetWestNeighbour().GetSouthNeighbour().GetAboveNeighbour();
	if (southWestUp.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southWestUp.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southWestUp);
	}
	BlockIterator southWestDown = currentBlockIter.GetWestNeighbour().GetSouthNeighbour().GetBelowNeighbour();
	if (southWestDown.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southWestDown.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southWestDown);
	}
	BlockIterator northEastUp = currentBlockIter.GetEastNeighbour().GetNorthNeighbour().GetAboveNeighbour();
	if (northEastUp.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northEastUp.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northEastUp);
	}
	BlockIterator northEastDown = currentBlockIter.GetEastNeighbour().GetNorthNeighbour().GetBelowNeighbour();
	if (northEastDown.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), northEastDown.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(northEastDown);
	}
	BlockIterator southEastUp = currentBlockIter.GetEastNeighbour().GetSouthNeighbour().GetAboveNeighbour();
	if (southEastUp.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southEastUp.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southEastUp);
	}
	BlockIterator southEastDown = currentBlockIter.GetEastNeighbour().GetSouthNeighbour().GetBelowNeighbour();
	if (southEastDown.IsBlockOpaque() && DoAABB3sOverlap(GetEntityBounds(), southEastDown.GetBlockBounds()))
	{
		PushOutOfShortestDistanceFromBlock(southEastDown);
	}
}

void Entity::PushOutWestFromBlock(BlockIterator blockIter)
{
	m_position -= Vec3(GetEntityBounds().m_maxs.x - blockIter.GetBlockBounds().m_mins.x, 0.f, 0.f);
	m_velocity.x = 0.f;
}

void Entity::PushOutEastFromBlock(BlockIterator blockIter)
{
	m_position += Vec3(blockIter.GetBlockBounds().m_maxs.x - GetEntityBounds().m_mins.x, 0.f, 0.f);
	m_velocity.x = 0.f;
}

void Entity::PushOutNorthFromBlock(BlockIterator blockIter)
{
	m_position += Vec3(0.f, blockIter.GetBlockBounds().m_maxs.y - GetEntityBounds().m_mins.y, 0.f);
	m_velocity.y = 0.f;
}

void Entity::PushOutSouthFromBlock(BlockIterator blockIter)
{
	m_position -= Vec3(0.f, GetEntityBounds().m_maxs.y - blockIter.GetBlockBounds().m_mins.y, 0.f);
	m_velocity.y = 0.f;
}

void Entity::PushOutAboveFromBlock(BlockIterator blockIter)
{
	m_position += Vec3(0.f, 0.f, blockIter.GetBlockBounds().m_maxs.z - GetEntityBounds().m_mins.z);
	m_velocity.z = 0.f;
}

void Entity::PushOutBelowFromBlock(BlockIterator blockIter)
{
	m_position -= Vec3(0.f, 0.f, GetEntityBounds().m_maxs.z - blockIter.GetBlockBounds().m_mins.z);
	m_velocity.z = 0.f;
}

void Entity::PushOutOfShortestDistanceFromBlock(BlockIterator blockIter)
{
	AABB3 entityBounds = GetEntityBounds();
	AABB3 blockBounds = blockIter.GetBlockBounds();

	float xOverlap, yOverlap, zOverlap;
	int xPushDirection, yPushDirection, zPushDirection;
	if (entityBounds.m_mins.x < blockBounds.m_mins.x)
	{
		xOverlap = entityBounds.m_maxs.x - blockBounds.m_mins.x;
		xPushDirection = -1;
	}
	else
	{
		xOverlap = blockBounds.m_maxs.x - entityBounds.m_mins.x;
		xPushDirection = 1;
	}

	if (entityBounds.m_mins.y < blockBounds.m_mins.y)
	{
		yOverlap = entityBounds.m_maxs.y - blockBounds.m_mins.y;
		yPushDirection = -1;
	}
	else
	{
		yOverlap = blockBounds.m_maxs.y - entityBounds.m_mins.y;
		yPushDirection = 1;
	}

	if (entityBounds.m_mins.z < blockBounds.m_mins.z)
	{
		zOverlap = entityBounds.m_maxs.z - blockBounds.m_mins.z;
		zPushDirection = -1;
	}
	else
	{
		zOverlap = blockBounds.m_maxs.z - entityBounds.m_mins.z;
		zPushDirection = 1;
	}

	if (xOverlap < yOverlap && xOverlap < zOverlap)
	{
		if (xPushDirection < 0)
			PushOutWestFromBlock(blockIter);
		else
			PushOutEastFromBlock(blockIter);
	}
	else if (yOverlap < xOverlap && yOverlap < zOverlap)
	{
		if (yPushDirection < 0)
			PushOutSouthFromBlock(blockIter);
		else
			PushOutNorthFromBlock(blockIter);
	}
	else
	{
		if (zPushDirection < 0)
			PushOutBelowFromBlock(blockIter);
		else
			PushOutAboveFromBlock(blockIter);
	}
}

