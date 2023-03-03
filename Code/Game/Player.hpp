#pragma once
#include "Game/Controller.hpp"
#include "Engine/Core/Stopwatch.hpp"

class World;

class Player : public Controller
{
public:
	Player() = default;
	Player(World* world);
	virtual void Update(float deltaSeconds) override;

private:
	void HandleMouseInput();
	void HandleKeyboardInput();

private:
	Vec3 m_moveDirection;
	EulerAngles m_rotationDelta;
	bool m_isSprinting = false;
	Stopwatch m_digCooldown;
};