// Prism native UI tokens. Distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_UI_H
#define GAME_CLIENT_PRISM_UI_H
#include <base/color.h>
#include <engine/shared/config.h>
namespace PrismUi
{
struct STheme
{
 ColorRGBA m_Accent, m_Background, m_Panel, m_Text;
 float m_Rounding;
 explicit STheme(const CConfig &Config) :
  m_Accent(color_cast<ColorRGBA>(ColorHSLA(Config.m_PrismThemeAccent))),
  m_Background(color_cast<ColorRGBA>(ColorHSLA(Config.m_PrismThemeBackground))),
  m_Panel(color_cast<ColorRGBA>(ColorHSLA(Config.m_PrismThemePanel))),
  m_Text(color_cast<ColorRGBA>(ColorHSLA(Config.m_PrismThemeText))),
  m_Rounding(Config.m_PrismThemeRounding) {}
};
inline void ApplyTheme(CConfig &Config, int Preset)
{
 const ColorRGBA Accents[] = {ColorRGBA(1.0f, 0.541f, 0.067f, 1.0f), ColorRGBA(0.40f, 0.72f, 1.0f, 1.0f), ColorRGBA(0.68f, 0.55f, 1.0f, 1.0f)};
 Config.m_PrismThemeAccent = color_cast<ColorHSLA>(Accents[std::clamp(Preset, 0, 2)]).Pack(false);
 Config.m_PrismThemeBackground = color_cast<ColorHSLA>(ColorRGBA(23 / 255.0f, 25 / 255.0f, 27 / 255.0f, 1)).Pack(false);
 Config.m_PrismThemePanel = color_cast<ColorHSLA>(ColorRGBA(32 / 255.0f, 34 / 255.0f, 37 / 255.0f, 1)).Pack(false);
 Config.m_PrismThemeText = color_cast<ColorHSLA>(ColorRGBA(237 / 255.0f, 239 / 255.0f, 242 / 255.0f, 1)).Pack(false);
}
}
#endif
