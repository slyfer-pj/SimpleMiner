#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Rgba8.hpp"

class AttractScreen;
class Game;
class App
{
public:
	App() {}
	~App() {}
	void Startup();
	void Run();
	void Shutdown();
	void HandleQuitRequest();
	void RemakeGame();

	bool IsQuitting() const { return m_isQuitting; }
	Clock& GetGameClock() { return m_gameClock; }
	void SetClearColor(const Rgba8& clearColor);

private:
	bool m_isQuitting = false;
	Game* m_theGame = nullptr;
	Clock m_gameClock;
	Rgba8 m_clearScreenColor = Rgba8::GREY;

private:
	void BeginFrame();
	void RunFrame();
	void Update();
	void Render() const;
	void EndFrame();
	void HandleKeyboardInput();
};