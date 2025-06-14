#include "Game/App.hpp"

#include "Game/GameCommon.hpp"
#include "Game/Chunk.hpp"
#include "Game/World.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/DevConsole.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Window.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"


App* g_app = nullptr;
AudioSystem* g_audio = nullptr;
RandomNumberGenerator* g_RNG = nullptr;
Renderer* g_renderer = nullptr;
Window* g_window = nullptr;
BitmapFont* g_squirrelFont = nullptr;
JobSystem* g_jobSystem = nullptr;

SpriteSheet* g_spritesheet = nullptr;

bool App::HandleQuitRequested(EventArgs& args)
{
	UNUSED(args);
	g_app->HandleQuitRequested();
	return true;
}

bool App::ShowControls(EventArgs& args)
{
	UNUSED(args);

	// Add controls to DevConsole

	return true;
}

App::App()
{

}

App::~App()
{
	delete g_renderer;
	g_renderer = nullptr;

	delete g_input;
	g_input = nullptr;

	delete g_audio;
	g_audio = nullptr;

	delete g_RNG;
	g_RNG = nullptr;

	delete m_game;
	m_game = nullptr;
}

void App::Startup()
{
	JobSystemConfig jobSystemConfig;
	jobSystemConfig.m_numWorkers = std::thread::hardware_concurrency() - 1;
	g_jobSystem = new JobSystem(jobSystemConfig);

	EventSystemConfig eventSystemConfig;
	g_eventSystem = new EventSystem(eventSystemConfig);

	InputConfig inputConfig;
	g_input = new InputSystem(inputConfig);

	WindowConfig windowConfig;
	windowConfig.m_inputSystem = g_input;
	windowConfig.m_windowTitle = "(snisal) SimpleMiner";
	//windowConfig.m_clientAspect = 2.f;
	windowConfig.m_isFullScreen = true;
	g_window = new Window(windowConfig);

	RenderConfig renderConfig;
	renderConfig.m_window = g_window;
	g_renderer = new Renderer(renderConfig);

	DevConsoleConfig devConsoleConfig;
	devConsoleConfig.m_renderer = g_renderer;
	devConsoleConfig.m_consoleFontFilePathWithNoExtension = "Data/Images/SquirrelFixedFont";
	m_devConsoleCamera.SetOrthoView(Vec2::ZERO, Vec2(2.f, 1.f));
	devConsoleConfig.m_camera = m_devConsoleCamera;
	g_console = new DevConsole(devConsoleConfig);

	AudioConfig audioConfig;
	g_audio = new AudioSystem(audioConfig);

	DebugRenderConfig debugRenderConfig;
	debugRenderConfig.m_renderer = g_renderer;
	debugRenderConfig.m_startVisible = false;
	debugRenderConfig.m_bitmapFontFilePathWithNoExtension = "Data/Images/SquirrelFixedFont";

	OpenXRConfig openXRConfig;
	openXRConfig.m_renderer = g_renderer;
	g_openXR = new OpenXR(openXRConfig);

	g_jobSystem->Startup();
	g_eventSystem->Startup();
	g_console->Startup();
	g_input->Startup();
	g_window->Startup();
	g_renderer->Startup();
	g_audio->Startup();
	DebugRenderSystemStartup(debugRenderConfig);
	g_openXR->Startup();

	m_game = new Game();

	InitializeCameras();

	SubscribeEventCallbackFunction("Quit", HandleQuitRequested, "Exits the application");
	SubscribeEventCallbackFunction("Controls", ShowControls, "Shows game controls");

	EventArgs emptyArgs;
	ShowControls(emptyArgs);

	LoadConfig();
}

void App::LoadConfig()
{
	XmlDocument gameConfigXmlFile("Data/Definitions/BlockDefinitions.xml");
	XmlResult fileLoadResult = gameConfigXmlFile.LoadFile("Data/Definitions/BlockDefinitions.xml");
	if (fileLoadResult == XmlResult::XML_SUCCESS)
	{
		XmlElement const* gameConfigXmlElement = gameConfigXmlFile.RootElement();
		g_gameConfigBlackboard.PopulateFromXmlElementAttributes(*gameConfigXmlElement);
	}

	g_activationRadius = g_gameConfigBlackboard.GetValue("activationRadius", g_activationRadius);
	g_deactivationRadius = g_activationRadius + CHUNK_SIZE_X + CHUNK_SIZE_Y;
}

void App::Run()
{
	while (!IsQuitting())
	{
		RunFrame();
	}
}

