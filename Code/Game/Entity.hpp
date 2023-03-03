#pragma once
#include "Game/BlockIterator.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/AABB3.hpp"

class World;
class Camera;
class Controller;
class GameCamera;

enum PHYSICS_MODE
{
	PHYSICS_MODE_WALKING = 0,
	PHYSICS_MODE_FLYING,
	PHYSICS_MODE_NOCLIP,
	PHYSICS_MODE_NUM_MODES
};

class Entity
{
public:
	Entity() {};
	Entity(World* world);
	Entity(World* world, Vec3 spawnPosition);
	virtual void Update(float deltaSeconds);
	virtual void Render() const;
	Mat44 GetModelMatrix() const;
	Vec3 GetForwardVector() const;
	Vec3 GetLeftVector() const;
	virtual ~Entity() {};
	void AddForces(const Vec3& force);
	void AddImpulse(const Vec3& impulse);
	void RotateEntity(const EulerAngles& rotation);
	void CyclePhysicsMode();
	void Jump();
	Vec3 GetEntityEyePosition() const;
	PHYSICS_MODE GetCurrentPhysicsMode() const { return m_currentPhysicsMode; }
	Controller* m_controller = nullptr;
	GameCamera* m_gameCamera = nullptr;

public:
	Vec3 m_position = Vec3::ZERO;
	Vec3 m_velocity = Vec3::ZERO;
	Vec3 m_acceleration = Vec3::ZERO;
	EulerAngles m_orientation3D;
	EulerAngles m_deltaRotation;
	World* m_world = nullptr;

private:
	bool m_isGrounded = false;
	PHYSICS_MODE m_currentPhysicsMode = PHYSICS_MODE_FLYING;

private:
	AABB3 GetEntityBounds() const;
	void CollideWithWorld();
	void UpdatePhysics(float deltaSeconds);
	void PushOutOfCardinalNeighbours(BlockIterator currentBlockIter);
	void PushOutOfNeighboursOfCardinalNeighbours(BlockIterator currentBlockIter);
	void PushOutOfDiagonalNeighbours(BlockIterator currentBlockIter);
	void PushOutWestFromBlock(BlockIterator blockIter);
	void PushOutEastFromBlock(BlockIterator blockIter);
	void PushOutNorthFromBlock(BlockIterator blockIter);
	void PushOutSouthFromBlock(BlockIterator blockIter);
	void PushOutAboveFromBlock(BlockIterator blockIter);
	void PushOutBelowFromBlock(BlockIterator blockIter);
	void PushOutOfShortestDistanceFromBlock(BlockIterator blockIter);
};
