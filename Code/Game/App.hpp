#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Camera.hpp"

#include "Game/Game.hpp"

class App
{
public:
						App							();
						~App						();
	void				Startup						();
	void				Shutdown					();
	void				Run							();
	void				RunFrame					();

	bool				IsQuitting					() const		{ return m_isQuitting; }
	bool				HandleQuitRequested			();
	static bool			HandleQuitRequested			(EventArgs& args);
	static bool			ShowControls				(EventArgs& args);

	void				LoadConfig					();

	// VR Integration
	void				InitializeCameras();
	XREye				GetCurrentEye() const;
	Camera const		GetCurrentCamera() const;

public:
	Game*				m_game;
	float				m_frameRate;

	// VR Integration
	Camera						m_screenCamera;
	Camera						m_worldCamera;
	Camera						m_leftEyeCamera;
	Camera						m_rightEyeCamera;
	XREye						m_currentEye = XREye::NONE;
	Texture*			m_screenRTVTexture = nullptr;

private:
	void				BeginFrame					();
	void				Update						();
	void				Render						() const;
	void				RenderScreen				() const;
	void				EndFrame					();

private:
	bool				m_isQuitting				= false;

	Camera				m_devConsoleCamera			= Camera();
};
