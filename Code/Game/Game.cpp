#include "Game/Game.hpp"

#include "Game/App.hpp"
#include "Game/GameCommon.hpp"
#include "Game/World.hpp"
#include "Game/BlockDefinition.hpp"
#include "Game/Chunk.hpp"
#include "Game/Block.hpp"
#include "Game/BlockIter.hpp"
#include "Game/BlockTemplate.hpp"

#include "Engine/Core/DevConsole.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/SpriteAnimDefinition.hpp"
#include "Engine/Renderer/SpriteDefinition.hpp"
#include "Engine/Renderer/Spritesheet.hpp"
#include "Engine/VirtualReality/OpenXR.hpp"

double g_chunkMeshRebuildTime = 0.f;
int g_numChunkMeshesRebuilt = 0;
double g_worldUpdateTime = 0.f;
double g_worldRenderTime = 0.f;
double g_lightingProcessingTime = 0.f;
double g_chunkActivationDeactivationDecisionTime = 0.f;
double g_chunkRebuildDecisionTime = 0.f;
double g_blockVertexesAddingTime = 0.f;
double g_chunkActivationTime = 0.f;

bool Game::Event_GameClock(EventArgs& args)
{
	bool isHelp = args.GetValue("help", false);
	if (isHelp)
	{
		g_console->AddLine("Modifies settings for the game clock", false);
		g_console->AddLine("Parameters", false);
		g_console->AddLine(Stringf("\t\t%-20s: pauses the game clock", "pause"), false);
		g_console->AddLine(Stringf("\t\t%-20s: resumes the game clock", "unpause"), false);
		g_console->AddLine(Stringf("\t\t%-20s: [float >= 0.f] changes the game clock timescale to the specified value", "timescale"), false);
	}

	bool pause = args.GetValue("pause", false);
	if (pause)
	{
		g_app->m_game->m_gameClock.Pause();
	}

	bool unpause = args.GetValue("unpause", false);
	if (unpause)
	{
		g_app->m_game->m_gameClock.Unpause();
	}

	float timeScale = args.GetValue("timescale", -1.f);
	if (timeScale >= 0.f)
	{
		g_app->m_game->m_gameClock.SetTimeScale(timeScale);
	}

	return true;
}

Game::Game()
{
	LoadAssets();
	BlockTemplate::InitializeBlockTemplates();
	SubscribeEventCallbackFunction("Gameclock", Event_GameClock, "Modifies settings for the game clock");
}

Game::~Game()
{
	delete m_world;
	m_world = nullptr;
}

void Game::LoadAssets()
{
	Texture* spritesTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/BasicSprites_64x64.png");
	g_spritesheet = new SpriteSheet(spritesTexture, IntVec2(64, 64));
	g_squirrelFont = g_renderer->CreateOrGetBitmapFont("Data/Images/SquirrelFixedFont");
	if (m_gameState == GameState::INTRO)
	{
		m_logoTexture = g_renderer->CreateOrGetTextureFromFile("Data/Images/Logo.png");
	}
	BlockDefinition::InitializeBlockDefinitions();
}

void Game::Update()
{
	float deltaSeconds = m_gameClock.GetDeltaSeconds();

	switch (m_gameState)
	{
		case GameState::INTRO:					UpdateIntroScreen(deltaSeconds);				break;
		case GameState::ATTRACT:				UpdateAttractScreen(deltaSeconds);				break;
		case GameState::GAME:					UpdateGame(deltaSeconds);						break;
	}

	if (g_openXR && g_openXR->IsInitialized())
	{
		EulerAngles leftEyeOrientation;
		g_openXR->GetTransformForEye_iFwd_jLeft_kUp(XREye::LEFT, m_leftEyeLocalPosition, leftEyeOrientation);

		EulerAngles rightEyeOrientation;
		g_openXR->GetTransformForEye_iFwd_jLeft_kUp(XREye::RIGHT, m_rightEyeLocalPosition, rightEyeOrientation);

		m_hmdOrientation = rightEyeOrientation;
	}

	Mat44 billboardTargetMatrix = Mat44::CreateTranslation3D(m_cameraPosition);
	billboardTargetMatrix.Append((m_cameraOrientation + m_hmdOrientation).GetAsMatrix_iFwd_jLeft_kUp());

	m_screenBillboardMatrix = GetBillboardMatrix(BillboardType::FULL_FACING, billboardTargetMatrix, m_cameraPosition + (m_cameraOrientation + m_hmdOrientation).GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D() * SCREEN_QUAD_DISTANCE);
}

void Game::UpdateGame(float deltaSeconds)
{
	HandleDeveloperCheats();

	if (m_world)
	{
		g_numChunkMeshesRebuilt = 0;
		m_world->Update();
		UpdateInput();
	}
	UpdateCameras(deltaSeconds);

	m_timeInState += deltaSeconds;
}


