#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Game/Player.hpp"
#include "Game/Game.hpp"
#include "Game/World.hpp"
#include "Game/Entity.hpp"
#include "Game/App.hpp"

extern InputSystem* g_theInput;
extern App* g_theApp;

constexpr float moveSpeed = 20.f;
constexpr float mouseDeltaScale = 0.1f;
constexpr float sprintMultiplier = 8.f;
constexpr float joystickSensitivity = 2.f;
constexpr float rollSpeed = 1.5f;
constexpr float digCooldown = 0.1f;

Player::Player(World* world)
	:Controller(world)
{
	m_digCooldown.Start(&g_theApp->GetGameClock(), digCooldown);
}

void Player::Update(float deltaSeconds)
{
	Entity* owner = GetPossessedEntity();
	UNUSED(deltaSeconds);
	m_moveDirection = Vec3();
	HandleMouseInput();
	HandleKeyboardInput();

	float currentMoveSpeed = moveSpeed;
	if (m_isSprinting)
		currentMoveSpeed *= sprintMultiplier;

	owner->AddForces(m_moveDirection * currentMoveSpeed);
	owner->RotateEntity(m_rotationDelta);

	m_isSprinting = false;
}

void Player::HandleMouseInput()
{
	Vec2 mouseDelta = g_theInput->GetMouseClientDelta();
	//DebuggerPrintf("Mouse delta = %f %f \n", mouseDelta.x, mouseDelta.y);
	m_rotationDelta.m_yawDegrees = (mouseDelta.x * mouseDeltaScale);
	m_rotationDelta.m_pitchDegrees = (mouseDelta.y * mouseDeltaScale);

	if (g_theInput->IsKeyDown(KEYCODE_LEFT_MOUSE) && m_digCooldown.CheckDurationElapsedAndDecrement())
	{
		m_digCooldown.Restart();
		m_world->DigBlock();
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
	{
		m_world->AddBlock();
	}
}

void Player::HandleKeyboardInput()
{
	Entity* owner = GetPossessedEntity();
	Vec3 forwardNormal = owner->GetForwardVector();
	Vec3 leftNormal = owner->GetLeftVector();
	if (g_theInput->IsKeyDown('W'))
		m_moveDirection += Vec3(forwardNormal.x, forwardNormal.y, 0.f);
	if (g_theInput->IsKeyDown('S'))
		m_moveDirection -= Vec3(forwardNormal.x, forwardNormal.y, 0.f);
	if (g_theInput->IsKeyDown('A'))
		m_moveDirection += Vec3(leftNormal.x, leftNormal.y, 0.f);
	if (g_theInput->IsKeyDown('D'))
		m_moveDirection -= Vec3(leftNormal.x, leftNormal.y, 0.f);
	if (g_theInput->IsKeyDown('Q'))
		m_moveDirection += Vec3(0.f, 0.f, 1.f);
	if (g_theInput->IsKeyDown('E'))
		m_moveDirection += Vec3(0.f, 0.f, -1.f);
	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
		m_isSprinting = true;
	if (g_theInput->WasKeyJustPressed(KEYCODE_SPACE))
		owner->Jump();
}

