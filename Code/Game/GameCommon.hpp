#pragma once

#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/JobSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/SimpleTriangleFont.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Math/RaycastUtils.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Renderer/Spritesheet.hpp"

class App;
struct Block;

extern App*							g_app;
extern RandomNumberGenerator*		g_RNG;
extern Renderer*					g_renderer;
extern AudioSystem*					g_audio;
extern Window*						g_window;
extern BitmapFont*					g_squirrelFont;
extern JobSystem*					g_jobSystem;

extern SpriteSheet*					g_spritesheet;

constexpr float SCREEN_SIZE_X		= 1600.f;
constexpr float SCREEN_SIZE_Y		= 800.f;

constexpr float MAX_CAMERA_SHAKE_X	= 10.f;
constexpr float MAX_CAMERA_SHAKE_Y	= 5.f;

constexpr float DEFAULT_ACTIVATION_RADIUS = 100.f;

constexpr int MAX_CHUNKS = 128;
//constexpr int MAX_CHUNKS = 1024;

extern float g_activationRadius;
extern float g_deactivationRadius;

bool operator<(IntVec2 const& a, IntVec2 const& b);

enum class Direction
{
	EAST, WEST, NORTH, SOUTH, SKYWARD, GROUNDWARD
};

struct SimpleMinerConstants
{
	Vec4		b_camWorldPos;
	Vec4		b_skyColor;
	Vec4		b_outdoorLightColor;
	Vec4		b_indoorLightColor;
	float		b_fogStartDist;
	float		b_fogEndDist;
	float		b_fogMaxAlpha;
	float		b_time;
};