void Game::HandleDeveloperCheats()
{
	XboxController xboxController = g_input->GetController(0);
	
	if (g_input->IsKeyDown('O'))
	{
		m_gameClock.StepSingleFrame();
	}

	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		QuitToAttractScreen();
	}
	if (g_input->WasKeyJustPressed('T'))
	{
		m_gameClock.SetTimeScale(0.1f);
	}
	if (g_input->WasKeyJustReleased('T'))
	{
		m_gameClock.SetTimeScale(1.f);
	}

	if (g_input->WasKeyJustPressed('P'))
	{
		m_gameClock.TogglePause();
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F1))
	{
		m_drawDebug = !m_drawDebug;
	}
}

void Game::UpdateEntities()
{
}

void Game::UpdateInput()
{
	UpdateKeyboardInput();
	if (g_openXR && g_openXR->IsInitialized())
	{
		UpdateVRInput();
	}
}

void Game::UpdateKeyboardInput()
{
	constexpr float CAMERA_DEFAULT_MOVESPEED = 4.f;
	constexpr float CAMERA_SPRINT_FACTOR = 10.f;
	constexpr float CAMERA_TURNRATE = 0.075f;

	float deltaSeconds = m_gameClock.GetDeltaSeconds();
	float movementSpeed = CAMERA_DEFAULT_MOVESPEED;

	if (g_input->IsShiftHeld())
	{
		movementSpeed *= CAMERA_SPRINT_FACTOR;
	}

	Vec3 cameraFwd;
	Vec3 cameraLeft;
	Vec3 cameraUp;
	(m_cameraOrientation + m_hmdOrientation).GetAsVectors_iFwd_jLeft_kUp(cameraFwd, cameraLeft, cameraUp);

	Vec3 cameraFwd2D = cameraFwd.GetXY().ToVec3().GetNormalized();
	Vec3 cameraLeft2D = cameraLeft.GetXY().ToVec3().GetNormalized();

	if (g_input->WasKeyJustPressed(KEYCODE_F8))
	{
		delete m_world;
		m_world = new World(this);
	}

	if (g_input->IsKeyDown('W'))
	{
		m_cameraPosition += cameraFwd2D * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('A'))
	{
		m_cameraPosition += cameraLeft2D * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('S'))
	{
		m_cameraPosition -= cameraFwd2D * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('D'))
	{
		m_cameraPosition -= cameraLeft2D * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('Q'))
	{
		m_cameraPosition += Vec3::GROUNDWARD * movementSpeed * deltaSeconds;
	}
	if (g_input->IsKeyDown('E'))
	{
		m_cameraPosition += Vec3::SKYWARD * movementSpeed * deltaSeconds;
	}
	if (g_input->WasKeyJustPressed('Y'))
	{
		m_world->m_worldTimeScale = 1000.f;
	}
	if (g_input->WasKeyJustReleased('Y'))
	{
		m_world->m_worldTimeScale = 200.f;
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F2))
	{
		m_world->m_disableLightning = !m_world->m_disableLightning;
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F3))
	{
		m_world->m_isWorldTimeFixedToDay = !m_world->m_isWorldTimeFixedToDay;
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F4))
	{
		m_world->m_disableLighting = !m_world->m_disableLighting;
	}
	if (g_input->WasKeyJustPressed(KEYCODE_F6))
	{
		m_world->m_disableWorldShader = !m_world->m_disableWorldShader;
	}

	//m_cameraPosition.z = GetClamped(m_cameraPosition.z, 0.f, (float)CHUNK_SIZE_Z);

	m_cameraOrientation.m_yawDegrees += (float)g_input->GetCursorClientDelta().x * CAMERA_TURNRATE;
	m_cameraOrientation.m_pitchDegrees -= (float)g_input->GetCursorClientDelta().y * CAMERA_TURNRATE;
	m_cameraOrientation.m_pitchDegrees = GetClamped(m_cameraOrientation.m_pitchDegrees, -89.f, 89.f);
	m_cameraOrientation.GetAsVectors_iFwd_jLeft_kUp(cameraFwd, cameraLeft, cameraUp);

	if (!m_isRaycastLocked)
	{
		m_raycastStartPosition = m_cameraPosition;
		m_raycastDirection = cameraFwd;
	}

	m_raycastResult = m_world->RaycastVsBlocks(m_raycastStartPosition, m_raycastDirection, 10.f);

	if (m_isRaycastLocked)
	{
		Rgba8 raycastColor = Rgba8::GRAY;

		if (!m_raycastResult.m_didImpact)
		{
			raycastColor = Rgba8::GREEN;
		}
		DebugAddWorldArrow(m_raycastStartPosition, m_raycastStartPosition + m_raycastDirection * 10.f, 0.01f, 0.f, raycastColor, raycastColor, DebugRenderMode::X_RAY);

		if (m_raycastResult.m_didImpact)
		{
			DebugAddWorldPoint(m_raycastResult.m_impactPosition, 0.05f, 0.f, Rgba8::WHITE, Rgba8::WHITE, DebugRenderMode::X_RAY);
			DebugAddWorldArrow(m_raycastResult.m_impactPosition, m_raycastResult.m_impactPosition + m_raycastResult.m_impactNormal * 0.5f, 0.01f, 0.f, Rgba8::YELLOW, Rgba8::YELLOW, DebugRenderMode::X_RAY);
			if (m_isRaycastLocked)
			{
				DebugAddWorldArrow(m_raycastStartPosition, m_raycastStartPosition + m_raycastDirection * 10.f, 0.01f, 0.f, Rgba8::GRAY, Rgba8::GRAY, DebugRenderMode::X_RAY);
				DebugAddWorldArrow(m_raycastStartPosition, m_raycastResult.m_impactPosition, 0.01f, 0.f, Rgba8::RED, Rgba8::RED, DebugRenderMode::X_RAY);
			}
		}
	}

	if (m_raycastResult.m_didImpact)
	{
		m_blockPlacingPosition = m_raycastResult.m_impactPosition + m_raycastResult.m_impactNormal * 0.5f;
		m_blockDiggingPosition = m_raycastResult.m_impactPosition - m_raycastResult.m_impactNormal * 0.5f;
	}

	if (g_input->WasKeyJustPressed(KEYCODE_RMB))
	{
		if (!m_raycastResult.m_didImpact)
		{
			return;
		}

		Chunk* chunk = m_world->GetChunkForWorldPosition(m_blockPlacingPosition);
		chunk->AddBlockAtWorldPosition(m_blockPlacingPosition, m_selectedBlockType);
	}
	if (g_input->WasKeyJustPressed(KEYCODE_LMB))
	{
		if (!m_raycastResult.m_didImpact)
		{
			return;
		}

		Chunk* chunk = m_world->GetChunkForWorldPosition(m_blockDiggingPosition);
		chunk->DigBlockAtWorldPosition(m_blockDiggingPosition);
	}

	if (g_input->WasKeyJustPressed('R'))
	{
		m_isRaycastLocked = !m_isRaycastLocked;
	}

	if (g_input->WasKeyJustPressed('1'))
	{
		m_selectedBlockType = 1;
	}
	if (g_input->WasKeyJustPressed('2'))
	{
		m_selectedBlockType = 2;
	}
	if (g_input->WasKeyJustPressed('3'))
	{
		m_selectedBlockType = 3;
	}
	if (g_input->WasKeyJustPressed('4'))
	{
		m_selectedBlockType = 4;
	}
	if (g_input->WasKeyJustPressed('5'))
	{
		m_selectedBlockType = 5;
	}
	if (g_input->WasKeyJustPressed('6'))
	{
		m_selectedBlockType = 6;
	}
	if (g_input->WasKeyJustPressed('7'))
	{
		m_selectedBlockType = 7;
	}
	if (g_input->WasKeyJustPressed('8'))
	{
		m_selectedBlockType = 8;
	}
	if (g_input->WasKeyJustPressed('9'))
	{
		m_selectedBlockType = 9;
	}

	if (!g_openXR || !g_openXR->IsInitialized())
	{
		DebugAddWorldArrow(m_cameraPosition + cameraFwd, m_cameraPosition + cameraFwd + Vec3::EAST * 0.05f, 0.003f, 0.f, Rgba8::RED, Rgba8::RED, DebugRenderMode::ALWAYS);
		DebugAddWorldArrow(m_cameraPosition + cameraFwd, m_cameraPosition + cameraFwd + Vec3::NORTH * 0.05f, 0.003f, 0.f, Rgba8::GREEN, Rgba8::GREEN, DebugRenderMode::ALWAYS);
		DebugAddWorldArrow(m_cameraPosition + cameraFwd, m_cameraPosition + cameraFwd + Vec3::SKYWARD * 0.05f, 0.003f, 0.f, Rgba8::BLUE, Rgba8::BLUE, DebugRenderMode::ALWAYS);
		DebugAddWorldPoint(m_cameraPosition + cameraFwd, 0.003f, 0.f, Rgba8::WHITE, Rgba8::WHITE, DebugRenderMode::ALWAYS);
	}

	DebugAddMessage(Stringf("Selected Block: %s", BlockDefinition::s_blockDefs[m_selectedBlockType].m_name.c_str()), 0.f, Rgba8::MAGENTA, Rgba8::MAGENTA);
	DebugAddMessage(Stringf("Chunks: %d; Vertexes: %d", (int)m_world->m_activeChunks.size(), m_world->m_totalRenderedVerts), 0.f, Rgba8::CYAN, Rgba8::CYAN);
	DebugAddMessage("T = Slow; F8 = Recreate world", 0.f, Rgba8::YELLOW, Rgba8::YELLOW);
	DebugAddMessage("WASD = Move in XY plane; QE = Groundward/Skyward; Shift (Hold) = Sprint", 0.f, Rgba8::YELLOW, Rgba8::WHITE);

	DebugAddScreenText(Stringf("World Time: %.2f", m_world->m_worldTime), Vec2(SCREEN_SIZE_X - 16.f, SCREEN_SIZE_Y - 48.f), 16.f, Vec2(1.f, 1.f), 0.f);
}

