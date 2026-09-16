// Prism additions, distributed under the zlib license in license.txt.
#include "menus.h"
#include <engine/shared/config.h>
#include <game/client/prism.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

void CMenus::RenderSettingsPrism(CUIRect MainView)
{
 Prism::Validate(g_Config);
 MainView.Draw(ColorRGBA(0.075f, 0.08f, 0.09f, 0.98f), IGraphics::CORNER_ALL, 5.0f);
 MainView.Margin(10.0f, &MainView);
 CUIRect Row, Tab;
 MainView.HSplitTop(25.0f, &Row, &MainView);
 Ui()->DoLabel(&Row, "Prism 0.1.0 / DDNet 20.0", 17.0f, TEXTALIGN_ML);
 static int s_Category = 0;
 static CButtonContainer s_aTabs[4];
 const char *apTabs[] = {"General", "Tee", "Hook", "Performance"};
 MainView.HSplitTop(25.0f, &Row, &MainView);
 const float TabWidth = Row.w / 4.0f;
 for(int i = 0; i < 4; ++i)
 {
  Row.VSplitLeft(TabWidth, &Tab, &Row);
  Tab.VMargin(2.0f, &Tab);
  if(DoButton_MenuTab(&s_aTabs[i], apTabs[i], s_Category == i, &Tab, IGraphics::CORNER_ALL)) s_Category = i;
 }
 MainView.HSplitTop(10.0f, nullptr, &MainView);
 static CScrollRegion s_aScroll[4];
 auto &Scroll = s_aScroll[s_Category];
 Scroll.Begin(&MainView);
 auto NextRow = [&](float Height = 24.0f) {
  CUIRect Result;
  MainView.HSplitTop(Height, &Result, &MainView);
  Scroll.AddRect(Result);
  return Result;
 };
 auto Check = [&](const char *pLabel, int *pValue) {
  CUIRect Rect = NextRow();
  if(!Scroll.RectClipped(Rect) && DoButton_CheckBox(pValue, pLabel, *pValue, &Rect)) *pValue ^= 1;
 };
 auto Slider = [&](const char *pLabel, int *pValue, int Min, int Max) {
  CUIRect Rect = NextRow();
  if(!Scroll.RectClipped(Rect)) Ui()->DoScrollbarOption(pValue, pValue, &Rect, pLabel, Min, Max);
 };
 auto Label = [&](const char *pLabel) {
  CUIRect Rect = NextRow(28.0f);
  if(!Scroll.RectClipped(Rect)) Ui()->DoLabel(&Rect, pLabel, 14.0f, TEXTALIGN_ML);
 };
 static CButtonContainer s_aColors[6];
 auto Color = [&](const char *pLabel, unsigned *pValue, int Index) {
  CUIRect Rect = NextRow(28.0f);
  if(!Scroll.RectClipped(Rect)) DoLine_ColorPicker(&s_aColors[Index], 24.0f, 12.0f, 4.0f, &Rect, pLabel, pValue, ColorRGBA(0.6f, 0.75f, 0.9f, 1.0f), false);
 };
 if(s_Category == 0)
 {
  Check("Enable Prism visuals", &g_Config.m_PrismEnabled);
  Label("Presets");
  const char *apPresets[] = {"Default", "Clean", "Competitive", "Cinematic", "Custom"};
  static CButtonContainer s_aPresets[5];
  for(int i = 0; i < 5; ++i)
  {
   CUIRect Rect = NextRow(27.0f);
   if(!Scroll.RectClipped(Rect) && DoButton_Menu(&s_aPresets[i], apPresets[i], g_Config.m_PrismPreset == i, &Rect)) Prism::ApplyPreset(g_Config, i);
  }
  Label("Default restores original DDNet visuals.");
  Label("Quick toggle: bind a key to prism_toggle in F1.");
  static CButtonContainer s_Reset;
  CUIRect Rect = NextRow(27.0f);
  if(!Scroll.RectClipped(Rect) && DoButton_Menu(&s_Reset, "Reset Prism to defaults", 0, &Rect)) Prism::Reset(g_Config);
 }
 else if(s_Category == 1)
 {
  Label("Local player");
  Check("Outline", &g_Config.m_PrismLocalOutline);
  Slider("Outline width", &g_Config.m_PrismLocalOutlineWidth, 1, 8);
  Color("Outline color", &g_Config.m_PrismLocalOutlineColor, 0);
  Check("Glow", &g_Config.m_PrismLocalGlow);
  Slider("Glow intensity", &g_Config.m_PrismLocalGlowIntensity, 0, 100);
  Color("Glow color", &g_Config.m_PrismLocalGlowColor, 1);
  Label("Other players");
  Check("Outline", &g_Config.m_PrismOtherOutline);
  Slider("Outline width", &g_Config.m_PrismOtherOutlineWidth, 1, 8);
  Color("Outline color", &g_Config.m_PrismOtherOutlineColor, 2);
  Check("Glow", &g_Config.m_PrismOtherGlow);
  Slider("Glow intensity", &g_Config.m_PrismOtherGlowIntensity, 0, 100);
  Color("Glow color", &g_Config.m_PrismOtherGlowColor, 3);
 }
 else if(s_Category == 2)
 {
  Label("Local player");
  Check("Custom hook color", &g_Config.m_PrismLocalHook);
  Color("Hook / glow color", &g_Config.m_PrismLocalHookColor, 4);
  Check("Hook glow", &g_Config.m_PrismLocalHookGlow);
  Slider("Glow intensity", &g_Config.m_PrismLocalHookIntensity, 0, 100);
  Label("Other players");
  Check("Custom hook color", &g_Config.m_PrismOtherHook);
  Color("Hook / glow color", &g_Config.m_PrismOtherHookColor, 5);
  Check("Hook glow", &g_Config.m_PrismOtherHookGlow);
  Slider("Glow intensity", &g_Config.m_PrismOtherHookIntensity, 0, 100);
 }
 else
 {
  Check("FPS and average frame time", &g_Config.m_PrismOverlay);
  Label("Uses DDNet frame timing. Visible during gameplay.");
  Label("Clean: local outline and hook tint; no glow.");
  Label("Glow can be disabled separately for each group.");
 }
 Scroll.End();
 Prism::Validate(g_Config);
}
