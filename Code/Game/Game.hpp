#pragma once

#include "Game/GameCommon.hpp"
#include "Game/Block.hpp"
#include "Game/World.hpp"

#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Stopwatch.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Renderer.hpp"


class		App;
class		Texture;
class		World;

enum class GameState
{
	INTRO,
	ATTRACT,
	GAME
};

class Game
{
public:
	~Game();
	Game();
	void						Update												();
	void						Render												() const;
	void						ClearScreen() const;
	void						RenderScreen() const;

	void						StartGame											();
	void						QuitToAttractScreen									();
	
	static bool					Event_GameClock										(EventArgs& args);

public:	
	static constexpr float SCREEN_QUAD_DISTANCE = 2.f;

	bool						m_isPaused											= false;
	bool						m_drawDebug											= false;

	Vec3						m_cameraPosition = Vec3(0.f, 0.f, 70.f);
	EulerAngles					m_cameraOrientation = EulerAngles::ZERO;
	//Camera						m_screenCamera;
	//Camera						m_worldCamera;

	// Change gameState to INTRO to show intro screen
	// BEWARE: The intro screen spritesheet is a large texture and the game will take much longer to load
	GameState					m_gameState											= GameState::ATTRACT;
	Clock						m_gameClock = Clock();
	World*						m_world = nullptr;

	Vec3						m_raycastStartPosition;
	Vec3						m_raycastDirection;
	bool						m_isRaycastLocked = false;
	BlockDefinitionID			m_selectedBlockType = 1;
	Vec3						m_blockPlacingPosition;
	Vec3						m_blockDiggingPosition;

	SimpleMinerRaycastResult	m_raycastResult;

	// VR Integration
	Vec3 m_leftEyeLocalPosition = Vec3::ZERO;
	Vec3 m_rightEyeLocalPosition = Vec3::ZERO;
	EulerAngles m_hmdOrientation = EulerAngles::ZERO;

private:

	void						UpdateIntroScreen									(float deltaSeconds);
	void						UpdateAttractScreen									(float deltaSeconds);
	void						UpdateGame											(float deltaSeconds);	

	void						HandleDeveloperCheats								();
	void						UpdateEntities										();

	void						UpdateInput											();
	void						UpdateKeyboardInput();
	void						UpdateVRInput();
	void						UpdateCameras										(float deltaSeconds);

	void RenderWorldScreenQuad() const;

	void						RenderIntroScreen									() const;
	void						RenderAttractScreen									() const;
	void						RenderGame											() const;

	void						LoadAssets											();

private:
	Texture*					m_logoTexture										= nullptr;

	float						m_timeInState										= 0.f;

	std::vector<Vertex_PCU>		m_gridStaticVerts;
	Mat44 m_screenBillboardMatrix;
};