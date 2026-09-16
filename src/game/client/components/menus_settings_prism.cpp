// Prism additions, distributed under the zlib license in license.txt.
#include "menus.h"

#include <algorithm>

#include <engine/shared/config.h>
#include <game/client/prism.h>
#include <game/client/ui_scrollregion.h>

void CMenus::RenderSettingsPrism(CUIRect Screen)
{
	Prism::Validate(g_Config);
	const float Motion = g_Config.m_PrismReducedMotion ? 1.0f : std::clamp(Client()->RenderFrameTime() * 15.0f, 0.0f, 1.0f);
	m_PrismTransition += ((m_PrismOpen ? 1.0f : 0.0f) - m_PrismTransition) * Motion;
	if(!m_PrismOpen && m_PrismTransition < 0.01f)
	{
		m_PrismTransition = 0.0f;
		return;
	}

	const float Fade = m_PrismTransition * m_PrismTransition * (3.0f - 2.0f * m_PrismTransition);
	const float Opacity = g_Config.m_PrismPanelOpacity / 100.0f;
	const float Scale = g_Config.m_PrismMenuScale / 100.0f;
	Screen.Draw(ColorRGBA(0.012f, 0.018f, 0.030f, 0.66f * Fade), IGraphics::CORNER_NONE, 0.0f);

	CUIRect Panel = Screen;
	Panel.w = std::min(Screen.w - 24.0f, 760.0f * Scale);
	Panel.h = std::min(Screen.h - 24.0f, 510.0f * Scale);
	Panel.x = Screen.x + (Screen.w - Panel.w) * 0.5f;
	Panel.y = Screen.y + (Screen.h - Panel.h) * 0.5f + (1.0f - Fade) * 13.0f;
	CUIRect Shadow = Panel;
	Shadow.x -= 8.0f;
	Shadow.y -= 3.0f;
	Shadow.w += 16.0f;
	Shadow.h += 17.0f;
	Shadow.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.26f * Fade), IGraphics::CORNER_ALL, 23.0f);
	CUIRect Edge = Panel;
	Edge.Margin(-1.0f, &Edge);
	Edge.Draw(ColorRGBA(0.61f, 0.72f, 0.83f, 0.18f * Fade), IGraphics::CORNER_ALL, 20.0f);
	Panel.Draw(ColorRGBA(0.063f, 0.081f, 0.105f, Opacity * Fade), IGraphics::CORNER_ALL, 19.0f);
	CUIRect Sheen;
	Panel.HSplitTop(52.0f, &Sheen, nullptr);
	Sheen.Draw(ColorRGBA(0.65f, 0.76f, 0.86f, 0.065f * Fade), IGraphics::CORNER_T, 19.0f);

	// Closing is purely visual: release interactive items immediately.
	if(!m_PrismOpen)
		return;

	CUIRect Inner = Panel;
	Inner.Margin(17.0f, &Inner);
	CUIRect Header, Body, Footer;
	Inner.HSplitTop(47.0f, &Header, &Inner);
	Inner.HSplitBottom(25.0f, &Body, &Footer);
	CUIRect Title, Close;
	Header.VSplitRight(67.0f, &Title, &Close);
	Ui()->DoLabel(&Title, "PRISM   /   VISUAL STUDIO", 18.0f, TEXTALIGN_ML);
	static CButtonContainer s_Close;
	if(DoButton_Menu(&s_Close, "Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f, ColorRGBA(0.35f, 0.43f, 0.51f, 0.22f)))
		m_PrismOpen = false;

	CUIRect Sidebar, Content;
	Body.VSplitLeft(std::min(153.0f * Scale, Body.w * 0.29f), &Sidebar, &Content);
	Sidebar.VSplitRight(12.0f, &Sidebar, nullptr);
	Sidebar.Draw(ColorRGBA(0.026f, 0.041f, 0.060f, 0.52f * Fade), IGraphics::CORNER_ALL, 12.0f);
	Sidebar.Margin(8.0f, &Sidebar);
	Content.Draw(ColorRGBA(0.034f, 0.049f, 0.068f, 0.45f * Fade), IGraphics::CORNER_ALL, 12.0f);
	Content.Margin(12.0f, &Content);

	static const char *s_apTabs[] = {"General", "Presets", "Tee", "Hook", "Interface", "Performance"};
	static CButtonContainer s_aTabs[6];
	static float s_aTabBlend[6] = {};
	for(int i = 0; i < 6; ++i)
	{
		CUIRect Tab;
		Sidebar.HSplitTop(40.0f, &Tab, &Sidebar);
		Sidebar.HSplitTop(5.0f, nullptr, &Sidebar);
		const float Target = m_PrismCategory == i ? 1.0f : 0.0f;
		s_aTabBlend[i] += (Target - s_aTabBlend[i]) * Motion;
		const float Highlight = s_aTabBlend[i];
		const ColorRGBA ButtonColor(0.26f + Highlight * 0.16f, 0.32f + Highlight * 0.18f, 0.40f + Highlight * 0.20f, 0.10f + Highlight * 0.32f);
		if(DoButton_Menu(&s_aTabs[i], s_apTabs[i], 0, &Tab, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f, ButtonColor))
		{
			m_PrismCategory = i;
			Ui()->SetActiveItem(nullptr);
		}
	}

	static CScrollRegion s_aScroll[6];
	CUIRect ScrollView = Content;
	CScrollRegion &Scroll = s_aScroll[m_PrismCategory];
	Scroll.Begin(&ScrollView);
	auto NextRow = [&](float Height = 34.0f) {
		CUIRect Row;
		ScrollView.HSplitTop(Height, &Row, &ScrollView);
		Scroll.AddRect(Row);
		ScrollView.HSplitTop(4.0f, nullptr, &ScrollView);
		return Row;
	};
	auto Label = [&](const char *pLabel) {
		CUIRect Row = NextRow(30.0f);
		if(!Scroll.RectClipped(Row))
			Ui()->DoLabel(&Row, pLabel, 15.0f, TEXTALIGN_ML);
	};
	static float s_aToggleProgress[24] = {};
	int ToggleIndex = 0;
	auto Toggle = [&](const char *pLabel, int *pValue) {
		CUIRect Row = NextRow(35.0f);
		const int Index = ToggleIndex++;
		if(Scroll.RectClipped(Row))
			return;
		Row.Draw(ColorRGBA(0.45f, 0.55f, 0.65f, 0.095f), IGraphics::CORNER_ALL, 8.0f);
		CUIRect LabelRect, Switch;
		Row.VSplitRight(52.0f, &LabelRect, &Switch);
		LabelRect.VMargin(9.0f, &LabelRect);
		Ui()->DoLabel(&LabelRect, pLabel, 13.0f, TEXTALIGN_ML);
		Switch.VMargin(7.0f, &Switch);
		Switch.HMargin(8.0f, &Switch);
		const float Target = *pValue ? 1.0f : 0.0f;
		s_aToggleProgress[Index] += (Target - s_aToggleProgress[Index]) * Motion;
		const float Position = s_aToggleProgress[Index];
		Switch.Draw(ColorRGBA(0.22f + 0.17f * Position, 0.26f + 0.27f * Position, 0.33f + 0.32f * Position, 0.9f), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Knob = Switch;
		Knob.w = 14.0f;
		Knob.h = 14.0f;
		Knob.x += 2.0f + Position * (Switch.w - Knob.w - 4.0f);
		Knob.y = Switch.y + (Switch.h - Knob.h) * 0.5f;
		Knob.Draw(ColorRGBA(0.94f, 0.96f, 0.98f, 1.0f), IGraphics::CORNER_ALL, 7.0f);
		if(Ui()->DoButtonLogic(pValue, *pValue, &Row, BUTTONFLAG_LEFT))
			*pValue ^= 1;
	};
	auto Slider = [&](const char *pLabel, int *pValue, int Min, int Max) {
		CUIRect Row = NextRow(37.0f);
		if(!Scroll.RectClipped(Row))
		{
			Row.Draw(ColorRGBA(0.45f, 0.55f, 0.65f, 0.065f), IGraphics::CORNER_ALL, 8.0f);
			Row.Margin(5.0f, &Row);
			Ui()->DoScrollbarOption(pValue, pValue, &Row, pLabel, Min, Max);
		}
	};
	static CButtonContainer s_aColors[6];
	auto Color = [&](const char *pLabel, unsigned *pValue, int Index) {
		CUIRect Row = NextRow(31.0f);
		if(!Scroll.RectClipped(Row))
			DoLine_ColorPicker(&s_aColors[Index], 24.0f, 12.0f, 3.0f, &Row, pLabel, pValue, ColorRGBA(0.65f, 0.78f, 0.91f, 1.0f), false);
	};

	switch(m_PrismCategory)
	{
	case 0:
	{
		Label("Your game, your visual language");
		Toggle("Enable Prism visuals", &g_Config.m_PrismEnabled);
		Toggle("Performance overlay", &g_Config.m_PrismOverlay);
		Label("Insert opens or closes this menu.");
		Label("Disabling visuals preserves original DDNet rendering.");
		static CButtonContainer s_Reset;
		CUIRect Reset = NextRow(37.0f);
		if(!Scroll.RectClipped(Reset) && DoButton_Menu(&s_Reset, "Restore Prism defaults", 0, &Reset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f, ColorRGBA(0.32f, 0.40f, 0.49f, 0.28f)))
			Prism::Reset(g_Config);
		break;
	}
	case 1:
	{
		Label("Curated looks");
		static const char *s_apPresets[] = {"Default  /  original DDNet", "Clean  /  quiet accents", "Competitive  /  clear silhouettes", "Cinematic  /  soft glow", "Custom  /  your settings"};
		static CButtonContainer s_aPresets[5];
		for(int i = 0; i < 5; ++i)
		{
			CUIRect Preset = NextRow(43.0f);
			if(Scroll.RectClipped(Preset))
				continue;
			const bool Selected = g_Config.m_PrismPreset == i;
			const ColorRGBA Color = Selected ? ColorRGBA(0.39f, 0.54f, 0.67f, 0.42f) : ColorRGBA(0.29f, 0.37f, 0.46f, 0.17f);
			if(DoButton_Menu(&s_aPresets[i], s_apPresets[i], 0, &Preset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 10.0f, 0.0f, Color))
				Prism::ApplyPreset(g_Config, i);
		}
		Label("Editing a built-in look switches to Custom.");
		break;
	}
	case 2:
		Label("Local player");
		Toggle("Outline", &g_Config.m_PrismLocalOutline);
		Slider("Outline width", &g_Config.m_PrismLocalOutlineWidth, 1, 8);
		Color("Outline color", &g_Config.m_PrismLocalOutlineColor, 0);
		Toggle("Glow", &g_Config.m_PrismLocalGlow);
		Slider("Glow intensity", &g_Config.m_PrismLocalGlowIntensity, 0, 100);
		Color("Glow color", &g_Config.m_PrismLocalGlowColor, 1);
		Label("Other players");
		Toggle("Outline", &g_Config.m_PrismOtherOutline);
		Slider("Outline width", &g_Config.m_PrismOtherOutlineWidth, 1, 8);
		Color("Outline color", &g_Config.m_PrismOtherOutlineColor, 2);
		Toggle("Glow", &g_Config.m_PrismOtherGlow);
		Slider("Glow intensity", &g_Config.m_PrismOtherGlowIntensity, 0, 100);
		Color("Glow color", &g_Config.m_PrismOtherGlowColor, 3);
		break;
	case 3:
		Label("Local player");
		Toggle("Custom hook color", &g_Config.m_PrismLocalHook);
		Color("Hook and glow color", &g_Config.m_PrismLocalHookColor, 4);
		Toggle("Hook glow", &g_Config.m_PrismLocalHookGlow);
		Slider("Glow intensity", &g_Config.m_PrismLocalHookIntensity, 0, 100);
		Label("Other players");
		Toggle("Custom hook color", &g_Config.m_PrismOtherHook);
		Color("Hook and glow color", &g_Config.m_PrismOtherHookColor, 5);
		Toggle("Hook glow", &g_Config.m_PrismOtherHookGlow);
		Slider("Glow intensity", &g_Config.m_PrismOtherHookIntensity, 0, 100);
		break;
	case 4:
		Label("Display and motion");
		Slider("Menu scale", &g_Config.m_PrismMenuScale, 80, 120);
		Slider("Glass opacity", &g_Config.m_PrismPanelOpacity, 50, 100);
		Toggle("Reduce animation", &g_Config.m_PrismReducedMotion);
		Label("Glass uses lightweight layered transparency.");
		break;
	case 5:
		Label("Rendering and monitoring");
		Toggle("Enable Prism visuals", &g_Config.m_PrismEnabled);
		Toggle("FPS and average frame time", &g_Config.m_PrismOverlay);
		Toggle("Local Tee glow", &g_Config.m_PrismLocalGlow);
		Toggle("Other Tee glow", &g_Config.m_PrismOtherGlow);
		Toggle("Local Hook glow", &g_Config.m_PrismLocalHookGlow);
		Toggle("Other Hook glow", &g_Config.m_PrismOtherHookGlow);
		Label("No framebuffer blur or per-frame allocations.");
		break;
	}
	Scroll.End();
	Footer.Draw(ColorRGBA(0.65f, 0.75f, 0.85f, 0.065f), IGraphics::CORNER_ALL, 8.0f);
	Ui()->DoLabel(&Footer, "PRISM 0.1  /  Insert or Esc to close", 11.0f, TEXTALIGN_MC);
	Prism::Validate(g_Config);
}
