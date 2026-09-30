#ifndef GAME_CLIENT_PRISM_ATMOSPHERE_H
#define GAME_CLIENT_PRISM_ATMOSPHERE_H
#include <base/color.h>
#include <base/vmath.h>

#include <engine/shared/config.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace PrismAtmosphere
{
	enum
	{
		OFF,
		SUBTLE,
		CINEMATIC,
		VIVID,
		CUSTOM
	};
	struct SSettings
	{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) int m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) unsigned m_##Name = Def;
#include <engine/shared/prism_atmosphere_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
	};
	inline SSettings Capture(const CConfig &Config)
	{
		SSettings Result;
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Result.m_##Name = std::clamp(Config.m_##Name, Min, Max);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Result.m_##Name = Config.m_##Name;
#include <engine/shared/prism_atmosphere_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
		return Result;
	}
	inline bool EqualAtmosphere(const SSettings &A, const SSettings &B)
	{
		return A.m_PrismAtmosphere == B.m_PrismAtmosphere && A.m_PrismExposure == B.m_PrismExposure &&
		       A.m_PrismContrast == B.m_PrismContrast && A.m_PrismSaturation == B.m_PrismSaturation &&
		       A.m_PrismGamma == B.m_PrismGamma && A.m_PrismHighlights == B.m_PrismHighlights &&
		       A.m_PrismShadows == B.m_PrismShadows && A.m_PrismBloomStrength == B.m_PrismBloomStrength &&
		       A.m_PrismBloomThreshold == B.m_PrismBloomThreshold && A.m_PrismBloomRadius == B.m_PrismBloomRadius &&
		       A.m_PrismVignette == B.m_PrismVignette && A.m_PrismTintStrength == B.m_PrismTintStrength &&
		       A.m_PrismAtmosphereTint == B.m_PrismAtmosphereTint;
	}
	inline void Clamp(CConfig &Config)
	{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Config.m_##Name = std::clamp(Config.m_##Name, Min, Max);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc)
#include <engine/shared/prism_atmosphere_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
	}
	template<class T>
	inline void ApplyPreset(T &C, int Id)
	{
		Id = std::clamp<int>(Id, OFF, CUSTOM);
		C.m_PrismAtmospherePreset = Id;
		if(Id == CUSTOM)
			return;
		if(Id == OFF)
		{
			C.m_PrismAtmosphere = 0;
			return;
		}
		C.m_PrismAtmosphere = 1;
		C.m_PrismExposure = Id == CINEMATIC ? -30 : 0;
		C.m_PrismContrast = Id == SUBTLE ? 105 : Id == CINEMATIC ? 125 :
									   115;
		C.m_PrismSaturation = Id == SUBTLE ? 105 : Id == CINEMATIC ? 80 :
									     150;
		C.m_PrismGamma = 100;
		C.m_PrismHighlights = Id == CINEMATIC ? -15 : 0;
		C.m_PrismShadows = Id == CINEMATIC ? -10 : 0;
		C.m_PrismBloomStrength = Id == SUBTLE ? 10 : Id == CINEMATIC ? 35 :
									       25;
		C.m_PrismBloomThreshold = 80;
		C.m_PrismBloomRadius = Id == CINEMATIC ? 8 : 4;
		C.m_PrismVignette = Id == CINEMATIC ? 25 : 0;
		C.m_PrismTintStrength = 0;
		// Tint color and all glow controls are independent of atmosphere presets.
	}
	inline void Validate(CConfig &C)
	{
		Clamp(C);
		if(C.m_PrismAtmospherePreset == CUSTOM)
			return;
		if(C.m_PrismAtmospherePreset == OFF)
		{
			if(C.m_PrismAtmosphere)
				C.m_PrismAtmospherePreset = CUSTOM;
			return;
		}
		SSettings Expected = Capture(C);
		ApplyPreset(Expected, C.m_PrismAtmospherePreset);
		if(C.m_PrismAtmosphere != Expected.m_PrismAtmosphere ||
			C.m_PrismExposure != Expected.m_PrismExposure || C.m_PrismContrast != Expected.m_PrismContrast ||
			C.m_PrismSaturation != Expected.m_PrismSaturation || C.m_PrismGamma != Expected.m_PrismGamma ||
			C.m_PrismHighlights != Expected.m_PrismHighlights || C.m_PrismShadows != Expected.m_PrismShadows ||
			C.m_PrismBloomStrength != Expected.m_PrismBloomStrength || C.m_PrismBloomThreshold != Expected.m_PrismBloomThreshold ||
			C.m_PrismBloomRadius != Expected.m_PrismBloomRadius || C.m_PrismVignette != Expected.m_PrismVignette ||
			C.m_PrismTintStrength != Expected.m_PrismTintStrength)
			C.m_PrismAtmospherePreset = CUSTOM;
	}
	inline void ValuesChanged(CConfig &C, const SSettings &Before)
	{
		if(!EqualAtmosphere(Before, Capture(C)))
			C.m_PrismAtmospherePreset = CUSTOM;
		Validate(C);
	}
	inline bool WorldEnabled(bool Master, bool Enabled, bool InGame, bool Supported)
	{ return Master && Enabled && InGame && Supported; }
	inline bool GlowEnabled(bool Enabled, bool Local, bool Dummy, bool ShowLocal, bool ShowOthers, bool ShowDummy)
	{ return Enabled && (Dummy ? ShowDummy : Local ? ShowLocal :
							 ShowOthers); }
	inline ColorRGBA GlowColor(int Mode, ColorRGBA Accent, ColorRGBA Entity, ColorRGBA Custom)
	{ return Mode == 1 ? Entity : Mode == 2 ? Custom :
						  Accent; }
	inline const std::array<vec2, 32> &AuraDirections()
	{
		static const auto Directions = [] {
			std::array<vec2, 32> Result;
			for(int i = 0; i < 32; ++i)
				Result[i] = direction(i * 2 * pi / 32);
			return Result;
		}();
		return Directions;
	}
	// Grid traversal: at most 15 cells per axis at the maximum 192-unit radius.
	// Solid callback receives tile coordinates. Conservative at exact corners.
	template<class F>
	inline vec2 ClipAuraRay(vec2 From, vec2 To, F Solid)
	{
		const vec2 Delta = To - From;
		int X = static_cast<int>(std::floor((From.x + 0.5f) / 32.0f));
		int Y = static_cast<int>(std::floor((From.y + 0.5f) / 32.0f));
		if(Solid(X, Y))
			return From;
		const int StepX = Delta.x >= 0 ? 1 : -1, StepY = Delta.y >= 0 ? 1 : -1;
		const float Infinity = std::numeric_limits<float>::infinity();
		const float DX = std::abs(Delta.x) > 0.00001f ? 32.0f / std::abs(Delta.x) : Infinity;
		const float DY = std::abs(Delta.y) > 0.00001f ? 32.0f / std::abs(Delta.y) : Infinity;
		float TX = std::isfinite(DX) ? ((X + (StepX > 0)) * 32.0f - 0.5f - From.x) / Delta.x : Infinity;
		float TY = std::isfinite(DY) ? ((Y + (StepY > 0)) * 32.0f - 0.5f - From.y) / Delta.y : Infinity;
		for(int Step = 0; Step < 32; ++Step)
		{
			const float T = std::min(TX, TY);
			if(T > 1.0f || !std::isfinite(T))
				return To;
			const bool CrossX = TX <= TY, CrossY = TY <= TX;
			if((CrossX && Solid(X + StepX, Y)) || (CrossY && Solid(X, Y + StepY)))
				return From + Delta * std::max(0.0f, T - 0.0001f);
			if(CrossX)
			{
				X += StepX;
				TX += DX;
			}
			if(CrossY)
			{
				Y += StepY;
				TY += DY;
			}
			if(Solid(X, Y))
				return From + Delta * std::max(0.0f, T - 0.0001f);
		}
		return From; // Safe bounded fallback for invalid/out-of-range input.
	}
	inline float Falloff(float DistanceFraction, float Exponent)
	{ return std::pow(std::clamp(1.0f - DistanceFraction, 0.0f, 1.0f), std::clamp(Exponent, 0.25f, 8.0f)); }
}
#endif
