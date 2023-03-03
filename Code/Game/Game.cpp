#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Job.hpp"
#include "Game/Game.hpp"
#include "Game/App.hpp"
#include "Game/Player.hpp"
#include "Game/World.hpp"
#include "Game/Block.hpp"
#include "Game/Entity.hpp"
#include "Game/GameCamera.hpp"

extern App* g_theApp;
extern Renderer* g_theRenderer;
extern InputSystem* g_theInput;
extern AudioSystem* g_theAudio;
extern Window* g_theWindow;
extern JobSystem* g_theJobSystem;

static float animationTimer = 0.f;

constexpr float worldCamFov = 60.f;
constexpr float worldCamNearZ = 0.1f;
constexpr float worldCamFarZ = 1000.f;
constexpr int numberOfGridLinesPerAxis = 200;


void Game::Startup()
{
	m_attractMode = true;
	InitializeAttractScreenDrawVertices();
	m_uiScreenSize.x = g_gameConfigBlackboard.GetValue("screenSizeWidth", m_uiScreenSize.x);
	m_uiScreenSize.y = g_gameConfigBlackboard.GetValue("screenSizeHeight", m_uiScreenSize.y);
	m_worldCamFov = g_gameConfigBlackboard.GetValue("worldCamFov", 60.f);
	m_worldCamNearZ = g_gameConfigBlackboard.GetValue("worldCamNearZ", 60.f);
	m_worldCamFarZ = g_gameConfigBlackboard.GetValue("worldCamFarZ", 60.f);
	m_worldCamera.SetViewToRenderTransform(Vec3(0.f, 0.f, 1.f), Vec3(-1.f, 0.f, 0.f), Vec3(0.f, 1.f, 0.f));
	m_stopwatch.Start(&g_theApp->GetGameClock(), 1.f);
	SubscribeEventCallbackFunction("controls", ControlsCommand);
	
	DebugAddWorldBasis(Mat44(), -1.f, Rgba8::WHITE, Rgba8::WHITE, DebugRenderMode::USEDEPTH);
	AddVertsForHelperBasis();
}

void Game::ShutDown()
{
	g_theJobSystem->CancelAllJobs();

	delete m_world;
	m_world = nullptr;
}

void Game::Update(float deltaSeconds)
{
	UNUSED(deltaSeconds);
	if (m_remakeGame)
	{
		g_theApp->RemakeGame();
		return;
	}
	if (m_spawnWorld)
	{
		SpawnWorld();
	}

	HandleMouseCursor();

	if (m_attractMode)
	{
		m_screenCamera.SetOrthoView(Vec2(0.f, 0.f), Vec2(m_uiScreenSize.x, m_uiScreenSize.y));
		HandleAttractModeInput();
		UpdateAttractMode(deltaSeconds);
	}
	else
	{
		m_worldCamera.SetPerspectiveView(g_theWindow->GetConfig().m_clientAspect, m_worldCamFov, m_worldCamNearZ, m_worldCamFarZ);
		HandleGameInput();
		//UpdateEntities(deltaSeconds);
		if(m_world)
			m_world->Update(deltaSeconds);
	}
}