void Game::UpdateVRInput()
{
	VRController const& leftController = g_openXR->GetLeftController();
	VRController const& rightController = g_openXR->GetRightController();

	constexpr float CAMERA_DEFAULT_MOVESPEED = 4.f;
	constexpr float CAMERA_SPRINT_FACTOR = 10.f;
	constexpr float CAMERA_TURNRATE = 90.f;

	float deltaSeconds = m_gameClock.GetDeltaSeconds();
	float movementSpeed = CAMERA_DEFAULT_MOVESPEED;

	if (leftController.GetTrigger())
	{
		movementSpeed *= CAMERA_SPRINT_FACTOR;
	}

	Vec3 cameraFwd;
	Vec3 cameraLeft;
	Vec3 cameraUp;
	(m_cameraOrientation + m_hmdOrientation).GetAsVectors_iFwd_jLeft_kUp(cameraFwd, cameraLeft, cameraUp);

	Vec3 cameraFwd2D = cameraFwd.GetXY().ToVec3().GetNormalized();
	Vec3 cameraLeft2D = cameraLeft.GetXY().ToVec3().GetNormalized();

	Vec2 leftJoystickPosition = leftController.GetJoystick().GetPosition();
	m_cameraPosition += (-leftJoystickPosition.x * cameraLeft2D * movementSpeed * deltaSeconds) + (leftJoystickPosition.y * cameraFwd2D * movementSpeed * deltaSeconds);

	if (leftController.IsJoystickPressed())
	{
		m_cameraPosition += Vec3::GROUNDWARD * movementSpeed * deltaSeconds;
	}
	if (rightController.IsJoystickPressed())
	{
		m_cameraPosition += Vec3::SKYWARD * movementSpeed * deltaSeconds;
	}

	Vec2 rightJoystickPosition = rightController.GetJoystick().GetPosition();
	m_cameraOrientation.m_yawDegrees -= rightJoystickPosition.x * CAMERA_TURNRATE * deltaSeconds;

	Mat44 playerModelMatrix = Mat44::CreateTranslation3D(m_cameraPosition);
	playerModelMatrix.Append(m_cameraOrientation.GetAsMatrix_iFwd_jLeft_kUp());

	Vec3 const& rightControllerPosition = playerModelMatrix.TransformPosition3D(rightController.GetPosition_iFwd_jLeft_kUp());
	Vec3 const& rightControllerFwd = playerModelMatrix.TransformVectorQuantity3D(rightController.GetOrientation_iFwd_jLeft_kUp().GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D());
	Vec3 const& rightControllerLeft = playerModelMatrix.TransformVectorQuantity3D(rightController.GetOrientation_iFwd_jLeft_kUp().GetAsMatrix_iFwd_jLeft_kUp().GetJBasis3D());
	Vec3 const& rightControllerUp = playerModelMatrix.TransformVectorQuantity3D(rightController.GetOrientation_iFwd_jLeft_kUp().GetAsMatrix_iFwd_jLeft_kUp().GetKBasis3D());


	if (!m_isRaycastLocked)
	{
		m_raycastStartPosition = rightControllerPosition;
		m_raycastDirection = rightControllerFwd;
	}

	m_raycastResult = m_world->RaycastVsBlocks(m_raycastStartPosition, m_raycastDirection, 10.f);

	if (m_isRaycastLocked)
	{
		Rgba8 raycastColor = Rgba8::GRAY;

		if (!m_raycastResult.m_didImpact)
		{
			raycastColor = Rgba8::GREEN;
		}
		DebugAddWorldArrow(m_raycastStartPosition, m_raycastStartPosition + m_raycastDirection * 10.f, 0.01f, 0.f, raycastColor, raycastColor, DebugRenderMode::X_RAY);

		if (m_raycastResult.m_didImpact)
		{
			DebugAddWorldPoint(m_raycastResult.m_impactPosition, 0.05f, 0.f, Rgba8::WHITE, Rgba8::WHITE, DebugRenderMode::X_RAY);
			DebugAddWorldArrow(m_raycastResult.m_impactPosition, m_raycastResult.m_impactPosition + m_raycastResult.m_impactNormal * 0.5f, 0.01f, 0.f, Rgba8::YELLOW, Rgba8::YELLOW, DebugRenderMode::X_RAY);
			if (m_isRaycastLocked)
			{
				DebugAddWorldArrow(m_raycastStartPosition, m_raycastStartPosition + m_raycastDirection * 10.f, 0.01f, 0.f, Rgba8::GRAY, Rgba8::GRAY, DebugRenderMode::X_RAY);
				DebugAddWorldArrow(m_raycastStartPosition, m_raycastResult.m_impactPosition, 0.01f, 0.f, Rgba8::RED, Rgba8::RED, DebugRenderMode::X_RAY);
			}
		}
	}

	if (m_raycastResult.m_didImpact)
	{
		m_blockPlacingPosition = m_raycastResult.m_impactPosition + m_raycastResult.m_impactNormal * 0.5f;
		m_blockDiggingPosition = m_raycastResult.m_impactPosition - m_raycastResult.m_impactNormal * 0.5f;
	}

	if (rightController.WasTriggerJustPressed())
	{
		if (!m_raycastResult.m_didImpact)
		{
			return;
		}

		Chunk* chunk = m_world->GetChunkForWorldPosition(m_blockPlacingPosition);
		chunk->AddBlockAtWorldPosition(m_blockPlacingPosition, m_selectedBlockType);
	}
	if (rightController.WasGripJustPressed())
	{
		if (!m_raycastResult.m_didImpact)
		{
			return;
		}

		Chunk* chunk = m_world->GetChunkForWorldPosition(m_blockDiggingPosition);
		chunk->DigBlockAtWorldPosition(m_blockDiggingPosition);
	}
	if (rightController.WasSelectButtonJustPressed())
	{
		m_selectedBlockType++;
		if (m_selectedBlockType >= (int)BlockDefinition::s_blockDefs.size())
		{
			m_selectedBlockType = 0;
		}
	}
	if (rightController.WasBackButtonJustPressed())
	{
		m_selectedBlockType--;
		if (m_selectedBlockType < 0)
		{
			m_selectedBlockType = BlockDefinitionID((int)BlockDefinition::s_blockDefs.size() - 1);
		}
	}

	DebugAddWorldArrow(rightControllerPosition, rightControllerPosition + rightControllerFwd * 0.05f, 0.003f, 0.f, Rgba8::RED, Rgba8::RED, DebugRenderMode::ALWAYS);
	DebugAddWorldArrow(rightControllerPosition, rightControllerPosition + rightControllerLeft * 0.05f, 0.003f, 0.f, Rgba8::GREEN, Rgba8::GREEN, DebugRenderMode::ALWAYS);
	DebugAddWorldArrow(rightControllerPosition, rightControllerPosition + rightControllerUp * 0.05f, 0.003f, 0.f, Rgba8::BLUE, Rgba8::BLUE, DebugRenderMode::ALWAYS);
	DebugAddWorldPoint(rightControllerPosition, 0.003f, 0.f, Rgba8::WHITE, Rgba8::WHITE, DebugRenderMode::ALWAYS);
}

