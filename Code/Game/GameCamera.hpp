#pragma once
#include "Game/Entity.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Renderer/Camera.hpp"

class World;

enum CAMERA_MODE
{
	CAMERA_MODE_SPECTATOR = 0,
	CAMERA_MODE_INDEPENDENT,
	CAMERA_MODE_FIRST_PERSON,
	CAMERA_MODE_FIXED_ANGLE_TRACKING,
	CAMERA_MODE_OVER_SHOULDER,
	CAMERA_MODE_NUM_MODES
};

class GameCamera : public Entity
{
public:
	GameCamera(World* world, Entity* entity);
	void CycleToNextMode();
	void Update(float deltaSeconds);
	Camera GetWorldCamera() const { return m_gameWorldCamera; }
	Camera GetScreenCamera() const { return m_gameScreenCamera; }
	CAMERA_MODE GetCurrentMode() { return m_currentMode; }

private:
	Entity* m_entityToFollow = nullptr;
	CAMERA_MODE m_currentMode = CAMERA_MODE_FIRST_PERSON;
	Camera m_gameWorldCamera;
	Camera m_gameScreenCamera;

private:
	void OnChangeMode();
};