void Game::Render() const
{
	if (m_attractMode)
	{
		g_theRenderer->BeginCamera(m_screenCamera);
		{
			g_theRenderer->ClearScreen(Rgba8::GREY);
			g_theRenderer->SetBlendMode(BlendMode::ALPHA);
			g_theRenderer->SetSamplerMode(SamplerMode::POINTCLAMP);
			g_theRenderer->SetRasterizerState(CullMode::NONE, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
			g_theRenderer->SetDepthStencilState(DepthTest::ALWAYS, false);
			RenderAttractScreen();
		}
		g_theRenderer->EndCamera(m_screenCamera);
	}
	else
	{
		g_theRenderer->SetSamplerMode(SamplerMode::BILINEARWRAP);
		g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
		g_theRenderer->SetDepthStencilState(DepthTest::LESSEQUAL, true);
		//RenderEntities();
		if (m_world)
		{
			m_world->Render();
		}
		DebugRenderWorld(m_worldCamera);
		DebugRender();

		g_theRenderer->BeginCamera(m_screenCamera);
		{
			if (m_world)
			{
				RenderInfoText();
			}
			DebugRenderScreen(m_screenCamera);
		}
		g_theRenderer->EndCamera(m_screenCamera);
	}

	g_theRenderer->BeginCamera(m_screenCamera);
	{
		//DebugDrawCursor();
		g_theRenderer->SetSamplerMode(SamplerMode::POINTCLAMP);
		g_theRenderer->SetRasterizerState(CullMode::NONE, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
		g_theRenderer->SetDepthStencilState(DepthTest::ALWAYS, false);
		g_theConsole->Render(AABB2(m_screenCamera.GetOrthoBottomLeft(), m_screenCamera.GetOrthoTopRight()));
	}
	g_theRenderer->EndCamera(m_screenCamera);
}

void Game::AddVertsForHelperBasis()
{
	std::vector<Vertex_PCU> xBasisVerts, yBasisVerts, zBasisVerts;
	AddVertsForArrow3D(xBasisVerts, Vec3(0.f, 0.f, 0.f), Vec3(0.3f, 0.f, 0.f), 0.05f, Rgba8::RED);
	AddVertsForArrow3D(yBasisVerts, Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.3f, 0.f), 0.05f, Rgba8::GREEN);
	AddVertsForArrow3D(zBasisVerts, Vec3(0.f, 0.f, 0.f), Vec3(0.f, 0.f, 0.3f), 0.05f, Rgba8::BLUE);

	for (int i = 0; i < xBasisVerts.size(); i++)
		m_debugHelperBasis.push_back(xBasisVerts[i]);
	for (int i = 0; i < yBasisVerts.size(); i++)
		m_debugHelperBasis.push_back(yBasisVerts[i]);
	for (int i = 0; i < zBasisVerts.size(); i++)
		m_debugHelperBasis.push_back(zBasisVerts[i]);
}

void Game::SpawnWorld()
{
	m_world = new World(this);
	m_spawnWorld = false;
}

void Game::HandleMouseCursor()
{
	bool gameWindowFocused = g_theWindow->HasFocus();
	if (gameWindowFocused)
	{
		if (g_theConsole->GetMode() == DevConsoleMode::OPEN_FULL)
		{
			g_theInput->SetMouseMode(false, false, false);
		}
		else
		{
			if (m_attractMode)
			{
				g_theInput->SetMouseMode(false, false, false);
			}
			else
			{
				g_theInput->SetMouseMode(true, true, true);
			}
		}
	}
}

void Game::HandleAttractModeInput()
{
	const XboxController& controller = g_theInput->GetController(0);

	if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE))
		g_theApp->HandleQuitRequest();
	if (g_theInput->WasKeyJustPressed(' ') || controller.WasButtonJustPressed(XBOX_BUTTON_START))
	{
		m_attractMode = false;
		m_spawnWorld = true;
	}
}

void Game::HandleGameInput()
{
	if (g_theInput->WasKeyJustPressed(KEYCODE_F1))
	{
		m_enableDebugDrawing = !m_enableDebugDrawing;
	}
	if (g_theInput->WasKeyJustPressed(KEYCODE_ESCAPE))
	{
		m_attractMode = true;
		m_remakeGame = true;
		m_stopwatch.Restart();
	}
}

void Game::InitializeAttractScreenDrawVertices()
{
	m_attractScreenDrawVertices[0].m_position = Vec3(-2.f, -2.f, 0.f);
	m_attractScreenDrawVertices[1].m_position = Vec3(0.f, 2.f, 0.f);
	m_attractScreenDrawVertices[2].m_position = Vec3(2.f, -2.f, 0.f);

	m_attractScreenDrawVertices[0].m_color = Rgba8(0, 255, 0, 255);
	m_attractScreenDrawVertices[1].m_color = Rgba8(0, 255, 0, 255);
	m_attractScreenDrawVertices[2].m_color = Rgba8(0, 255, 0, 255);
}