void Game::UpdateCameras(float deltaSeconds)
{
	UNUSED(deltaSeconds);

	if (g_openXR && g_openXR->IsInitialized())
	{
		Mat44 playerModelMatrix = Mat44::CreateTranslation3D(m_cameraPosition);
		playerModelMatrix.Append(m_cameraOrientation.GetAsMatrix_iFwd_jLeft_kUp());

		constexpr float XR_NEAR = 0.1f;
		constexpr float XR_FAR = 1000.f;
		float lFovLeft, lFovRight, lFovUp, lFovDown;
		float rFovLeft, rFovRight, rFovUp, rFovDown;
		Vec3 leftEyePosition, rightEyePosition;
		EulerAngles leftEyeOrientation, rightEyeOrientation;

		g_openXR->GetFovsForEye(XREye::LEFT, lFovLeft, lFovRight, lFovUp, lFovDown);
		g_app->m_leftEyeCamera.SetXRView(lFovLeft, lFovRight, lFovUp, lFovDown, XR_NEAR, XR_FAR);
		Mat44 leftEyeTransform = playerModelMatrix;
		leftEyeTransform.AppendTranslation3D(m_leftEyeLocalPosition);
		leftEyeTransform.Append(m_hmdOrientation.GetAsMatrix_iFwd_jLeft_kUp());
		g_app->m_leftEyeCamera.SetTransform(leftEyeTransform);

		g_openXR->GetFovsForEye(XREye::RIGHT, rFovLeft, rFovRight, rFovUp, rFovDown);
		g_app->m_rightEyeCamera.SetXRView(rFovLeft, rFovRight, rFovUp, rFovDown, XR_NEAR, XR_FAR);
		Mat44 rightEyeTransform = playerModelMatrix;
		rightEyeTransform.AppendTranslation3D(m_rightEyeLocalPosition);
		rightEyeTransform.Append(m_hmdOrientation.GetAsMatrix_iFwd_jLeft_kUp());
		g_app->m_rightEyeCamera.SetTransform(rightEyeTransform);
	}

	g_app->m_worldCamera.SetTransform(m_cameraPosition, m_cameraOrientation + m_hmdOrientation);
}

