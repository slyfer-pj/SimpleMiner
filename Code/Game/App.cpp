#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Game/App.hpp"
#include "Game/Game.hpp"

Renderer* g_theRenderer = nullptr;
App* g_theApp = nullptr;
InputSystem* g_theInput = nullptr;
AudioSystem* g_theAudio = nullptr;
Window* g_theWindow = nullptr;
JobSystem* g_theJobSystem = nullptr;

void App::Startup()
{
	LoadGameConfigBlackboard();

	EventSystemConfig eventSystemConfig;
	g_theEventSystem = new EventSystem(eventSystemConfig);

	InputSystemConfig inputConfig;
	g_theInput = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_clientAspect = g_gameConfigBlackboard.GetValue("windowAspect", windowConfig.m_clientAspect);
	windowConfig.m_inputSystem = g_theInput;
	windowConfig.m_isFullscreen = g_gameConfigBlackboard.GetValue("isFullscreen", windowConfig.m_isFullscreen);
	windowConfig.m_windowTitle = g_gameConfigBlackboard.GetValue("windowTitle", windowConfig.m_windowTitle);
	g_theWindow = new Window(windowConfig);

	RendererConfig renderConfig;
	renderConfig.m_window = g_theWindow;
	g_theRenderer = new Renderer(renderConfig);

	DevConsoleConfig consoleConfig;
	consoleConfig.m_renderer = g_theRenderer;
	consoleConfig.m_fontFilePath = g_gameConfigBlackboard.GetValue("devconsoleDefaultFont", consoleConfig.m_fontFilePath);
	consoleConfig.m_fontAspect = g_gameConfigBlackboard.GetValue("devconsoleFontAspect", consoleConfig.m_fontAspect);
	consoleConfig.m_fontCellHeight = g_gameConfigBlackboard.GetValue("devconsoleTextHeight", consoleConfig.m_fontCellHeight);
	consoleConfig.m_maxLinesToPrint = g_gameConfigBlackboard.GetValue("devconsoleMaxLines", consoleConfig.m_maxLinesToPrint);
	consoleConfig.m_maxCommandHistory = g_gameConfigBlackboard.GetValue("devconsoleMaxCommandHistory", consoleConfig.m_maxCommandHistory);
	g_theConsole = new DevConsole(consoleConfig);

	AudioSystemConfig audioConfig;
	g_theAudio = new AudioSystem(audioConfig);

	JobSystemConfig jobSystemConfig;
	jobSystemConfig.m_numWorkerThreads = std::thread::hardware_concurrency();
	g_theJobSystem = new JobSystem(jobSystemConfig);

	g_theEventSystem->Startup();
	g_theInput->Startup();
	g_theWindow->Startup();
	g_theRenderer->Startup();
	g_theConsole->Startup();
	g_theAudio->Startup();
	g_theJobSystem->Startup();

	m_theGame = new Game();
	m_theGame->Startup();
}

void App::BeginFrame()
{	
	Clock::SystemBeginFrame();

	g_theEventSystem->BeginFrame();
	g_theInput->BeginFrame();
	g_theWindow->BeginFrame();
	g_theRenderer->BeginFrame();
	g_theConsole->BeginFrame();
	g_theAudio->BeginFrame();
	g_theJobSystem->BeginFrame();
}

void App::Update()
{
	HandleKeyboardInput();

	float deltaSeconds = static_cast<float>(m_gameClock.GetDeltaTime());
	if(m_theGame)
		m_theGame->Update(deltaSeconds);
}

void App::Render() const
{
	//g_theRenderer->ClearScreen(m_clearScreenColor);
	if (m_theGame)
		m_theGame->Render();
}

void App::EndFrame()
{
	g_theEventSystem->EndFrame();
	g_theInput->EndFrame();
	g_theWindow->EndFrame();
	g_theRenderer->EndFrame();
	g_theConsole->EndFrame();
	g_theAudio->EndFrame();
	g_theJobSystem->EndFrame();
}

void App::Shutdown()
{
	m_theGame->ShutDown();
	delete m_theGame;
	m_theGame = nullptr;

	g_theJobSystem->Shutdown();
	delete g_theJobSystem;
	g_theJobSystem = nullptr;

	g_theAudio->Shutdown();
	delete g_theAudio;
	g_theAudio = nullptr;

	g_theConsole->Shutdown();
	delete g_theConsole;
	g_theConsole = nullptr;

	g_theRenderer->Shutdown();
	delete g_theRenderer;
	g_theRenderer = nullptr;

	g_theWindow->Shutdown();
	delete g_theWindow;
	g_theWindow = nullptr;

	g_theInput->Shutdown();
	delete g_theInput;
	g_theInput = nullptr;

	g_theEventSystem->Shutdown();
	delete g_theEventSystem;
	g_theEventSystem = nullptr;
}

void App::RunFrame()
{
	BeginFrame();
	Update();
	Render();
	EndFrame();
}

void App::Run()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

void App::HandleKeyboardInput()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_theConsole->ToggleMode(DevConsoleMode::OPEN_FULL);
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_F8))
	{
		RemakeGame();
	}
	if (g_theConsole->GetMode() == DevConsoleMode::OPEN_FULL)
	{
		g_theInput->ClearKeyInput();
	}
	if (g_theInput->WasKeyJustPressed('P'))
	{
		m_gameClock.TogglePause();
	}
	if (g_theInput->WasKeyJustPressed('O'))
	{
		m_gameClock.StepFrame();
	}
	if (g_theInput->IsKeyDown('T'))
	{
		m_gameClock.SetTimeDilation(0.2f);
	}
	if (!g_theInput->IsKeyDown('T'))
	{
		m_gameClock.SetTimeDilation(1.f);
	}
}

void App::HandleQuitRequest()
{
	m_isQuitting = true;
}

void App::RemakeGame()
{
	m_theGame->ShutDown();
	delete m_theGame;
	m_theGame = nullptr;

	m_theGame = new Game();
	m_theGame->Startup();
}

void App::SetClearColor(const Rgba8& clearColor)
{
	m_clearScreenColor = clearColor;
}