void App::RunFrame()
{
	BeginFrame();
	Update();
	
	m_currentEye = XREye::NONE;
	g_renderer->BeginRenderForEye(XREye::NONE);

	g_renderer->BeginRenderEvent("Screen to Texture");
	RenderScreen();
	g_renderer->EndRenderEvent("Screen to Texture");

	m_game->ClearScreen();
	g_renderer->BeginRenderEvent("Desktop Single View");
	Render();
	g_renderer->EndRenderEvent("Desktop Single View");

	if (g_openXR->IsInitialized())
	{
		m_currentEye = XREye::LEFT;
		g_renderer->BeginRenderForEye(XREye::LEFT);
		g_renderer->BeginRenderEvent("HMD Left Eye");
		m_game->ClearScreen();
		Render();
		g_renderer->EndRenderEvent("HMD Left Eye");

		m_currentEye = XREye::RIGHT;
		g_renderer->BeginRenderForEye(XREye::RIGHT);
		g_renderer->BeginRenderEvent("HMD Right Eye");
		m_game->ClearScreen();
		Render();
		g_renderer->EndRenderEvent("HMD Right Eye");
	}

	EndFrame();
}

bool App::HandleQuitRequested()
{
	m_isQuitting = true;

	return true;
}

void App::BeginFrame()
{
	Clock::TickSystemClock();

	m_frameRate = 1.f / Clock::GetSystemClock().GetDeltaSeconds();

	// Set cursor modes
	if (g_console->GetMode() != DevConsoleMode::HIDDEN || !g_window->HasFocus() || m_game->m_gameState == GameState::ATTRACT)
	{
		g_input->SetCursorMode(false, false);
	}
	else
	{
		g_input->SetCursorMode(true, true);
	}

	g_jobSystem->BeginFrame();
	g_eventSystem->BeginFrame();
	g_console->BeginFrame();
	g_input->BeginFrame();
	g_window->BeginFrame();
	g_renderer->BeginFrame();
	g_audio->BeginFrame();
	DebugRenderBeginFrame();
	g_openXR->BeginFrame();
}

void App::Update()
{
	m_game->Update();

	if (g_input->WasKeyJustPressed(KEYCODE_TILDE))
	{
		g_console->ToggleMode(DevConsoleMode::OPENFULL);
	}

	DebugAddScreenText(Stringf("%-15s Time: %.2f, Frames per Seconds: %.2f, Scale: %.2f", "System Clock:", Clock::GetSystemClock().GetTotalSeconds(), m_frameRate, Clock::GetSystemClock().GetTimeScale()), Vec2(SCREEN_SIZE_X - 16.f, SCREEN_SIZE_Y - 16.f), 16.f, Vec2(1.f, 1.f), 0.f);
	float gameFPS = 0.f;
	if (!m_game->m_gameClock.IsPaused())
	{
		gameFPS = 1.f / m_game->m_gameClock.GetDeltaSeconds();
	}
	DebugAddScreenText(Stringf("%-15s Time: %.2f, Frames per Seconds: %.2f, Scale: %.2f", "Game Clock:",  m_game->m_gameClock.GetTotalSeconds(), gameFPS, m_game->m_gameClock.GetTimeScale()), Vec2(SCREEN_SIZE_X - 16.f, SCREEN_SIZE_Y - 32.f), 16.f, Vec2(1.f, 1.f), 0.f);
}

void App::Render() const
{
	if (m_currentEye == XREye::NONE)
	{
		g_renderer->BeginCamera(m_worldCamera);
	}
	else if (m_currentEye == XREye::LEFT)
	{
		g_renderer->BeginCamera(m_leftEyeCamera);
	}
	else if (m_currentEye == XREye::RIGHT)
	{
		g_renderer->BeginCamera(m_rightEyeCamera);
	}

	// Render game
	m_game->Render();

	if (m_currentEye == XREye::NONE)
	{
		g_renderer->EndCamera(m_worldCamera);
		DebugRenderWorld(m_worldCamera);
	}
	else if (m_currentEye == XREye::LEFT)
	{
		g_renderer->EndCamera(m_leftEyeCamera);
		DebugRenderWorld(m_leftEyeCamera);
	}
	else if (m_currentEye == XREye::RIGHT)
	{
		g_renderer->EndCamera(m_rightEyeCamera);
		DebugRenderWorld(m_rightEyeCamera);
	}

	//g_console->Render(AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)));
}

