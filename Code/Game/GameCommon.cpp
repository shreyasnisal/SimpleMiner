#include "Game/GameCommon.hpp"


float g_activationRadius = DEFAULT_ACTIVATION_RADIUS;
float g_deactivationRadius = DEFAULT_ACTIVATION_RADIUS;



bool operator<(IntVec2 const& a, IntVec2 const& b)
{
	if (a.y < b.y)
	{
		return true;
	}
	else if (a.y > b.y)
	{
		return false;
	}
	else
	{
		return (a.x < b.x);
	}
}
