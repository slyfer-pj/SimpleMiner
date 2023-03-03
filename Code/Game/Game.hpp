#pragma once
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Core/Stopwatch.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EventSystem.hpp"


class Entity;
class Player;
class World;
struct IntVec2;

bool operator<(const IntVec2& a, const IntVec2& b);

class Game 
{
public:
	Game() {};
	void Startup();
	void Update(float deltaSeconds);
	void Render() const;
	void ShutDown();
	static bool ControlsCommand(EventArgs& args);

public:
	bool m_enableDebugDrawing = false;
	Vec2 m_uiScreenSize = Vec2(500.f, 300.f);
	Camera m_worldCamera;
	Camera m_screenCamera;

private:
	Vertex_PCU m_attractScreenDrawVertices[3];
	bool m_attractMode = false;
	unsigned char m_attractModeTriangleAlpha = 0;
	Stopwatch m_stopwatch;
	float m_attractTriangleMinAlpha = 50.f;
	float m_attractTriangleMaxAlpha = 255.f;
	World* m_world = nullptr;
	bool m_remakeGame = false;
	std::vector<Vertex_PCU> m_debugHelperBasis;
	bool m_spawnWorld = false;
	float m_worldCamFov = 60.f;
	float m_worldCamNearZ = 0.1f;
	float m_worldCamFarZ = 100.f;
	
private:
	void HandleMouseCursor();
	void HandleAttractModeInput();
	void HandleGameInput();
	void InitializeAttractScreenDrawVertices();
	void UpdateAttractMode(float deltaSeconds);
	void RenderAttractScreen() const;
	void DebugRender() const;
	void RenderInfoText() const;
	void AddVertsForHelperBasis();
	void SpawnWorld();
};