extern double g_chunkMeshRebuildTime;
extern int g_numChunkMeshesRebuilt;
extern double g_worldUpdateTime;
extern double g_worldRenderTime;
extern double g_lightingProcessingTime;
extern double g_chunkActivationDeactivationDecisionTime;
extern double g_chunkRebuildDecisionTime;
extern double g_blockVertexesAddingTime;
extern double g_chunkActivationTime;
void App::RenderScreen() const
{
	g_renderer->BindTexture(nullptr);

	g_renderer->SetRTV(m_screenRTVTexture);
	g_renderer->ClearRTV(Rgba8::TRANSPARENT_BLACK, m_screenRTVTexture);

	g_renderer->BeginCamera(m_screenCamera);
	m_game->RenderScreen();
	g_renderer->EndCamera(m_screenCamera);

	DebugAddMessage(Stringf("World Update: %f ms", g_worldUpdateTime), 0.f, Rgba8::MAGENTA, Rgba8::MAGENTA);
	DebugAddMessage(Stringf("World Render: %f ms", g_worldRenderTime), 0.f, Rgba8::GREEN, Rgba8::GREEN);
	DebugAddMessage(Stringf("Chunk Mesh Rebuild Decision Time: %f ms", g_chunkRebuildDecisionTime), 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(Stringf("Chunk Mesh Rebuild Time: %f ms, Num Meshes Rebuilt: %d", g_chunkMeshRebuildTime, g_numChunkMeshesRebuilt), 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(Stringf("Lighting Processing: %f ms", g_lightingProcessingTime), 0.f, Rgba8::YELLOW, Rgba8::YELLOW);
	DebugAddMessage(Stringf("Chunk Activation/Deactivation Decision Time: %f ms", g_chunkActivationDeactivationDecisionTime), 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(Stringf("Chunk Activate Time: %f ms", g_chunkActivationTime), 0.f, Rgba8::RED, Rgba8::RED);
	DebugAddMessage(Stringf("AddVertsForBlock Total (per chunk): %f ms", g_blockVertexesAddingTime), 0.f, Rgba8::RED, Rgba8::RED);

	DebugRenderScreen(m_screenCamera);

	// Render Dev Console
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetModelConstants();
	g_renderer->BindShader(nullptr);
	g_console->Render(AABB2(Vec2(0.f, 0.f), Vec2(g_window->GetAspect(), 1.f)));


	g_renderer->SetRTV();
}

void App::EndFrame()
{
	g_openXR->EndFrame();
	DebugRenderEndFrame();
	g_audio->EndFrame();
	g_renderer->EndFrame();
	g_window->EndFrame();
	g_input->EndFrame();
	g_console->EndFrame();
	g_eventSystem->EndFrame();
	g_jobSystem->EndFrame();
}

void App::Shutdown()
{
	g_openXR->Shutdown();
	DebugRenderSystemShutdown();
	g_audio->Shutdown();
	g_renderer->Shutdown();
	g_input->Shutdown();
	g_window->Shutdown();
	g_console->Shutdown();
	g_eventSystem->Shutdown();
	g_jobSystem->Shutdown();
}

void App::InitializeCameras()
{
	m_worldCamera.SetRenderBasis(Vec3::SKYWARD, Vec3::WEST, Vec3::NORTH);
	m_worldCamera.SetPerspectiveView(g_window->GetAspect(), 60.f, 0.01f, 10000.f);
	m_worldCamera.SetTransform(Vec3::ZERO, EulerAngles::ZERO);

	m_screenCamera.SetOrthoView(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
	m_screenCamera.SetViewport(Vec2::ZERO, Vec2(SCREEN_SIZE_Y * g_window->GetAspect(), SCREEN_SIZE_Y));
	m_screenRTVTexture = g_renderer->CreateRenderTargetTexture("ScreenTexture", IntVec2(int(SCREEN_SIZE_Y * g_window->GetAspect()), int(SCREEN_SIZE_Y)));

	m_leftEyeCamera.SetRenderBasis(Vec3::GROUNDWARD, Vec3::WEST, Vec3::NORTH);
	m_rightEyeCamera.SetRenderBasis(Vec3::GROUNDWARD, Vec3::WEST, Vec3::NORTH);
}

XREye App::GetCurrentEye() const
{
	return m_currentEye;
}

Camera const App::GetCurrentCamera() const
{
	if (m_currentEye == XREye::LEFT)
	{
		return m_leftEyeCamera;
	}

	if (m_currentEye == XREye::RIGHT)
	{
		return m_rightEyeCamera;
	}

	return m_worldCamera;
}