void Game::RenderAttractScreen() const
{
	Vertex_PCU tempCopyOfBlinkingTriangle[3];

	for (int i = 0; i < 3; i++)
	{
		tempCopyOfBlinkingTriangle[i].m_position = m_attractScreenDrawVertices[i].m_position;
		tempCopyOfBlinkingTriangle[i].m_color.r = m_attractScreenDrawVertices[i].m_color.r;
		tempCopyOfBlinkingTriangle[i].m_color.g = m_attractScreenDrawVertices[i].m_color.g;
		tempCopyOfBlinkingTriangle[i].m_color.b = m_attractScreenDrawVertices[i].m_color.b;
		tempCopyOfBlinkingTriangle[i].m_color.a = m_attractModeTriangleAlpha;
	}

	TransformVertexArrayXY3D(3, tempCopyOfBlinkingTriangle, 25.f, 0.f, Vec2(m_uiScreenSize.x * 0.5f, m_uiScreenSize.y * 0.5f));
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->DrawVertexArray(3, tempCopyOfBlinkingTriangle);
}

void Game::UpdateAttractMode(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	if (m_stopwatch.CheckDurationElapsedAndDecrement())
	{
		float temp = m_attractTriangleMinAlpha;
		m_attractTriangleMinAlpha = m_attractTriangleMaxAlpha;
		m_attractTriangleMaxAlpha = temp;
	}
	float alpha = Interpolate(m_attractTriangleMinAlpha, m_attractTriangleMaxAlpha, m_stopwatch.GetElapsedFraction());
	m_attractModeTriangleAlpha = (unsigned char)alpha;
}

