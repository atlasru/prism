#!/usr/bin/env python3
"""Upgrade the existing Insert overlay without replacing its render architecture."""
from pathlib import Path


def edit(path, old, new, marker):
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    if marker in text:
        return
    if text.count(old) != 1:
        raise RuntimeError(f"{path}: expected exactly one anchor, found {text.count(old)}: {old[:110]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"patched {path}: {marker}")


MENU = "src/game/client/components/menus_settings_prism.cpp"
edit(MENU, '#include <game/client/prism.h>\n', '#include <game/client/prism.h>\n#include <game/client/prism_qol.h>\n', '#include <game/client/prism_qol.h>')
edit(MENU, 'PRISM   /   VISUAL STUDIO', 'PRISM   /   CONTROL CENTER', 'PRISM   /   CONTROL CENTER')
edit(MENU, 'static const char *s_apTabs[] = {"General", "Presets", "Tee", "Hook", "Interface", "Performance"};\n\tstatic CButtonContainer s_aTabs[6];\n\tstatic float s_aTabBlend[6] = {};\n\tfor(int i = 0; i < 6; ++i)', 'static const char *s_apTabs[] = {"Dashboard", "Visuals", "Double Tee", "Macros", "HUD", "Performance", "Settings"};\n\tstatic CButtonContainer s_aTabs[7];\n\tstatic float s_aTabBlend[7] = {};\n\tfor(int i = 0; i < 7; ++i)', 'static CButtonContainer s_aTabs[7]')
edit(MENU, 'static CScrollRegion s_aScroll[6];', 'static CScrollRegion s_aScroll[7];', 'static CScrollRegion s_aScroll[7];')
edit("src/game/client/components/menus.cpp", '(m_PrismCategory + 1) % 6;', '(m_PrismCategory + 1) % 7;', '(m_PrismCategory + 1) % 7;')
edit("src/game/client/components/menus.h", '\tint m_PrismCategory = 0;\n', '\tint m_PrismCategory = 0;\n\tint m_PrismCaptureBind = -1; // -1 none; 0 assistant, 1 emergency stop, 2..5 macros.\n', 'int m_PrismCaptureBind = -1;')
edit("src/game/client/components/menus.cpp", 'bool CMenus::OnInput(const IInput::CEvent &Event)\n{', '''bool CMenus::OnInput(const IInput::CEvent &Event)
{
\tif(m_PrismOpen && m_PrismCaptureBind >= 0 && (Event.m_Flags & IInput::FLAG_PRESS) && !(Event.m_Flags & IInput::FLAG_REPEAT))
\t{
\t\tint *apBind[] = {&g_Config.m_PrismDoubleBind, &g_Config.m_PrismStopBind,
\t\t\t&g_Config.m_PrismMacro1Bind, &g_Config.m_PrismMacro2Bind,
\t\t\t&g_Config.m_PrismMacro3Bind, &g_Config.m_PrismMacro4Bind};
\t\tif(m_PrismCaptureBind < 6)
\t\t{
\t\t\tif(Event.m_Key == KEY_ESCAPE || Event.m_Key == KEY_BACKSPACE)
\t\t\t\t*apBind[m_PrismCaptureBind] = 0;
\t\t\telse if(Event.m_Key != KEY_INSERT && Event.m_Key != KEY_F12)
\t\t\t\t*apBind[m_PrismCaptureBind] = Event.m_Key;
\t\t}
\t\tm_PrismCaptureBind = -1;
\t\treturn true;
\t}
''', 'if(m_PrismOpen && m_PrismCaptureBind >= 0')
edit("src/game/client/components/menus.cpp", 'm_PrismOpen = !m_PrismOpen;\n', 'm_PrismOpen = !m_PrismOpen;\n\t\tm_PrismCaptureBind = -1;\n', 'm_PrismCaptureBind = -1;\n\t\tif(!m_PrismOpen)')

p = Path(MENU)
s = p.read_text(encoding="utf-8")
if '// Phase 3 navigation and editors' not in s:
    start = s.index('\tswitch(m_PrismCategory)\n\t{')
    end = s.index('\tScroll.End();', start)
    replacement = r'''
    // Phase 3 navigation and editors. All controls edit saved DDNet configuration.
    // Existing visual presets affect visuals only; all QoL preferences are separate.
    static CButtonContainer s_aActionButtons[32];
    auto Button = [&](const char *pText, int Id, const ColorRGBA &Tint = ColorRGBA(0.37f, 0.47f, 0.58f, 0.25f)) {
        CUIRect Row = NextRow(35.0f);
        return !Scroll.RectClipped(Row) && DoButton_Menu(&s_aActionButtons[Id], pText, 0, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.45f, Tint);
    };
    auto Bind = [&](const char *pLabel, int *pValue, int Target, int ButtonId) {
        char aText[128];
        str_format(aText, sizeof(aText), "%s: %s", pLabel,
            m_PrismCaptureBind == Target ? "PRESS A KEY (Esc unbinds)" : (*pValue ? Input()->KeyName(*pValue) : "Unbound"));
        if(Button(aText, ButtonId)) m_PrismCaptureBind = Target;
    };
    switch(m_PrismCategory)
    {
    case 0: // Dashboard
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
        break;
    }
    case 1: // Visuals, including presets and all Tee/Hook adjustments
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
        break;
    }
    case 2: // Double Tee
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
    case 3: // Strictly bounded visual macro editor
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
    case 4: // HUD drag/drop + layout and per-module visibility
    {
        Label("Modular HUD   /   drag handles to reposition");
        Toggle("Enable all modules", &g_Config.m_PrismHudEnabled);
        Slider("HUD scale (%)", &g_Config.m_PrismHudScale, 65, 150);
        Slider("HUD opacity (%)", &g_Config.m_PrismHudOpacity, 20, 100);
        static const char *s_apModuleNames[] = {"Active Hotkeys", "Prism name/version", "Performance", "Dummy Status", "Verified Staff"};
        int *apEnabled[] = {&g_Config.m_PrismHudHotkeys, &g_Config.m_PrismHudIdentity,
            &g_Config.m_PrismHudPerformance, &g_Config.m_PrismHudDummy, &g_Config.m_PrismHudStaff};
        int *apX[] = {&g_Config.m_PrismHudHotkeysX, &g_Config.m_PrismHudIdentityX,
            &g_Config.m_PrismHudPerformanceX, &g_Config.m_PrismHudDummyX, &g_Config.m_PrismHudStaffX};
        int *apY[] = {&g_Config.m_PrismHudHotkeysY, &g_Config.m_PrismHudIdentityY,
            &g_Config.m_PrismHudPerformanceY, &g_Config.m_PrismHudDummyY, &g_Config.m_PrismHudStaffY};
        for(int i = 0; i < 5; ++i) Toggle(s_apModuleNames[i], apEnabled[i]);
        Label("Layout preview  /  drag enabled modules");
        CUIRect Preview = NextRow(165.0f);
        if(!Scroll.RectClipped(Preview))
        {
            Preview.Draw(ColorRGBA(0.018f, 0.031f, 0.047f, 0.75f), IGraphics::CORNER_ALL, 9.0f);
            static CButtonContainer s_aPins[5];
            static int s_DragIndex = -1;
            static float s_DragOffsetX = 0.0f, s_DragOffsetY = 0.0f;
            if(!Ui()->MouseButton(0))
            { s_DragIndex = -1; }
            for(int i = 0; i < 5; ++i)
            {
                if(!*apEnabled[i]) continue;
                CUIRect Pin;
                Pin.w = 82.0f; Pin.h = 18.0f;
                Pin.x = Preview.x + PrismQol::HudCoordinate(*apX[i], Preview.w, Pin.w);
                Pin.y = Preview.y + PrismQol::HudCoordinate(*apY[i], Preview.h, Pin.h);
                if(s_DragIndex == -1 && Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&Pin))
                {
                    s_DragIndex = i;
                    s_DragOffsetX = Ui()->MouseX() - Pin.x;
                    s_DragOffsetY = Ui()->MouseY() - Pin.y;
                    Ui()->SetActiveItem(&s_aPins[i]);
                }
                if(s_DragIndex == i && Ui()->MouseButton(0))
                {
                    Ui()->CheckActiveItem(&s_aPins[i]);
                    *apX[i] = PrismQol::HudNormalize(Ui()->MouseX() - s_DragOffsetX - Preview.x, Preview.w);
                    *apY[i] = PrismQol::HudNormalize(Ui()->MouseY() - s_DragOffsetY - Preview.y, Preview.h);
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
            const int aX[] = {300, 300, 8200, 300, 8200};
            const int aY[] = {900, 250, 250, 1600, 950};
            for(int i = 0; i < 5; ++i) { *apX[i] = aX[i]; *apY[i] = aY[i]; }
            g_Config.m_PrismHudScale = 100;
            g_Config.m_PrismHudOpacity = 85;
        }
        Label("Staff appears only with server-supplied auth data.");
        break;
    }
    case 5: // Performance
        Label("Performance   /   measured locally");
        Toggle("FPS and frame-time overlay", &g_Config.m_PrismOverlay);
        Toggle("Performance HUD panel", &g_Config.m_PrismHudPerformance);
        Toggle("Enable Prism visual effects", &g_Config.m_PrismEnabled);
        Toggle("Local Tee glow", &g_Config.m_PrismLocalGlow);
        Toggle("Other Tee glow", &g_Config.m_PrismOtherGlow);
        Toggle("Local Hook glow", &g_Config.m_PrismLocalHookGlow);
        Toggle("Other Hook glow", &g_Config.m_PrismOtherHookGlow);
        Label("No GPU framebuffer blur or per-frame heap work.");
        break;
    case 6: // Settings
        Label("Interface   /   restrained glass");
        Slider("Menu scale", &g_Config.m_PrismMenuScale, 80, 120);
        Slider("Glass opacity", &g_Config.m_PrismPanelOpacity, 50, 100);
        Toggle("Reduce animation", &g_Config.m_PrismReducedMotion);
        Label("Visual presets do not change macros or HUD layouts.");
        if(Button("Reset visual presets only", 15)) Prism::Reset(g_Config);
        break;
    }
'''
    s = s[:start] + replacement + s[end:]
    p.write_text(s, encoding="utf-8")
    print("upgraded Prism overlay to Phase 3")
print("Phase 3 menu integration complete")
