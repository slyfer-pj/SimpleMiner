#include "Game/GameCamera.hpp"
#include "Game/Entity.hpp"
#include "Game/Controller.hpp"
#include "Game/World.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/MathUtils.hpp"

extern Window* g_theWindow;

constexpr float FIXED_ANGLE_TRACKING_OFFSET_DISTANCE = 10.f;
constexpr float OVER_SHOULDER_OFFSET_DISTANCE = 4.f;

GameCamera::GameCamera(World* world, Entity* entity)
	:Entity(world), m_entityToFollow(entity)
{
	m_gameScreenCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(m_world->GetGame()->m_uiScreenSize.x, m_world->GetGame()->m_uiScreenSize.y));
	m_gameWorldCamera.SetViewToRenderTransform(Vec3(0.f, 0.f, 1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
	float fov = g_gameConfigBlackboard.GetValue("worldCamFov", 60.f);
	float nearZ = g_gameConfigBlackboard.GetValue("worldCamNearZ", 60.f);
	float farZ = g_gameConfigBlackboard.GetValue("worldCamFarZ", 60.f);
	m_gameWorldCamera.SetPerspectiveView(g_theWindow->GetConfig().m_clientAspect, fov, nearZ, farZ);
	m_position = entity->m_position;
	m_orientation3D = entity->m_orientation3D;
}

void GameCamera::CycleToNextMode()
{
	int currentModeIndex = static_cast<int>(m_currentMode);
	currentModeIndex++;

	if (currentModeIndex >= static_cast<int>(CAMERA_MODE_NUM_MODES))
		currentModeIndex = 0;

	m_currentMode = static_cast<CAMERA_MODE>(currentModeIndex);
	OnChangeMode();
}

void GameCamera::Update(float deltaSeconds)
{
	Vec3 eyePosition = Vec3(0.f, 0.f, 1.65f);
	Vec3 basePosition = Vec3::ZERO;
	EulerAngles baseRotation;
	Vec3 offset = Vec3::ZERO;

	if (m_currentMode == CAMERA_MODE_SPECTATOR)
	{
		m_velocity += m_acceleration * deltaSeconds;
		m_position += m_velocity * deltaSeconds;
		m_orientation3D.m_yawDegrees += m_deltaRotation.m_yawDegrees;
		m_orientation3D.m_pitchDegrees -= m_deltaRotation.m_pitchDegrees;
		m_orientation3D.m_rollDegrees += m_deltaRotation.m_rollDegrees;
		m_orientation3D.m_pitchDegrees = Clamp(m_orientation3D.m_pitchDegrees, -89.f, 89.f);
		basePosition = m_position;
		baseRotation = m_orientation3D;
		m_acceleration = Vec3::ZERO;
	}
	else if (m_currentMode == CAMERA_MODE_INDEPENDENT)
	{
		basePosition = m_position;
		baseRotation = m_orientation3D;
	}
	else if (m_currentMode == CAMERA_MODE_FIRST_PERSON)
	{
		basePosition = m_entityToFollow->m_position;
		baseRotation = m_entityToFollow->m_orientation3D;
	}
	else if (m_currentMode == CAMERA_MODE_FIXED_ANGLE_TRACKING)
	{
		basePosition = m_entityToFollow->m_position;
		baseRotation = EulerAngles(40.f, 30.f, 0.f);
		Vec3 baseRotationForward = baseRotation.GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D();
		GameRaycastResult3D result = m_world->RaycastVsWorld(basePosition + eyePosition, -1.f * baseRotationForward, FIXED_ANGLE_TRACKING_OFFSET_DISTANCE);
		float percentOfDistance = 1.f;
		if (result.m_didImpact)
		{
			constexpr float breathingRoom = 0.1f;
			percentOfDistance = (result.m_impactDistance / FIXED_ANGLE_TRACKING_OFFSET_DISTANCE) * (1.f - breathingRoom);
		}
		offset = baseRotationForward * (FIXED_ANGLE_TRACKING_OFFSET_DISTANCE * percentOfDistance);
	}
	else if (m_currentMode == CAMERA_MODE_OVER_SHOULDER)
	{
		basePosition = m_entityToFollow->m_position;
		baseRotation = m_entityToFollow->m_orientation3D;
		GameRaycastResult3D result = m_world->RaycastVsWorld(basePosition + eyePosition, -1.f * m_entityToFollow->GetForwardVector(), OVER_SHOULDER_OFFSET_DISTANCE);
		float percentOfDistance = 1.f;
		if (result.m_didImpact)
		{
			constexpr float breathingRoom = 0.1f;
			percentOfDistance = (result.m_impactDistance / OVER_SHOULDER_OFFSET_DISTANCE) * (1.f - breathingRoom);
		}
		offset = baseRotation.GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D() * (OVER_SHOULDER_OFFSET_DISTANCE * percentOfDistance);
	}

	Vec3 finalPosition = basePosition + eyePosition - offset;
	m_gameWorldCamera.SetTransform(finalPosition, baseRotation);
}

void GameCamera::OnChangeMode()
{
	//if spectator, dispossess controller from entity to follow and possess self. else restore controller possession
	if (m_currentMode == CAMERA_MODE_SPECTATOR)
	{
		m_entityToFollow->m_controller->Possess(this);
	}
	else
	{
		m_entityToFollow->m_controller->Possess(m_entityToFollow);
	}

	//set initial positions for modes
	if (m_currentMode == CAMERA_MODE_SPECTATOR || m_currentMode == CAMERA_MODE_INDEPENDENT)
	{
		Vec3 offset = EulerAngles(40.f, 30.f, 0.f).GetAsMatrix_XFwd_YLeft_ZUp().GetIBasis3D() * FIXED_ANGLE_TRACKING_OFFSET_DISTANCE;
		m_position = m_entityToFollow->m_position - offset;
		m_orientation3D = EulerAngles(40.f, 30.f, 0.f);
	}
}