void Game::DebugRender() const
{
	if (m_enableDebugDrawing)
	{
		Entity* player = m_world->GetPlayer();
		std::vector<Vertex_PCU> tempVerts;
		tempVerts = m_debugHelperBasis;
		//Mat44 basisTransform = m_player->GetModelMatrix();
		Mat44 translation = Mat44::CreateTranslation3D(player->m_position + (player->GetForwardVector() * 10.f));
		//translation.Append(basisTransform);
		TransformVertexArray3D((int)tempVerts.size(), tempVerts.data(), translation);
		g_theRenderer->SetRasterizerState(CullMode::BACK, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
		g_theRenderer->SetDepthStencilState(DepthTest::ALWAYS, true);
		g_theRenderer->SetModelColor(Rgba8::WHITE);
		g_theRenderer->BindTexture(nullptr);
		g_theRenderer->DrawVertexArray((int)tempVerts.size(), tempVerts.data());
	}
}

void Game::RenderInfoText() const
{
	Clock& clock = g_theApp->GetGameClock();
	Entity* player = m_world->GetPlayer();
	std::vector<Vertex_PCU> textVerts;
	std::string infoLine1 = Stringf("WASD = horizontal, QE = vertical, Space = fast, F1 = debug, BlockType = ");
	infoLine1.append(BlockDefintion::s_definitions[m_world->m_blockTypeToAdd].m_name);
	infoLine1.append("\n");
	std::string infoLine2 = Stringf("Chunks = %d, Vertices = %d, pos = (%.1f, %.1f, %.1f), orientation = (%.1f, %.1f, %.1f), time = %.2f, frames = %.1f ms (%.1f) \n", m_world->GetNumberOfChunks(), m_world->GetTotalNumberOfVerticesInChunks(),
		player->m_position.x, player->m_position.y, player->m_position.z, player->m_orientation3D.m_yawDegrees, player->m_orientation3D.m_pitchDegrees, player->m_orientation3D.m_rollDegrees,
		m_world->GetWorldTime(), clock.GetDeltaTime() * 1000.f, 1.f / clock.GetDeltaTime());

	std::string physicsMode;
	PHYSICS_MODE currentPhysicsMode = player->GetCurrentPhysicsMode();
	if (currentPhysicsMode == PHYSICS_MODE_FLYING)
		physicsMode = "Flying";
	else if (currentPhysicsMode == PHYSICS_MODE_NOCLIP)
		physicsMode = "No Clip";
	else
		physicsMode = "Walking";

	std::string cameraMode;
	CAMERA_MODE currentCameraMode = player->m_gameCamera->GetCurrentMode();
	if (currentCameraMode == CAMERA_MODE_FIRST_PERSON)
		cameraMode = "First Person";
	else if (currentCameraMode == CAMERA_MODE_FIXED_ANGLE_TRACKING)
		cameraMode = "Fixed Angle Tracking";
	else if (currentCameraMode == CAMERA_MODE_OVER_SHOULDER)
		cameraMode = "Over shoulder";
	else if (currentCameraMode == CAMERA_MODE_INDEPENDENT)
		cameraMode = "Independent";
	else
		cameraMode = "Spectator";

	std::string infoLine3 = Stringf("Physics = %s, Camera = %s \n", physicsMode.c_str(), cameraMode.c_str());
	
	infoLine1.append(infoLine2);
	infoLine1.append(infoLine3);
	constexpr float cellHeight = 20.f;
	Vec2 textBoxMins = Vec2(0, m_uiScreenSize.y - (cellHeight * 2.f));
	BitmapFont* font = g_theRenderer->CreateOrGetBitmapFont("Data/Fonts/MyFixedFont");
	font->AddVertsForTextInBox2D(textVerts, AABB2(textBoxMins, m_uiScreenSize - Vec2(50.f, 0.f)), cellHeight, infoLine1, Rgba8::WHITE, 1.f, Vec2(0.f, 1.f));
	g_theRenderer->SetRasterizerState(CullMode::NONE, FillMode::SOLID, WindingOrder::COUNTERCLOCKWISE);
	g_theRenderer->BindTexture(&font->GetTexture());
	g_theRenderer->DrawVertexArray((int)textVerts.size(), textVerts.data());
}

bool Game::ControlsCommand(EventArgs& args)
{
	UNUSED(args);
	g_theConsole->AddLine(g_theConsole->COMMAND, "--- controls ---");
	g_theConsole->AddLine(g_theConsole->COMMAND, "W/A/S/D/Left Joystick - Move");
	g_theConsole->AddLine(g_theConsole->COMMAND, "Q/E/LT/RT - Roll");
	g_theConsole->AddLine(g_theConsole->COMMAND, "Z/C/LB/RB - Up/Down");
	g_theConsole->AddLine(g_theConsole->COMMAND, "Mouse/Right Joystick - Aim");
	g_theConsole->AddLine(g_theConsole->COMMAND, "1 - Add debug wire sphere");
	g_theConsole->AddLine(g_theConsole->COMMAND, "2 - Add debug world line");
	g_theConsole->AddLine(g_theConsole->COMMAND, "3 - Add debug world basis");
	g_theConsole->AddLine(g_theConsole->COMMAND, "4 - Add debug world billboard text");
	g_theConsole->AddLine(g_theConsole->COMMAND, "5 - Add debug wire cylinder");
	g_theConsole->AddLine(g_theConsole->COMMAND, "6 - Add debug camera message");
	g_theConsole->AddLine(g_theConsole->COMMAND, "9 - Decrease debug clock speed");
	g_theConsole->AddLine(g_theConsole->COMMAND, "0 - Increase debug clock speed");
	g_theConsole->AddLine(g_theConsole->COMMAND, "~ - Open dev console");
	g_theConsole->AddLine(g_theConsole->COMMAND, "Escape- Exit");
	g_theConsole->AddLine(g_theConsole->COMMAND, "Space/Start - New game");
	g_theConsole->AddLine(g_theConsole->COMMAND, "T -  Slow Mo");
	g_theConsole->AddLine(g_theConsole->COMMAND, "O - Step frame");
	g_theConsole->AddLine(g_theConsole->COMMAND, "P - Toggle Pause");
	g_theConsole->AddLine(g_theConsole->COMMAND, "F8 - Hard Reset");

	return false;
}

bool operator<(const IntVec2& a, const IntVec2& b)
{
	if (a.y < b.y)
		return true;
	else if (a.y > b.y)
		return false;
	else
		return a.x < b.x;
}
