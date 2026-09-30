#ifndef GAME_CLIENT_PRISM_ATMOSPHERE_H
#define GAME_CLIENT_PRISM_ATMOSPHERE_H
#include <base/color.h>

#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

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
	inline bool WorldEnabled(bool Master, bool Enabled, bool InGame, bool Supported)
	{ return Master && Enabled && InGame && Supported; }
	inline bool GlowEnabled(bool Enabled, bool Local, bool Dummy, bool ShowLocal, bool ShowOthers, bool ShowDummy)
	{ return Enabled && (Dummy ? ShowDummy : Local ? ShowLocal :
							 ShowOthers); }
	inline ColorRGBA GlowColor(int Mode, ColorRGBA Accent, ColorRGBA Entity, ColorRGBA Custom)
	{ return Mode == 1 ? Entity : Mode == 2 ? Custom :
						  Accent; }
	inline float Falloff(float DistanceFraction, float Exponent)
	{ return std::pow(std::clamp(1.0f - DistanceFraction, 0.0f, 1.0f), std::clamp(Exponent, 0.25f, 8.0f)); }
}
#endif
