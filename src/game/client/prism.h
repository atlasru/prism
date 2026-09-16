// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_H
#define GAME_CLIENT_PRISM_H
#include <engine/shared/config.h>
#include <algorithm>

namespace Prism
{
enum { DEFAULT, CLEAN, COMPETITIVE, CINEMATIC, CUSTOM };
struct CVisualSettings
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) int m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) unsigned m_##Name = Def;
#include <engine/shared/prism_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
};
inline CVisualSettings Capture(const CConfig &Config)
{
 CVisualSettings Result;
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Result.m_##Name = std::clamp(Config.m_##Name, Min, Max);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Result.m_##Name = Config.m_##Name;
#include <engine/shared/prism_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
 return Result;
}
inline CVisualSettings Preset(int Id)
{
 CVisualSettings Result;
 if(Id < CLEAN || Id > CINEMATIC) return Result;
 Result.m_PrismEnabled = 1;
 Result.m_PrismLocalOutline = 1;
 Result.m_PrismLocalHook = 1;
 if(Id >= COMPETITIVE)
 {
  Result.m_PrismOtherOutline = 1;
  Result.m_PrismOtherHook = 1;
  Result.m_PrismLocalOutlineWidth = 3;
 }
 if(Id == CINEMATIC)
 {
  Result.m_PrismLocalGlow = Result.m_PrismOtherGlow = 1;
  Result.m_PrismLocalHookGlow = Result.m_PrismOtherHookGlow = 1;
  Result.m_PrismLocalGlowIntensity = Result.m_PrismOtherGlowIntensity = 65;
  Result.m_PrismLocalHookIntensity = Result.m_PrismOtherHookIntensity = 60;
 }
 return Result;
}
inline void Store(CConfig &Config, const CVisualSettings &Values)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Config.m_##Name = std::clamp(Values.m_##Name, Min, Max);
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Config.m_##Name = Values.m_##Name;
#include <engine/shared/prism_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
}
inline bool Equal(const CVisualSettings &A, const CVisualSettings &B)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) if(A.m_##Name != B.m_##Name) return false;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) if(A.m_##Name != B.m_##Name) return false;
#include <engine/shared/prism_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
 return true;
}
inline void ApplyPreset(CConfig &Config, int Id)
{
 Id = std::clamp(Id, (int)DEFAULT, (int)CUSTOM);
 if(Id != CUSTOM) Store(Config, Preset(Id));
 Config.m_PrismPreset = Id;
}
inline void Validate(CConfig &Config)
{
 // Also protects against direct assignments outside the console range validator.
 const auto Current = Capture(Config);
 Store(Config, Current);
 Config.m_PrismPreset = std::clamp(Config.m_PrismPreset, (int)DEFAULT, (int)CUSTOM);
 Config.m_PrismOverlay = std::clamp(Config.m_PrismOverlay, 0, 1);
 if(Config.m_PrismPreset != CUSTOM && !Equal(Current, Preset(Config.m_PrismPreset)))
  Config.m_PrismPreset = CUSTOM;
}
inline void Reset(CConfig &Config)
{
 ApplyPreset(Config, DEFAULT);
 Config.m_PrismOverlay = 0;
}
}
#endif
