"""Apply the isolated Prism Phase 2 integration changes to the pinned DDNet worktree.

The GitHub Actions Windows runner has the source checkout; every replacement asserts its
original context, so unexpected upstream changes fail without silently overwriting work.
This helper is only for the Phase 2 integration commit, not part of the runtime.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace(path: str, old: str, new: str) -> None:
    file = ROOT / path
    contents = file.read_text(encoding="utf-8")
    occurrences = contents.count(old)
    if occurrences != 1:
        raise RuntimeError(f"{path}: expected exactly one patch anchor, found {occurrences}: {old[:90]!r}")
    file.write_bytes(contents.replace(old, new).encode("utf-8"))
    print(f"patched {path}")


# Keep all pre-existing DDNet menu state. Prism owns an independent modal surface.
replace("src/game/client/components/menus.h",
        "\tbool m_MenuActive;\n",
        "\tbool m_MenuActive;\n\tbool m_PrismOpen = false;\n\tfloat m_PrismTransition = 0.0f;\n\tint m_PrismCategory = 0;\n")
replace("src/game/client/components/menus.h",
        "\t\tSETTINGS_CREDITS,\n\t\tSETTINGS_PRISM,\n\n\t\tSETTINGS_LENGTH,",
        "\t\tSETTINGS_CREDITS,\n\n\t\tSETTINGS_LENGTH,")

# Remove legacy settings navigation; values still use the same saved config.
replace("src/game/client/components/menus_settings.cpp",
        "void CMenus::RenderSettings(CUIRect MainView)\n{\n",
        "void CMenus::RenderSettings(CUIRect MainView)\n{\n\tif(g_Config.m_UiSettingsPage < 0 || g_Config.m_UiSettingsPage >= SETTINGS_LENGTH)\n\t\tg_Config.m_UiSettingsPage = SETTINGS_GENERAL;\n")
replace("src/game/client/components/menus_settings.cpp",
        "\t\tLocalize(\"Credits\"),\n\t\t\"Prism\"};",
        "\t\tLocalize(\"Credits\")};")
replace("src/game/client/components/menus_settings.cpp",
        "\telse if(g_Config.m_UiSettingsPage == SETTINGS_PRISM)\n\t{\n\t\tRenderSettingsPrism(MainView);\n\t}\n",
        "")

replace("src/game/client/components/menus.cpp",
        "\tif(!m_MenuActive)\n\t\treturn false;\n\n\tUi()->ConvertMouseMove(&x, &y, CursorType);",
        "\tif(!m_MenuActive && !m_PrismOpen)\n\t\treturn false;\n\n\tUi()->ConvertMouseMove(&x, &y, CursorType);")
replace("src/game/client/components/menus.cpp",
        "bool CMenus::OnInput(const IInput::CEvent &Event)\n{\n\t// Escape key is always handled to activate/deactivate menu\n\tif((Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE) || IsActive())\n\t{\n\t\tUi()->OnInput(Event);\n\t\treturn true;\n\t}\n\treturn false;\n}\n",
        "bool CMenus::OnInput(const IInput::CEvent &Event)\n{\n\tconst bool Playing = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;\n\tif(Playing && m_Popup == POPUP_NONE && (m_PrismOpen || !IsActive()) &&\n\t\t(Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_INSERT)\n\t{\n\t\tm_PrismOpen = !m_PrismOpen;\n\t\tUi()->SetActiveItem(nullptr);\n\t\tUi()->SetHotItem(nullptr);\n\t\tUi()->ClearHotkeys();\n\t\tif(m_PrismOpen)\n\t\t\tGameClient()->OnRelease();\n\t\treturn true;\n\t}\n\tif(m_PrismOpen)\n\t{\n\t\tif((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE)\n\t\t{\n\t\t\tm_PrismOpen = false;\n\t\t\tUi()->SetActiveItem(nullptr);\n\t\t\tUi()->ClearHotkeys();\n\t\t\treturn true;\n\t\t}\n\t\tUi()->OnInput(Event);\n\t\treturn true;\n\t}\n\t// Escape key is always handled to activate/deactivate the original menu.\n\tif(((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE) || IsActive())\n\t{\n\t\tUi()->OnInput(Event);\n\t\treturn true;\n\t}\n\treturn false;\n}\n")
replace("src/game/client/components/menus.cpp",
        "void CMenus::OnStateChange(int NewState, int OldState)\n{\n\t// reset active item\n",
        "void CMenus::OnStateChange(int NewState, int OldState)\n{\n\tm_PrismOpen = false;\n\tm_PrismTransition = 0.0f;\n\t// reset active item\n")
replace("src/game/client/components/menus.cpp",
        "void CMenus::SetActive(bool Active)\n{\n\tif(Active != m_MenuActive)",
        "void CMenus::SetActive(bool Active)\n{\n\tif(Active && m_PrismOpen)\n\t{\n\t\tm_PrismOpen = false;\n\t\tm_PrismTransition = 0.0f;\n\t}\n\tif(Active != m_MenuActive)")
replace("src/game/client/components/menus.cpp",
        "\tif(!IsActive())\n\t{\n\t\tif(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))",
        "\t// Prism is rendered after the world but before normal DDNet menus. Its input\n\t// is captured by OnInput/OnCursorMove, never forwarded to movement controls.\n\tif((m_PrismOpen || m_PrismTransition > 0.0f) &&\n\t\t(Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))\n\t{\n\t\tUi()->MapScreen();\n\t\tUi()->StartCheck();\n\t\tUpdateColors();\n\t\tUi()->Update();\n\t\tif(m_PrismOpen && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))\n\t\t{\n\t\t\tm_PrismCategory = (m_PrismCategory + 1) % 6;\n\t\t\tUi()->SetActiveItem(nullptr);\n\t\t}\n\t\tRenderSettingsPrism(*Ui()->Screen());\n\t\tUi()->RenderPopupMenus();\n\t\tif(m_PrismOpen)\n\t\t\tRenderTools()->RenderCursor(Ui()->MousePos(), 24.0f);\n\t\tUi()->FinishCheck();\n\t\tUi()->ClearHotkeys();\n\t\treturn;\n\t}\n\n\tif(!IsActive())\n\t{\n\t\tif(Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE))")

# Feathered, bounded alpha silhouettes: cached masks and the original interpolated
# animations are reused; nothing enters physics, prediction or the network path.
replace("src/game/client/render.cpp",
        " // Three bounded alpha layers approximate a halo with no framebuffer or shader dependency.\n if(Glow && Intensity > 0.0f)\n  for(int i = 3; i >= 1; --i) Layer(GlowColor.WithAlpha(Intensity * 0.13f), (2.0f + i * 3.0f) * AnimScale);\n if(Outline) Layer(OutlineColor, Width * AnimScale);",
        " // Four low-opacity feather layers approximate a halo without framebuffer blur.\n // The outermost layer is faint; the inner falloff is deliberately restrained.\n if(Glow && Intensity > 0.0f)\n {\n  constexpr float aRadius[] = {13.0f, 9.5f, 6.0f, 3.0f};\n  constexpr float aAlpha[] = {0.028f, 0.048f, 0.072f, 0.11f};\n  for(int i = 0; i < 4; ++i)\n   Layer(GlowColor.WithAlpha(Intensity * aAlpha[i]), aRadius[i] * AnimScale);\n }\n if(Outline)\n {\n  Layer(OutlineColor.WithAlpha(0.12f), (Width + 2.5f) * AnimScale);\n  Layer(OutlineColor.WithAlpha(0.75f), Width * AnimScale);\n }")
replace("src/game/client/components/players.cpp",
        "   Graphics()->SetColor(Accent.WithAlpha(Alpha * Intensity * 0.13f));\n   for(int i = 3; i >= 1; --i)\n   {\n    const vec2 Offset = Normal * (2.0f + i * 2.0f);\n    IGraphics::CFreeformItem Item(HookPos - Offset, HookPos + Offset, Pos - Offset, Pos + Offset);\n    Graphics()->QuadsDrawFreeform(&Item, 1);\n   }",
        "   // Broad, faint outside; narrow, soft inside. Identical hook endpoints.\n   constexpr float aRadius[] = {11.0f, 8.0f, 5.0f, 2.5f};\n   constexpr float aAlpha[] = {0.025f, 0.045f, 0.075f, 0.12f};\n   for(int i = 0; i < 4; ++i)\n   {\n    const vec2 Offset = Normal * aRadius[i];\n    Graphics()->SetColor(Accent.WithAlpha(Alpha * Intensity * aAlpha[i]));\n    IGraphics::CFreeformItem Item(HookPos - Offset, HookPos + Offset, Pos - Offset, Pos + Offset);\n    Graphics()->QuadsDrawFreeform(&Item, 1);\n   }")

print("Prism Phase 2 targeted integration finished")
