// Prism additions, distributed under the zlib license in license.txt.
#include "menus.h"
#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

#include <engine/shared/config.h>
#include <game/client/prism.h>
#include <game/client/prism_qol.h>
#include <game/client/prism_theme.h>
#include <game/client/prism_ui.h>
#include <game/client/prism_version.h>
#include <game/client/ui_scrollregion.h>

void CMenus::RenderSettingsPrism(CUIRect Screen)
{
	Prism::Validate(g_Config);
	const bool Animate = g_Config.m_PrismAnimations && !g_Config.m_PrismReducedMotion && g_Config.m_PrismThemeAnimation > 0;
	const float Duration = std::max(0.01f, g_Config.m_PrismThemeAnimation / 1000.0f);
	const float Motion = Animate ? 1.0f - std::exp(-std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f) * 4.0f / Duration) : 1.0f;
	m_PrismTransition += ((m_PrismOpen ? 1.0f : 0.0f) - m_PrismTransition) * Motion;
	if(!m_PrismOpen && m_PrismTransition < 0.01f)
	{
		m_PrismTransition = 0.0f;
		return;
	}

	const float Fade = m_PrismTransition * m_PrismTransition * (3.0f - 2.0f * m_PrismTransition);
	const PrismUi::STheme Theme(g_Config);
	const float Opacity = g_Config.m_PrismPanelOpacity / 100.0f;
	const float Scale = g_Config.m_PrismMenuScale / 100.0f;
	const float BackdropDarkness = g_Config.m_PrismHudEdit && m_PrismCategory == 1 ? 0.28f : g_Config.m_PrismGlassDarkness / 100.0f;
	Screen.Draw(ColorRGBA(0.025f, 0.029f, 0.034f, BackdropDarkness * Fade), IGraphics::CORNER_NONE, 0.0f);

	CUIRect Panel = Screen;
	Panel.w = std::max(1.0f, std::min(Screen.w - 24.0f, 620.0f * Scale));
	Panel.h = std::max(1.0f, std::min(Screen.h - 24.0f, 410.0f * Scale));
	const float TravelX = std::max(0.0f, Screen.w - Panel.w);
	const float TravelY = std::max(0.0f, Screen.h - Panel.h);
	Panel.x = Screen.x + TravelX * g_Config.m_PrismMenuX / 10000.0f;
	Panel.y = Screen.y + TravelY * g_Config.m_PrismMenuY / 10000.0f;
	if(m_PrismOpen && !g_Config.m_PrismMenuLock)
	{
		CUIRect Grab = Panel;
		Grab.h = std::min(38.0f, Panel.h);
		Grab.w = std::max(0.0f, Grab.w - 75.0f);
		if(!m_PrismDragging && Ui()->ActiveItem() == nullptr && Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&Grab))
		{
			m_PrismDragging = true;
			m_PrismDragOffset = Ui()->MousePos() - Panel.TopLeft();
			Ui()->SetActiveItem(&m_PrismDragging);
		}
		if(m_PrismDragging && Ui()->MouseButton(0))
		{
			Ui()->CheckActiveItem(&m_PrismDragging);
			float X = std::clamp(Ui()->MouseX() - m_PrismDragOffset.x - Screen.x, 0.0f, TravelX);
			float Y = std::clamp(Ui()->MouseY() - m_PrismDragOffset.y - Screen.y, 0.0f, TravelY);
			if(g_Config.m_PrismMenuSnap)
			{
				X = PrismQol::HudSnap(X, Screen.w, Panel.w, 8.0f);
				Y = PrismQol::HudSnap(Y, Screen.h, Panel.h, 8.0f);
			}
			g_Config.m_PrismMenuX = TravelX > 0 ? std::clamp((int)std::round(X / TravelX * 10000.0f), 0, 10000) : 5000;
			g_Config.m_PrismMenuY = TravelY > 0 ? std::clamp((int)std::round(Y / TravelY * 10000.0f), 0, 10000) : 5000;
			Panel.x = Screen.x + X;
			Panel.y = Screen.y + Y;
		}
		else if(m_PrismDragging)
		{
			m_PrismDragging = false;
			Ui()->SetActiveItem(nullptr);
		}
	}
	else if(m_PrismDragging)
	{
		m_PrismDragging = false;
		Ui()->SetActiveItem(nullptr);
	}
	const float VisualY = (1.0f - Fade) * 9.0f;
	Panel.y += VisualY;
	CUIRect Shadow = Panel;
	Shadow.x -= 8.0f;
	Shadow.y -= 3.0f;
	Shadow.w += 16.0f;
	Shadow.h += 17.0f;
	const float Radius = g_Config.m_PrismThemeRounding;
	Shadow.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, g_Config.m_PrismThemeShadow / 100.0f * Fade), IGraphics::CORNER_ALL, Radius + 3.0f);
	CUIRect Edge = Panel;
	Edge.Margin(-1.0f, &Edge);
	Edge.Draw(Theme.m_Text.WithAlpha(g_Config.m_PrismThemeBorder / 500.0f * Fade), IGraphics::CORNER_ALL, Radius + 1.0f);
	Panel.Draw(Theme.m_Panel.WithAlpha((0.38f + 0.61f * Opacity) * Fade), IGraphics::CORNER_ALL, Radius);
	CUIRect Sheen;
	Panel.HSplitTop(41.0f, &Sheen, nullptr);
	const float Tint = g_Config.m_PrismGlassTint / 100.0f;
	Sheen.Draw4(Theme.m_Accent.WithAlpha(Tint * 0.20f * Fade), Theme.m_Accent.WithAlpha(Tint * 0.07f * Fade),
		Theme.m_Panel.WithAlpha(0.02f * Fade), Theme.m_Panel.WithAlpha(0.02f * Fade), IGraphics::CORNER_T, 11.0f);
	if(m_PrismOpen && m_PrismCategory == 1 && g_Config.m_PrismHudEdit && g_Config.m_PrismHudEnabled)
	{
		int *apEnabled[] = {&g_Config.m_PrismHudHotkeys, &g_Config.m_PrismHudIdentity, &g_Config.m_PrismHudPerformance,
			&g_Config.m_PrismHudDummy, &g_Config.m_PrismHudStaff, &g_Config.m_PrismHudInput, &g_Config.m_PrismHudEffects};
		int *apX[] = {&g_Config.m_PrismHudHotkeysX, &g_Config.m_PrismHudIdentityX, &g_Config.m_PrismHudPerformanceX,
			&g_Config.m_PrismHudDummyX, &g_Config.m_PrismHudStaffX, &g_Config.m_PrismHudInputX, &g_Config.m_PrismHudEffectsX};
		int *apY[] = {&g_Config.m_PrismHudHotkeysY, &g_Config.m_PrismHudIdentityY, &g_Config.m_PrismHudPerformanceY,
			&g_Config.m_PrismHudDummyY, &g_Config.m_PrismHudStaffY, &g_Config.m_PrismHudInputY, &g_Config.m_PrismHudEffectsY};
		int *apScale[] = {&g_Config.m_PrismHudHotkeysScale, &g_Config.m_PrismHudIdentityScale,
			&g_Config.m_PrismHudPerformanceScale, &g_Config.m_PrismHudDummyScale, &g_Config.m_PrismHudStaffScale,
			&g_Config.m_PrismHudInputScale, &g_Config.m_PrismHudEffectsScale};
		static int s_aHudDragIds[PrismQol::NUM_HUD_MODULES] = {};
		const float ToUi = Screen.h / 300.0f;
		if(!Ui()->MouseButton(0))
		{
			if(m_PrismHudDragIndex >= 0)
				Ui()->SetActiveItem(nullptr);
			m_PrismHudDragIndex = -1;
		}
		for(int i = PrismQol::NUM_HUD_MODULES - 1; i >= 0; --i)
		{
			if(!*apEnabled[i])
				continue;
			float Width, Height;
			PrismQol::HudModuleExtent(i, g_Config.m_PrismHudScale, *apScale[i], g_Config.m_PrismHudPadding,
				g_Config.m_PrismHudFontSize, g_Config.m_PrismInputKeySize, Width, Height);
			CUIRect Handle;
			Handle.w = std::min(Width * ToUi, Screen.w);
			Handle.h = std::min(Height * ToUi, Screen.h);
			Handle.x = Screen.x + PrismQol::HudCoordinate(*apX[i], Screen.w, Handle.w);
			Handle.y = Screen.y + PrismQol::HudCoordinate(*apY[i], Screen.h, Handle.h);
			if(!g_Config.m_PrismHudLayoutLock && m_PrismHudDragIndex == -1 && Ui()->ActiveItem() == nullptr &&
				Ui()->MouseButtonClicked(0) && !Panel.Inside(Ui()->MousePos()) && Ui()->MouseInside(&Handle))
			{
				m_PrismHudDragIndex = i;
				m_PrismHudDragOffset = Ui()->MousePos() - Handle.TopLeft();
				Ui()->SetActiveItem(&s_aHudDragIds[i]);
			}
			if(m_PrismHudDragIndex == i && Ui()->MouseButton(0) && !g_Config.m_PrismHudLayoutLock)
			{
				Ui()->CheckActiveItem(&s_aHudDragIds[i]);
				float X = std::clamp(Ui()->MouseX() - m_PrismHudDragOffset.x - Screen.x, 0.0f, Screen.w - Handle.w);
				float Y = std::clamp(Ui()->MouseY() - m_PrismHudDragOffset.y - Screen.y, 0.0f, Screen.h - Handle.h);
				if(g_Config.m_PrismHudEdgeSnap)
				{
					X = PrismQol::HudSnap(X, Screen.w, Handle.w, 7.0f);
					Y = PrismQol::HudSnap(Y, Screen.h, Handle.h, 7.0f);
				}
				if(g_Config.m_PrismHudSnap)
				{
					for(int Other = 0; Other < PrismQol::NUM_HUD_MODULES; ++Other)
					{
						if(Other == i || !*apEnabled[Other])
							continue;
						float OtherWidth, OtherHeight;
						PrismQol::HudModuleExtent(Other, g_Config.m_PrismHudScale, *apScale[Other], g_Config.m_PrismHudPadding,
							g_Config.m_PrismHudFontSize, g_Config.m_PrismInputKeySize, OtherWidth, OtherHeight);
						OtherWidth = std::min(OtherWidth * ToUi, Screen.w);
						OtherHeight = std::min(OtherHeight * ToUi, Screen.h);
						const float OtherX = PrismQol::HudCoordinate(*apX[Other], Screen.w, OtherWidth);
						const float OtherY = PrismQol::HudCoordinate(*apY[Other], Screen.h, OtherHeight);
						const float GuidesX[] = {OtherX, OtherX + (OtherWidth - Handle.w) / 2.0f, OtherX + OtherWidth - Handle.w};
						const float GuidesY[] = {OtherY, OtherY + (OtherHeight - Handle.h) / 2.0f, OtherY + OtherHeight - Handle.h};
						for(float Guide : GuidesX)
							if(std::abs(X - Guide) < 4.0f)
								X = std::clamp(Guide, 0.0f, Screen.w - Handle.w);
						for(float Guide : GuidesY)
							if(std::abs(Y - Guide) < 4.0f)
								Y = std::clamp(Guide, 0.0f, Screen.h - Handle.h);
					}
				}
				*apX[i] = PrismQol::HudNormalize(X, Screen.w);
				*apY[i] = PrismQol::HudNormalize(Y, Screen.h);
				g_Config.m_PrismHudPreset = PrismQol::HUD_CUSTOM;
				Handle.x = Screen.x + PrismQol::HudCoordinate(*apX[i], Screen.w, Handle.w);
				Handle.y = Screen.y + PrismQol::HudCoordinate(*apY[i], Screen.h, Handle.h);
			}
			const ColorRGBA OutlineColor = Theme.m_Accent.WithAlpha(m_PrismHudDragIndex == i ? 0.75f : 0.38f);
			const float Stroke = 1.0f;
			CUIRect Border = {Handle.x, Handle.y, Handle.w, Stroke};
			Border.Draw(OutlineColor, IGraphics::CORNER_NONE, 0.0f);
			Border.y = Handle.y + Handle.h - Stroke;
			Border.Draw(OutlineColor, IGraphics::CORNER_NONE, 0.0f);
			Border = {Handle.x, Handle.y, Stroke, Handle.h};
			Border.Draw(OutlineColor, IGraphics::CORNER_NONE, 0.0f);
			Border.x = Handle.x + Handle.w - Stroke;
			Border.Draw(OutlineColor, IGraphics::CORNER_NONE, 0.0f);
		}
	}
	else
		m_PrismHudDragIndex = -1;

	// Closing is purely visual: release interactive items immediately.
	if(!m_PrismOpen)
		return;

	CUIRect Inner = Panel;
	Inner.Margin(11.0f, &Inner);
	CUIRect Header, Body, Footer;
	Inner.HSplitTop(35.0f, &Header, &Inner);
	Inner.HSplitBottom(19.0f, &Body, &Footer);
	CUIRect Title, Close;
	Header.VSplitRight(67.0f, &Title, &Close);
	const float PreviousTextScaleX = TextRender()->GetTextScaleX();
	TextRender()->SetFontPreset(EFontPreset::PRISM_HEADING);
	TextRender()->SetTextScaleX(g_Config.m_PrismThemeHeadingWidth / 100.0f);
	Ui()->DoLabel(&Title, "PRISM", 15.0f, TEXTALIGN_ML);
	TextRender()->SetTextScaleX(PreviousTextScaleX);
	TextRender()->SetFontPreset(EFontPreset::PRISM_BODY);
	static CButtonContainer s_Close;
	if(DoButton_Menu(&s_Close, "Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.46f, ColorRGBA(0.35f, 0.43f, 0.51f, 0.22f)))
	{
		Ui()->ClosePopupMenus();
		m_PrismOpen = false;
	}

	CUIRect Sidebar, Content;
	Body.VSplitLeft(std::min(116.0f * Scale, Body.w * 0.25f), &Sidebar, &Content);
	Sidebar.VSplitRight(7.0f, &Sidebar, nullptr);
	Sidebar.Draw(Theme.m_Background.WithAlpha(0.91f * Fade), IGraphics::CORNER_ALL, 8.0f);
	Sidebar.Margin(6.0f, &Sidebar);
	Content.Draw(Theme.m_Background.WithAlpha(0.83f * Fade), IGraphics::CORNER_ALL, 8.0f);
	Content.Margin(8.0f, &Content);

	static const char *s_apTabs[] = {"Visuals", "HUD", "Input", "QoL", "Macros", "Themes", "Settings", "Assist"};
	static CButtonContainer s_aTabs[8];
	static float s_aTabBlend[8] = {};
	for(int i = 0; i < 8; ++i)
	{
		CUIRect Tab;
		Sidebar.HSplitTop(29.0f, &Tab, &Sidebar);
		Sidebar.HSplitTop(3.0f, nullptr, &Sidebar);
		const float Target = m_PrismCategory == i ? 1.0f : 0.0f;
		s_aTabBlend[i] += (Target - s_aTabBlend[i]) * Motion;
		const float Highlight = s_aTabBlend[i];
		const ColorRGBA ButtonColor(Theme.m_Accent.r, Theme.m_Accent.g, Theme.m_Accent.b, 0.06f + Highlight * 0.36f);
		if(DoButton_Menu(&s_aTabs[i], s_apTabs[i], 0, &Tab, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.41f, ButtonColor))
		{
			m_PrismCategory = i;
			Ui()->SetActiveItem(nullptr);
		}
	}

	static CScrollRegion s_aScroll[8];
	CUIRect ScrollView = Content;
	CScrollRegion &Scroll = s_aScroll[m_PrismCategory];
	Scroll.Begin(&ScrollView);
	auto NextRow = [&](float Height = 28.0f) {
		CUIRect Row;
		ScrollView.HSplitTop(Height, &Row, &ScrollView);
		Scroll.AddRect(Row);
		ScrollView.HSplitTop(3.0f, nullptr, &ScrollView);
		return Row;
	};
	auto Label = [&](const char *pLabel) {
		CUIRect Row = NextRow(26.0f);
		if(!Scroll.RectClipped(Row))
			Ui()->DoLabel(&Row, pLabel, 12.0f, TEXTALIGN_ML);
	};
	static float s_aaToggleProgress[8][96] = {};
	static float s_aaToggleHover[8][96] = {};
	static bool s_aaToggleInitialized[8][96] = {};
	int ToggleIndex = 0;
	auto Toggle = [&](const char *pLabel, int *pValue) {
		CUIRect Row = NextRow(29.0f);
		const int Index = ToggleIndex++;
		if(Scroll.RectClipped(Row))
			return;
		const int Slot = std::min(Index, 95);
		float &Hover = s_aaToggleHover[m_PrismCategory][Slot];
		Hover += ((Ui()->MouseInside(&Row) ? 1.0f : 0.0f) - Hover) * Motion;
		Row.Draw(Theme.m_Panel.WithAlpha(0.72f + 0.16f * Hover), IGraphics::CORNER_ALL, 6.0f);
		CUIRect LabelRect, Switch;
		Row.VSplitRight(39.0f, &LabelRect, &Switch);
		LabelRect.VMargin(8.0f, &LabelRect);
		Ui()->DoLabel(&LabelRect, pLabel, 11.0f, TEXTALIGN_ML);
		Switch.VMargin(5.0f, &Switch);
		Switch.HMargin(6.0f, &Switch);
		const float Target = *pValue ? 1.0f : 0.0f;
		float &Progress = s_aaToggleProgress[m_PrismCategory][Slot];
		if(!s_aaToggleInitialized[m_PrismCategory][Slot])
		{
			Progress = Target;
			s_aaToggleInitialized[m_PrismCategory][Slot] = true;
		}
		Progress += (Target - Progress) * Motion;
		const float Position = Progress;
		Switch.Draw(ColorRGBA(Theme.m_Panel.r + (Theme.m_Accent.r - Theme.m_Panel.r) * Position,
			Theme.m_Panel.g + (Theme.m_Accent.g - Theme.m_Panel.g) * Position,
			Theme.m_Panel.b + (Theme.m_Accent.b - Theme.m_Panel.b) * Position, 0.9f), IGraphics::CORNER_ALL, 6.0f);
		CUIRect Knob = Switch;
		Knob.w = 11.0f;
		Knob.h = 11.0f;
		Knob.x += 2.0f + Position * (Switch.w - Knob.w - 4.0f);
		Knob.y = Switch.y + (Switch.h - Knob.h) * 0.5f;
		Knob.Draw(ColorRGBA(0.94f, 0.96f, 0.98f, 1.0f), IGraphics::CORNER_ALL, 5.0f);
		if(Ui()->DoButtonLogic(pValue, *pValue, &Row, BUTTONFLAG_LEFT))
			*pValue ^= 1;
	};
	auto Slider = [&](const char *pLabel, int *pValue, int Min, int Max) {
		CUIRect Row = NextRow(31.0f);
		if(!Scroll.RectClipped(Row))
		{
			Row.Draw(Theme.m_Panel.WithAlpha(0.72f), IGraphics::CORNER_ALL, 6.0f);
			Row.Margin(5.0f, &Row);
			Ui()->DoScrollbarOption(pValue, pValue, &Row, pLabel, Min, Max);
		}
	};
	static CButtonContainer s_aColors[16];
	auto Color = [&](const char *pLabel, unsigned *pValue, int Index) {
		CUIRect Row = NextRow(27.0f);
		if(!Scroll.RectClipped(Row))
			DoLine_ColorPicker(&s_aColors[Index], 24.0f, 12.0f, 3.0f, &Row, pLabel, pValue, ColorRGBA(0.65f, 0.78f, 0.91f, 1.0f), false, nullptr, Index == 9 || Index == 10);
	};


    // Phase 3 navigation and editors. All controls edit saved DDNet configuration.
    // Existing visual presets affect visuals only; all QoL preferences are separate.
    static CButtonContainer s_aActionButtons[32];
    auto Button = [&](const char *pText, int Id, const ColorRGBA &Tint = ColorRGBA(0.37f, 0.47f, 0.58f, 0.25f)) {
        CUIRect Row = NextRow(29.0f);
        return !Scroll.RectClipped(Row) && DoButton_Menu(&s_aActionButtons[Id], pText, 0, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.41f, Tint);
    };
    auto Bind = [&](const char *pLabel, int *pValue, int Target, int ButtonId) {
        char aText[128];
        str_format(aText, sizeof(aText), "%s: %s", pLabel,
            m_PrismCaptureBind == Target ? "PRESS A KEY (Esc unbinds)" : (*pValue ? Input()->KeyName(*pValue) : "Unbound"));
        if(Button(aText, ButtonId)) m_PrismCaptureBind = Target;
    };
    switch(m_PrismCategory)
    {
    case 7: // Bounded local prediction and input assistance
        Label("Assist   /   local prediction");
        Toggle("Hook Assist (hold Hook)", &g_Config.m_PrismHookAssist);
        Slider("Hook target FOV (degrees)", &g_Config.m_PrismHookFov, 5, 180);
        Slider("Hook target range", &g_Config.m_PrismHookRange, 64, 1200);
        Slider("Hook correction (%)", &g_Config.m_PrismHookStrength, 1, 100);
        Slider("Lead target (ticks)", &g_Config.m_PrismHookPrediction, 0, 12);
        Toggle("Hook Assist debug", &g_Config.m_PrismHookDebug);
        Label("Freeze Avoid   /   only when needed");
        if(Button(g_Config.m_PrismFreezeAvoid == 0 ? "Freeze Avoid: Off" :
                g_Config.m_PrismFreezeAvoid == 1 ? "Freeze Avoid: Warning" : "Freeze Avoid: Assist", 30))
            g_Config.m_PrismFreezeAvoid = (g_Config.m_PrismFreezeAvoid + 1) % 3;
        Slider("Prediction horizon (ticks)", &g_Config.m_PrismFreezeHorizon, 4, 24);
        Toggle("Show trajectory debug", &g_Config.m_PrismFreezeDebug);
        Label("Manual direction and macros retain priority.");
        break;
    case 6: // Settings and status
    {
        Label("Prism   /   Control center");
        Toggle("Enable Prism visuals", &g_Config.m_PrismEnabled);
        Toggle("Double Tee assistant", &g_Config.m_PrismDoubleEnabled);
        Toggle("Modular HUD", &g_Config.m_PrismHudEnabled);
        char aStatus[128];
        str_format(aStatus, sizeof(aStatus), "Dummy: %s   |   Active macros: %d",
            Client()->DummyConnected() ? "connected" : "offline", GameClient()->m_PrismMacros.ActiveCount());
        Label(aStatus);
        Label("Insert / Esc: close     F12: emergency stop");
        if(Button("EMERGENCY STOP — release Prism inputs", 0, ColorRGBA(0.65f, 0.28f, 0.29f, 0.35f)))
            GameClient()->PrismEmergencyStop();
        Label("Performance   /   measured locally");
        Toggle("FPS and frame-time overlay", &g_Config.m_PrismOverlay);
        Toggle("Performance HUD panel", &g_Config.m_PrismHudPerformance);
        Slider("Menu scale", &g_Config.m_PrismMenuScale, 80, 120);
        Slider("Glass opacity", &g_Config.m_PrismPanelOpacity, 50, 100);
        Slider("Background darkness", &g_Config.m_PrismGlassDarkness, 0, 100);
        Slider("Glass tint", &g_Config.m_PrismGlassTint, 0, 100);
        Toggle("Animations", &g_Config.m_PrismAnimations);
        Slider("Animation duration (ms)", &g_Config.m_PrismThemeAnimation, 0, 5000);
        Toggle("Reduce animation", &g_Config.m_PrismReducedMotion);
        Toggle("Lock ClickGUI position", &g_Config.m_PrismMenuLock);
        Toggle("Snap ClickGUI to edges", &g_Config.m_PrismMenuSnap);
        if(Button("Reset ClickGUI position", 23))
        {
            g_Config.m_PrismMenuX = 5000;
            g_Config.m_PrismMenuY = 5000;
        }
        if(Button(g_Config.m_PrismMenuCursor ? "Menu cursor: System" : "Menu cursor: DDNet", 28))
            g_Config.m_PrismMenuCursor ^= 1;
        if(Button("Reset visual presets only", 15)) Prism::Reset(g_Config);
        break;
    }
    case 0: // Visuals, including presets and all Tee/Hook adjustments
    {
        Label("Visual presets");
        static const char *s_apPresets[] = {"Default", "Clean", "Competitive", "Cinematic", "Custom"};
        static CButtonContainer s_aPresetButtons[5];
        for(int i = 0; i < 5; ++i)
        {
            CUIRect Row = NextRow(36.0f);
            if(!Scroll.RectClipped(Row) && DoButton_Menu(&s_aPresetButtons[i], s_apPresets[i], 0, &Row, BUTTONFLAG_LEFT, nullptr,
                IGraphics::CORNER_ALL, 9.0f, 0.45f, g_Config.m_PrismPreset == i ? ColorRGBA(0.39f, 0.56f, 0.71f, 0.43f) : ColorRGBA(0.34f, 0.42f, 0.50f, 0.18f)))
                Prism::ApplyPreset(g_Config, i);
        }
        Label("Tee   /   local player");
        Toggle("Outline", &g_Config.m_PrismLocalOutline);
        Slider("Outline width", &g_Config.m_PrismLocalOutlineWidth, 1, 8);
        Color("Outline color", &g_Config.m_PrismLocalOutlineColor, 0);
        Toggle("Glow", &g_Config.m_PrismLocalGlow);
        Slider("Glow strength", &g_Config.m_PrismLocalGlowIntensity, 0, 100);
        Color("Glow color", &g_Config.m_PrismLocalGlowColor, 1);
        Label("Tee   /   other players");
        Toggle("Outline", &g_Config.m_PrismOtherOutline);
        Slider("Outline width", &g_Config.m_PrismOtherOutlineWidth, 1, 8);
        Color("Outline color", &g_Config.m_PrismOtherOutlineColor, 2);
        Toggle("Glow", &g_Config.m_PrismOtherGlow);
        Slider("Glow strength", &g_Config.m_PrismOtherGlowIntensity, 0, 100);
        Color("Glow color", &g_Config.m_PrismOtherGlowColor, 3);
        Label("Hook   /   local player");
        Toggle("Custom color", &g_Config.m_PrismLocalHook);
        Color("Hook color", &g_Config.m_PrismLocalHookColor, 4);
        Toggle("Glow", &g_Config.m_PrismLocalHookGlow);
        Slider("Glow strength", &g_Config.m_PrismLocalHookIntensity, 0, 100);
        Label("Hook   /   other players");
        Toggle("Custom color", &g_Config.m_PrismOtherHook);
        Color("Hook color", &g_Config.m_PrismOtherHookColor, 5);
        Toggle("Glow", &g_Config.m_PrismOtherHookGlow);
        Slider("Glow strength", &g_Config.m_PrismOtherHookIntensity, 0, 100);
        Label("Hook particles  /  cosmetic only");
        if(Button(g_Config.m_PrismHookEffect == 0 ? "Mode: Off" :
            g_Config.m_PrismHookEffect == 1 ? "Mode: Leaves" :
            g_Config.m_PrismHookEffect == 2 ? "Mode: Sparkles" :
            g_Config.m_PrismHookEffect == 3 ? "Mode: Hearts" : "Mode: Pulse", 16))
            g_Config.m_PrismHookEffect = (g_Config.m_PrismHookEffect + 1) % 5;
        Toggle("Local hook", &g_Config.m_PrismHookEffectLocal);
        Toggle("Other visible hooks", &g_Config.m_PrismHookEffectOthers);
        Color("Particle color", &g_Config.m_PrismHookParticleColor, 6);
        Slider("Particle rate", &g_Config.m_PrismHookParticleRate, 1, 120);
        Slider("Particle cap", &g_Config.m_PrismHookParticleCap, 0, 512);
        Slider("Particle size", &g_Config.m_PrismHookParticleSize, 1, 24);
        Slider("Particle lifetime", &g_Config.m_PrismHookParticleLifetime, 100, 3000);
        Slider("Particle opacity", &g_Config.m_PrismHookParticleOpacity, 0, 100);
        Slider("Particle spread", &g_Config.m_PrismHookParticleSpread, 0, 80);
        Slider("Particle velocity", &g_Config.m_PrismHookParticleSpeed, 0, 100);
        Toggle("Particle fade", &g_Config.m_PrismHookParticleFade);
        Toggle("Particle glow", &g_Config.m_PrismHookParticleGlow);
        Slider("Pulse frequency", &g_Config.m_PrismHookPulseFrequency, 1, 60);
        Slider("Pulse intensity", &g_Config.m_PrismHookPulseIntensity, 0, 100);
        Label("Player trail");
        if(Button(g_Config.m_PrismTrail == 0 ? "Trail: Off" :
            g_Config.m_PrismTrail == 1 ? "Trail: Line" :
            g_Config.m_PrismTrail == 2 ? "Trail: Ribbon" : "Trail: Particles", 17))
            g_Config.m_PrismTrail = (g_Config.m_PrismTrail + 1) % 4;
        Toggle("Local trail", &g_Config.m_PrismTrailLocal);
        Toggle("Other visible trails", &g_Config.m_PrismTrailOthers);
        Color("Trail color", &g_Config.m_PrismTrailColor, 7);
        Slider("Trail width", &g_Config.m_PrismTrailWidth, 1, 24);
        Slider("Trail lifetime", &g_Config.m_PrismTrailLength, 50, 3000);
        Slider("Sample interval", &g_Config.m_PrismTrailInterval, 8, 100);
        Slider("Trail opacity", &g_Config.m_PrismTrailOpacity, 0, 100);
        Toggle("Trail fade", &g_Config.m_PrismTrailFade);
        Toggle("Trail glow", &g_Config.m_PrismTrailGlow);
        Label("Player highlight  /  normally visible Tees");
        Toggle("Highlight", &g_Config.m_PrismHighlight);
        Toggle("Local Tee", &g_Config.m_PrismHighlightLocal);
        Toggle("Other visible Tees", &g_Config.m_PrismHighlightOthers);
        Color("Highlight color", &g_Config.m_PrismHighlightColor, 8);
        Slider("Fill opacity", &g_Config.m_PrismHighlightFill, 0, 100);
        Slider("Outline opacity", &g_Config.m_PrismHighlightOpacity, 0, 100);
        Slider("Outline width", &g_Config.m_PrismHighlightWidth, 1, 8);
        Slider("Box scale", &g_Config.m_PrismHighlightScale, 50, 200);
        Slider("Corner radius", &g_Config.m_PrismHighlightRounding, 0, 24);
        Label("Effects quality");
        Slider("Quality: low / medium / high", &g_Config.m_PrismEffectQuality, 0, 2);
        break;
    }
    case 3: // QoL and Double Tee
        Label("Double Tee   /   input composition");
        Toggle("Assistant active", &g_Config.m_PrismDoubleEnabled);
        if(Button(g_Config.m_PrismDoubleMode == 0 ? "Mode: movement copying" :
            g_Config.m_PrismDoubleMode == 1 ? "Mode: Hammer Fly" : "Mode: combined copy + Hammer Fly", 1))
            g_Config.m_PrismDoubleMode = (g_Config.m_PrismDoubleMode + 1) % 3;
        Slider("Hammer interval (ticks)", &g_Config.m_PrismDoubleInterval, 5, 100);
        Bind("Activation key", &g_Config.m_PrismDoubleBind, 0, 2);
        Bind("Emergency stop key", &g_Config.m_PrismStopBind, 1, 3);
        Label("F12 always stops Prism automation.");
        Label("Manual movement takes priority. No physics changes.");
        if(Button("Stop now and release owned inputs", 4, ColorRGBA(0.65f, 0.28f, 0.29f, 0.35f)))
            GameClient()->PrismEmergencyStop();
        break;
    case 4: // Strictly bounded visual macro editor
    {
        Label("Macros   /   visual sequence editor");
        static int s_SelectedMacro = 0;
        static int s_SelectedStep = 0;
        static CButtonContainer s_aMacroSlots[PrismQol::NUM_MACROS];
        CUIRect Slots = NextRow(40.0f);
        if(!Scroll.RectClipped(Slots))
        {
            for(int i = 0; i < PrismQol::NUM_MACROS; ++i)
            {
                CUIRect Slot;
                Slots.VSplitLeft(Slots.w / (PrismQol::NUM_MACROS - i), &Slot, &Slots);
                char aName[20]; str_format(aName, sizeof(aName), "Macro %d", i + 1);
                if(DoButton_Menu(&s_aMacroSlots[i], aName, 0, &Slot, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f, 0.45f,
                    s_SelectedMacro == i ? ColorRGBA(0.42f, 0.58f, 0.69f, 0.40f) : ColorRGBA(0.34f, 0.41f, 0.49f, 0.20f)))
                { s_SelectedMacro = i; s_SelectedStep = 0; }
            }
        }
        char *apDefinition[] = {g_Config.m_PrismMacro1, g_Config.m_PrismMacro2, g_Config.m_PrismMacro3, g_Config.m_PrismMacro4};
        int *apEnabled[] = {&g_Config.m_PrismMacro1Enabled, &g_Config.m_PrismMacro2Enabled,
            &g_Config.m_PrismMacro3Enabled, &g_Config.m_PrismMacro4Enabled};
        int *apBind[] = {&g_Config.m_PrismMacro1Bind, &g_Config.m_PrismMacro2Bind,
            &g_Config.m_PrismMacro3Bind, &g_Config.m_PrismMacro4Bind};
        int *apMode[] = {&g_Config.m_PrismMacro1Mode, &g_Config.m_PrismMacro2Mode,
            &g_Config.m_PrismMacro3Mode, &g_Config.m_PrismMacro4Mode};
        Toggle("Enabled", apEnabled[s_SelectedMacro]);
        Bind("Trigger key", apBind[s_SelectedMacro], 2 + s_SelectedMacro, 5);
        if(Button(*apMode[s_SelectedMacro] == PrismQol::ONCE ? "Mode: Once" :
            *apMode[s_SelectedMacro] == PrismQol::HOLD ? "Mode: Hold" : "Mode: Toggle", 6))
            *apMode[s_SelectedMacro] = (*apMode[s_SelectedMacro] + 1) % 3;
        PrismQol::SSequence Sequence;
        if(!PrismQol::Parse(apDefinition[s_SelectedMacro], Sequence))
        {
            Label("Invalid saved sequence. Use Clear to reset.");
            if(Button("Clear invalid sequence", 7))
                apDefinition[s_SelectedMacro][0] = '\0';
            break;
        }
        s_SelectedStep = std::clamp(s_SelectedStep, 0, std::max(0, Sequence.m_Count - 1));
        static CButtonContainer s_aStepButtons[PrismQol::MAX_STEPS];
        Label("Steps   /   select to edit");
        for(int i = 0; i < Sequence.m_Count; ++i)
        {
            CUIRect StepRow = NextRow(33.0f);
            if(Scroll.RectClipped(StepRow)) continue;
            char aText[128];
            str_format(aText, sizeof(aText), "%d   %s   /   %d ms", i + 1,
                PrismQol::ACTION_NAMES[Sequence.m_aSteps[i].m_Action], Sequence.m_aSteps[i].m_DelayMs);
            if(DoButton_Menu(&s_aStepButtons[i], aText, 0, &StepRow, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f, 0.43f,
                s_SelectedStep == i ? ColorRGBA(0.40f, 0.54f, 0.67f, 0.38f) : ColorRGBA(0.35f, 0.43f, 0.52f, 0.18f)))
                s_SelectedStep = i;
        }
        bool Changed = false;
        if(Sequence.m_Count < PrismQol::MAX_STEPS && Button("+ Add step", 8))
        {
            Sequence.m_aSteps[Sequence.m_Count++] = {PrismQol::WAIT, 100};
            s_SelectedStep = Sequence.m_Count - 1;
            Changed = true;
        }
        if(Sequence.m_Count)
        {
            auto &Step = Sequence.m_aSteps[s_SelectedStep];
            char aAction[128];
            str_format(aAction, sizeof(aAction), "Action: %s   (click to cycle)", PrismQol::ACTION_NAMES[Step.m_Action]);
            if(Button(aAction, 9)) { Step.m_Action = (Step.m_Action + 1) % PrismQol::NUM_ACTIONS; Changed = true; }
            static int s_Delay = 100;
            static int s_PrevMacro = -1, s_PrevStep = -1;
            if(s_PrevMacro != s_SelectedMacro || s_PrevStep != s_SelectedStep || Ui()->ActiveItem() != &s_Delay)
                s_Delay = Step.m_DelayMs;
            s_PrevMacro = s_SelectedMacro; s_PrevStep = s_SelectedStep;
            Slider("Delay after action (ms)", &s_Delay, 0, PrismQol::MAX_DELAY_MS);
            if(s_Delay != Step.m_DelayMs) { Step.m_DelayMs = s_Delay; Changed = true; }
            if(s_SelectedStep > 0 && Button("Move step up", 10))
            { std::swap(Sequence.m_aSteps[s_SelectedStep], Sequence.m_aSteps[s_SelectedStep - 1]); --s_SelectedStep; Changed = true; }
            if(s_SelectedStep + 1 < Sequence.m_Count && Button("Move step down", 11))
            { std::swap(Sequence.m_aSteps[s_SelectedStep], Sequence.m_aSteps[s_SelectedStep + 1]); ++s_SelectedStep; Changed = true; }
            if(Button("Delete selected step", 12))
            {
                for(int i = s_SelectedStep; i + 1 < Sequence.m_Count; ++i)
                    Sequence.m_aSteps[i] = Sequence.m_aSteps[i + 1];
                --Sequence.m_Count;
                s_SelectedStep = std::clamp(s_SelectedStep, 0, std::max(0, Sequence.m_Count - 1));
                Changed = true;
            }
        }
        if(Button("Clear macro sequence", 13)) { Sequence = {}; s_SelectedStep = 0; Changed = true; }
        if(Changed)
        {
            char aEncoded[512];
            if(PrismQol::Encode(Sequence, aEncoded, sizeof(aEncoded)))
                str_copy(apDefinition[s_SelectedMacro], aEncoded, 512);
        }
        Label("Actions only affect game input. No shell or scripts.");
        break;
    }
    case 1: // HUD drag/drop + layout and per-module visibility
    {
        Label("Modular HUD   /   drag handles to reposition");
        Toggle("Enable all modules", &g_Config.m_PrismHudEnabled);
        Slider("HUD scale (%)", &g_Config.m_PrismHudScale, 65, 150);
        Slider("HUD opacity (%)", &g_Config.m_PrismHudOpacity, 20, 100);
        static const char *s_apModuleNames[] = {"Active Hotkeys", "Prism name/version", "Performance", "Dummy Status", "Verified Staff", "Input Overlay", "Effects Status"};
        int *apEnabled[] = {&g_Config.m_PrismHudHotkeys, &g_Config.m_PrismHudIdentity,
            &g_Config.m_PrismHudPerformance, &g_Config.m_PrismHudDummy, &g_Config.m_PrismHudStaff,
            &g_Config.m_PrismHudInput, &g_Config.m_PrismHudEffects};
        int *apX[] = {&g_Config.m_PrismHudHotkeysX, &g_Config.m_PrismHudIdentityX,
            &g_Config.m_PrismHudPerformanceX, &g_Config.m_PrismHudDummyX, &g_Config.m_PrismHudStaffX,
            &g_Config.m_PrismHudInputX, &g_Config.m_PrismHudEffectsX};
        int *apY[] = {&g_Config.m_PrismHudHotkeysY, &g_Config.m_PrismHudIdentityY,
            &g_Config.m_PrismHudPerformanceY, &g_Config.m_PrismHudDummyY, &g_Config.m_PrismHudStaffY,
            &g_Config.m_PrismHudInputY, &g_Config.m_PrismHudEffectsY};
        int *apScale[] = {&g_Config.m_PrismHudHotkeysScale, &g_Config.m_PrismHudIdentityScale,
            &g_Config.m_PrismHudPerformanceScale, &g_Config.m_PrismHudDummyScale, &g_Config.m_PrismHudStaffScale,
            &g_Config.m_PrismHudInputScale, &g_Config.m_PrismHudEffectsScale};
        static const char *s_apHudPresets[] = {"Minimal", "Streaming", "Competitive", "Custom"};
        for(int Preset = 0; Preset < 4; ++Preset)
        {
            if(Button(s_apHudPresets[Preset], 24 + Preset))
            {
                if(Preset != PrismQol::HUD_CUSTOM)
                {
                    const auto Layout = PrismQol::HudPreset(Preset);
                    for(int i = 0; i < PrismQol::NUM_HUD_MODULES; ++i)
                    {
                        *apEnabled[i] = Layout.m_aEnabled[i];
                        *apX[i] = Layout.m_aX[i];
                        *apY[i] = Layout.m_aY[i];
                    }
                }
                g_Config.m_PrismHudPreset = Preset;
            }
        }
        for(int i = 0; i < 7; ++i) Toggle(s_apModuleNames[i], apEnabled[i]);
        Slider("Module scale: Active Hotkeys", apScale[0], 65, 150);
        Slider("Module scale: Identity", apScale[1], 65, 150);
        Slider("Module scale: Performance", apScale[2], 65, 150);
        Slider("Module scale: Dummy", apScale[3], 65, 150);
        Slider("Module scale: Staff", apScale[4], 65, 150);
        Slider("Module scale: Input", apScale[5], 65, 150);
        Slider("Module scale: Effects", apScale[6], 65, 150);
        Toggle("HUD background", &g_Config.m_PrismHudBackground);
        Toggle("Snap to guides", &g_Config.m_PrismHudSnap);
        Toggle("Snap to screen edges", &g_Config.m_PrismHudEdgeSnap);
        Toggle("Lock HUD layout", &g_Config.m_PrismHudLayoutLock);
        Toggle("Drag widgets on game screen", &g_Config.m_PrismHudEdit);
        Toggle("Show guides", &g_Config.m_PrismHudGuides);
        Slider("Padding", &g_Config.m_PrismHudPadding, 0, 16);
        Slider("Rounding", &g_Config.m_PrismHudRounding, 0, 12);
        Slider("Font size", &g_Config.m_PrismHudFontSize, 5, 12);
        Label("Physical input overlay");
        Slider("Key size", &g_Config.m_PrismInputKeySize, 12, 32);
        Slider("Key rounding", &g_Config.m_PrismInputRounding, 0, 12);
        Toggle("Action labels", &g_Config.m_PrismInputLabels);
        Toggle("Key animation", &g_Config.m_PrismInputAnimation);
        Color("Pressed key", &g_Config.m_PrismInputActiveColor, 9);
        Color("Released key", &g_Config.m_PrismInputInactiveColor, 10);
        Label("Layout preview  /  drag enabled modules");
        CUIRect Preview = NextRow(165.0f);
        if(!Scroll.RectClipped(Preview))
        {
            Preview.Draw(ColorRGBA(0.018f, 0.031f, 0.047f, 0.75f), IGraphics::CORNER_ALL, 9.0f);
            static CButtonContainer s_aPins[7];
            static int s_DragIndex = -1;
            static float s_DragOffsetX = 0.0f, s_DragOffsetY = 0.0f;
            if(!Ui()->MouseButton(0))
            { s_DragIndex = -1; }
            for(int i = 0; i < 7; ++i)
            {
                if(!*apEnabled[i]) continue;
                CUIRect Pin;
                Pin.w = 82.0f; Pin.h = 18.0f;
                Pin.x = Preview.x + PrismQol::HudCoordinate(*apX[i], Preview.w, Pin.w);
                Pin.y = Preview.y + PrismQol::HudCoordinate(*apY[i], Preview.h, Pin.h);
				if(!g_Config.m_PrismHudLayoutLock && s_DragIndex == -1 && Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&Pin))
                {
                    s_DragIndex = i;
                    s_DragOffsetX = Ui()->MouseX() - Pin.x;
                    s_DragOffsetY = Ui()->MouseY() - Pin.y;
                    Ui()->SetActiveItem(&s_aPins[i]);
                }
				if(s_DragIndex == i && Ui()->MouseButton(0) && !g_Config.m_PrismHudLayoutLock)
                {
                    Ui()->CheckActiveItem(&s_aPins[i]);
                    float NewX = Ui()->MouseX() - s_DragOffsetX - Preview.x;
                    float NewY = Ui()->MouseY() - s_DragOffsetY - Preview.y;
                    if(g_Config.m_PrismHudSnap)
                    {
                        NewX = PrismQol::HudSnap(NewX, Preview.w, Pin.w);
                        NewY = PrismQol::HudSnap(NewY, Preview.h, Pin.h);
                    }
                    *apX[i] = PrismQol::HudNormalize(NewX, Preview.w);
                    *apY[i] = PrismQol::HudNormalize(NewY, Preview.h);
                    g_Config.m_PrismHudPreset = PrismQol::HUD_CUSTOM;
                    Pin.x = Preview.x + PrismQol::HudCoordinate(*apX[i], Preview.w, Pin.w);
                    Pin.y = Preview.y + PrismQol::HudCoordinate(*apY[i], Preview.h, Pin.h);
                }
                Pin.Draw(s_DragIndex == i ? ColorRGBA(0.54f, 0.70f, 0.85f, 0.65f) :
                    ColorRGBA(0.29f, 0.40f, 0.51f, 0.80f), IGraphics::CORNER_ALL, 5.0f);
                Ui()->DoLabel(&Pin, s_apModuleNames[i], 8.0f, TEXTALIGN_MC);
            }
        }
        if(Button("Reset HUD layout", 14))
        {
            const int aX[] = {300, 300, 8200, 300, 8200, 300, 7600};
            const int aY[] = {900, 250, 250, 1600, 950, 7500, 1800};
            for(int i = 0; i < 7; ++i) { *apX[i] = aX[i]; *apY[i] = aY[i]; }
            g_Config.m_PrismHudScale = 100;
            g_Config.m_PrismHudOpacity = 85;
        }
        Label("Staff appears only with server-supplied auth data.");
        break;
    }
    case 2: // Physical input overlay
        Label("Input overlay  /  physical controls");
        Toggle("Show input overlay", &g_Config.m_PrismHudInput);
        Slider("Key size", &g_Config.m_PrismInputKeySize, 12, 32);
        Slider("Key rounding", &g_Config.m_PrismInputRounding, 0, 12);
        Toggle("Action labels", &g_Config.m_PrismInputLabels);
        Toggle("Key animation", &g_Config.m_PrismInputAnimation);
        Color("Pressed key", &g_Config.m_PrismInputActiveColor, 9);
        Color("Released key", &g_Config.m_PrismInputInactiveColor, 10);
        Label("Movement, Jump, Hook and Fire use physical input.");
        break;
    case 5: // Themes
        Label("Themes   /   Prism identity");
        if(Button("Theme: Orange graphite", 18)) PrismUi::ApplyTheme(g_Config, 0);
        if(Button("Theme: Glacier", 19)) PrismUi::ApplyTheme(g_Config, 1);
        if(Button("Theme: Violet", 20)) PrismUi::ApplyTheme(g_Config, 2);
        Color("Accent", &g_Config.m_PrismThemeAccent, 11);
        Color("Background", &g_Config.m_PrismThemeBackground, 12);
        Color("Panel", &g_Config.m_PrismThemePanel, 13);
        Color("Text", &g_Config.m_PrismThemeText, 14);
        Slider("Heading width (%)", &g_Config.m_PrismThemeHeadingWidth, 100, 140);
        Slider("Corner radius", &g_Config.m_PrismThemeRounding, 0, 20);
        Slider("Border opacity", &g_Config.m_PrismThemeBorder, 0, 100);
        Slider("Shadow opacity", &g_Config.m_PrismThemeShadow, 0, 100);
        if(Button("Export theme to prism/theme.json", 21)) PrismTheme::Save(Storage(), g_Config);
        if(Button("Import theme from prism/theme.json", 22)) PrismTheme::Load(Storage(), g_Config);
        Label("Visual presets do not change macros or HUD layouts.");
        break;
    }
	Scroll.End();
	Footer.Draw(Theme.m_Text.WithAlpha(0.04f), IGraphics::CORNER_ALL, 5.0f);
	Ui()->DoLabel(&Footer, "PRISM " PRISM_VERSION "  /  Insert or Esc", 9.0f, TEXTALIGN_MC);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	Prism::Validate(g_Config);
}