void Game::RenderWorldScreenQuad() const
{
	g_renderer->BeginRenderEvent("World Screen Quad");
	{
		// Math for rendering screen quad
		XREye currentEye = g_app->GetCurrentEye();
		float quadHeight = SCREEN_QUAD_DISTANCE * TanDegrees(30.f) * (currentEye == XREye::NONE ? 1.f : 0.5f);
		float quadWidth = quadHeight * g_window->GetAspect();

		// Render screen quad
		std::vector<Vertex_PCU> screenVerts;
		AddVertsForQuad3D(screenVerts, Vec3(0.f, quadWidth, -quadHeight), Vec3(0.f, -quadWidth, -quadHeight), Vec3(0.f, -quadWidth, quadHeight), Vec3(0.f, quadWidth, quadHeight), Rgba8::WHITE, AABB2(Vec2(1.f, 1.f), Vec2(0.f, 0.f)));
		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::DISABLED);
		g_renderer->SetModelConstants(m_screenBillboardMatrix);
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(g_app->m_screenRTVTexture);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexArray(screenVerts);
	}
	g_renderer->EndRenderEvent("World Screen Quad");
}

void Game::Render() const
{
	switch (m_gameState)
	{
		//case GameState::INTRO:				RenderIntroScreen();					break;
		//case GameState::ATTRACT:			RenderAttractScreen();					break;
		case GameState::GAME:				RenderGame();							break;
	}

	RenderWorldScreenQuad();
}

void Game::ClearScreen() const
{
	switch (m_gameState)
	{
		case GameState::INTRO:
		case GameState::ATTRACT:
		{
			g_renderer->ClearScreen(Rgba8::BLACK);
			break;
		}
		case GameState::GAME:
		{
			g_renderer->ClearScreen(m_world->m_skyColor);
			break;
		}
	}
}

void Game::RenderScreen() const
{
	if (m_gameState == GameState::ATTRACT)
	{
		RenderAttractScreen();
	}
}

void Game::RenderGame() const
{
	m_world->Render();


	if (g_openXR && g_openXR->IsInitialized())
	{
		std::vector<Vertex_PCU> rayVerts;
		VRController rightController = g_openXR->GetRightController();
		Mat44 playerModelMatrix = Mat44::CreateTranslation3D(m_cameraPosition);
		playerModelMatrix.Append(m_cameraOrientation.GetAsMatrix_iFwd_jLeft_kUp());
		Vec3 rightControllerPosition = playerModelMatrix.TransformPosition3D(rightController.GetPosition_iFwd_jLeft_kUp());
		Vec3 rightControllerFwd = playerModelMatrix.TransformVectorQuantity3D(rightController.GetOrientation_iFwd_jLeft_kUp().GetAsMatrix_iFwd_jLeft_kUp().GetIBasis3D());
		AddVertsForGradientLineSegment3D(rayVerts, rightControllerPosition, rightControllerPosition + rightControllerFwd * 10.f, 0.001f);

		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetModelConstants();
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_BACK);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(nullptr);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexArray(rayVerts);
	}

	std::vector<Vertex_PCU> raycastHighlightVerts;
	if (m_raycastResult.m_didImpact)
	{
		Vec3 blockPosition = m_raycastResult.m_impactBlockIter.GetWorldCenter();

		AABB3 blockBounds(Vec3::ZERO, Vec3::EAST + Vec3::NORTH + Vec3::SKYWARD);
		blockBounds.SetCenter(blockPosition);
		Vec3 const& mins = blockBounds.m_mins;
		Vec3 const& maxs = blockBounds.m_maxs;

		Vec3 BLF = Vec3(mins.x, maxs.y, mins.z);
		Vec3 BRF = Vec3(mins.x, mins.y, mins.z);
		Vec3 TRF = Vec3(mins.x, mins.y, maxs.z);
		Vec3 TLF = Vec3(mins.x, maxs.y, maxs.z);
		Vec3 BLB = Vec3(maxs.x, maxs.y, mins.z);
		Vec3 BRB = Vec3(maxs.x, mins.y, mins.z);
		Vec3 TRB = Vec3(maxs.x, mins.y, maxs.z);
		Vec3 TLB = Vec3(maxs.x, maxs.y, maxs.z);
		if (m_raycastResult.m_impactNormal == Vec3::EAST)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, BRB, BLB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BLB, TLB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TLB, TRB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRB, BRB, 0.01f, Rgba8::MAGENTA);
		}
		else if (m_raycastResult.m_impactNormal == Vec3::WEST)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, BLF, BRF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BRF, TRF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRF, TLF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TLF, BLF, 0.01f, Rgba8::MAGENTA);
		}
		else if (m_raycastResult.m_impactNormal == Vec3::NORTH)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, BLB, BLF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BLF, TLF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TLF, TLB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TLB, BLB, 0.01f, Rgba8::MAGENTA);
		}
		else if (m_raycastResult.m_impactNormal == Vec3::SOUTH)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, BRF, BRB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BRB, TRB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRB, TRF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRF, BRF, 0.01f, Rgba8::MAGENTA);
		}
		else if (m_raycastResult.m_impactNormal == Vec3::SKYWARD)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, TLF, TRF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRF, TRB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TRB, TLB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, TLB, TLF, 0.01f, Rgba8::MAGENTA);
		}
		else if (m_raycastResult.m_impactNormal == Vec3::GROUNDWARD)
		{
			AddVertsForLineSegment3D(raycastHighlightVerts, BLB, BRB, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BRB, BRF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BRF, BLF, 0.01f, Rgba8::MAGENTA);
			AddVertsForLineSegment3D(raycastHighlightVerts, BLF, BLB, 0.01f, Rgba8::MAGENTA);
		}

		g_renderer->SetBlendMode(BlendMode::ALPHA);
		g_renderer->SetDepthMode(DepthMode::ENABLED);
		g_renderer->SetModelConstants();
		g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
		g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
		g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
		g_renderer->BindTexture(nullptr);
		g_renderer->BindShader(nullptr);
		g_renderer->DrawVertexArray(raycastHighlightVerts);
	}

	
	//DebugRenderWorld(g_app->m_worldCamera);

	//DebugAddMessage(Stringf("World Update: %f ms", g_worldUpdateTime), 0.f, Rgba8::MAGENTA, Rgba8::MAGENTA);
	//DebugAddMessage(Stringf("World Render: %f ms", g_worldRenderTime), 0.f, Rgba8::GREEN, Rgba8::GREEN);
	//DebugAddMessage(Stringf("Chunk Mesh Rebuild Decision Time: %f ms", g_chunkRebuildDecisionTime), 0.f, Rgba8::RED, Rgba8::RED);
	//DebugAddMessage(Stringf("Chunk Mesh Rebuild Time: %f ms, Num Meshes Rebuilt: %d", g_chunkMeshRebuildTime, g_numChunkMeshesRebuilt), 0.f, Rgba8::RED, Rgba8::RED);
	//DebugAddMessage(Stringf("Lighting Processing: %f ms", g_lightingProcessingTime), 0.f, Rgba8::YELLOW, Rgba8::YELLOW);
	//DebugAddMessage(Stringf("Chunk Activation/Deactivation Decision Time: %f ms", g_chunkActivationDeactivationDecisionTime), 0.f, Rgba8::RED, Rgba8::RED);
	//DebugAddMessage(Stringf("Chunk Activate Time: %f ms", g_chunkActivationTime), 0.f, Rgba8::RED, Rgba8::RED);
	//DebugAddMessage(Stringf("AddVertsForBlock Total (per chunk): %f ms", g_blockVertexesAddingTime), 0.f, Rgba8::RED, Rgba8::RED);
	//
	//DebugRenderScreen(g_app->m_screenCamera);
}

void Game::StartGame()
{
	m_gameState = GameState::GAME;
	m_timeInState = 0.f;

	m_world = new World(this);

	DebugAddWorldArrow(Vec3::ZERO, Vec3::EAST * 1.5f, 0.05f, -1.f, Rgba8::RED);
	DebugAddWorldText("x- Forward", Mat44(Vec3::SOUTH, Vec3::EAST, Vec3::SKYWARD, Vec3(0.6f, 0.f, 0.15f)), 0.1f, Vec2(0.5f, 0.5f), -1.f, Rgba8::RED);
	DebugAddWorldArrow(Vec3::ZERO, Vec3::NORTH * 1.5f, 0.05f, -1.f, Rgba8::GREEN);
	DebugAddWorldText("Y- Left", Mat44(Vec3::WEST, Vec3::SOUTH, Vec3::SKYWARD, Vec3(0.f, 0.5f, 0.15f)), 0.1f, Vec2(0.5f, 0.5f), -1.f, Rgba8::GREEN);
	DebugAddWorldArrow(Vec3::ZERO, Vec3::SKYWARD * 1.5f, 0.05f, -1.f, Rgba8::BLUE);
	DebugAddWorldText("Z- Up", Mat44(Vec3::EAST, Vec3::SKYWARD, Vec3::NORTH, Vec3(0.f, -0.15f, 0.5f)), 0.1f, Vec2(0.5f, 0.5f), -1.f, Rgba8::BLUE);
}

void Game::QuitToAttractScreen()
{
	m_gameState = GameState::ATTRACT;
	m_timeInState = 0.f;
	delete m_world;
	m_world = nullptr;
}

void Game::UpdateIntroScreen(float deltaSeconds)
{
	m_timeInState += deltaSeconds;

	if (m_timeInState >= 4.5f)
	{
		m_gameState = GameState::ATTRACT;
		m_timeInState = 0.f;
	}

	UpdateCameras(deltaSeconds);
}

void Game::UpdateAttractScreen(float deltaSeconds)
{
	m_timeInState += deltaSeconds;

	XboxController xboxController = g_input->GetController(0);

	bool wasAnyStartKeyPressed =	g_input->WasKeyJustPressed(' ') ||
									xboxController.WasButtonJustPressed(XBOX_BUTTON_A);

	if (wasAnyStartKeyPressed)
	{
		StartGame();
	}
	if (g_input->WasKeyJustPressed(KEYCODE_ESC))
	{
		g_app->HandleQuitRequested();
	}

	if (g_openXR && g_openXR->IsInitialized())
	{
		VRController const& leftController = g_openXR->GetLeftController();
		VRController const& rightController = g_openXR->GetRightController();

		if (leftController.WasSelectButtonJustPressed() || rightController.WasSelectButtonJustPressed())
		{
			StartGame();
		}
		if (leftController.WasBackButtonJustPressed() || rightController.WasBackButtonJustPressed())
		{
			g_app->HandleQuitRequested();
		}
	}

	UpdateCameras(deltaSeconds);
}

void Game::RenderIntroScreen() const
{
	SpriteSheet logoSpriteSheet(m_logoTexture, IntVec2(15, 19));
	SpriteAnimDefinition logoAnimation(&logoSpriteSheet, 0, 271, 2.f, SpriteAnimPlaybackType::ONCE);
	SpriteAnimDefinition logoBlinkAnimation(&logoSpriteSheet, 270, 271, 0.2f, SpriteAnimPlaybackType::LOOP);
	
	std::vector<Vertex_PCU> introScreenVertexes;
	std::vector<Vertex_PCU> introScreenFadeOutVertexes;
	AABB2 animatedLogoBox(Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y) * 0.5f - Vec2(320.f, 200.f), Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y) * 0.5f + Vec2(320.f, 200.f));
	if (m_timeInState >= 2.f)
	{
		SpriteDefinition currentSprite = logoBlinkAnimation.GetSpriteDefAtTime(m_timeInState);
		AddVertsForAABB2(introScreenVertexes, animatedLogoBox, Rgba8::WHITE, currentSprite.GetUVs().m_mins, currentSprite.GetUVs().m_maxs);
	
		if (m_timeInState >= 3.5f)
		{
			AddVertsForAABB2(introScreenFadeOutVertexes, AABB2(Vec2::ZERO, Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y)), Rgba8(0, 0, 0, static_cast<unsigned char>(RoundDownToInt(RangeMapClamped((4.5f - m_timeInState), 1.f, 0.f, 0.f, 255.f)))));
		}
	}
	else
	{
		SpriteDefinition currentSprite = logoAnimation.GetSpriteDefAtTime(m_timeInState);
		AddVertsForAABB2(introScreenVertexes, animatedLogoBox, Rgba8::WHITE, currentSprite.GetUVs().m_mins, currentSprite.GetUVs().m_maxs);
	}
	g_renderer->BindTexture(m_logoTexture);
	g_renderer->DrawVertexArray(introScreenVertexes);
	g_renderer->BindTexture(nullptr);
	g_renderer->DrawVertexArray(introScreenFadeOutVertexes);
}

void Game::RenderAttractScreen() const
{
	std::vector<Vertex_PCU> attractVerts;
	float discRadius = SCREEN_SIZE_Y * (0.3f + 0.1f * (0.5f + 0.5f * SinDegrees(m_timeInState * 180.f)));
	AddVertsForDisc2D(attractVerts, Vec2(SCREEN_SIZE_X * 0.5f, SCREEN_SIZE_Y * 0.5f), discRadius, Rgba8::MAGENTA, Vec2::ZERO, Vec2::ONE, 64);
	g_renderer->SetBlendMode(BlendMode::ALPHA);
	g_renderer->SetDepthMode(DepthMode::DISABLED);
	g_renderer->SetModelConstants();
	g_renderer->SetRasterizerCullMode(RasterizerCullMode::CULL_NONE);
	g_renderer->SetRasterizerFillMode(RasterizerFillMode::SOLID);
	g_renderer->SetSamplerMode(SamplerMode::POINT_CLAMP);
	g_renderer->BindTexture(nullptr);
	g_renderer->BindShader(nullptr);
	g_renderer->DrawVertexArray(attractVerts);
}
