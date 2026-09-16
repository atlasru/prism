"""One-time narrowly scoped UI refinements after Phase 2 integration."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace(path, before, after):
    file = ROOT / path
    source = file.read_text(encoding="utf-8")
    count = source.count(before)
    if count != 1:
        raise RuntimeError(f"{path}: expected one occurrence, got {count}: {before[:90]!r}")
    file.write_bytes(source.replace(before, after).encode("utf-8"))
    print("refined", path)

ui = "src/game/client/components/menus_settings_prism.cpp"
replace(ui, '"Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f,',
            '"Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.52f,')
replace(ui, 's_apTabs[i], 0, &Tab, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f,',
            's_apTabs[i], 0, &Tab, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.42f,')
replace(ui, '"Restore Prism defaults", 0, &Reset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.0f,',
            '"Restore Prism defaults", 0, &Reset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.40f,')
replace(ui, 's_apPresets[i], 0, &Preset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 10.0f, 0.0f,',
            's_apPresets[i], 0, &Preset, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 10.0f, 0.38f,')
replace(ui, 'if(DoButton_Menu(&s_Close, "Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.52f, ColorRGBA(0.35f, 0.43f, 0.51f, 0.22f)))\n\t\tm_PrismOpen = false;',
            'if(DoButton_Menu(&s_Close, "Close", 0, &Close, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.52f, ColorRGBA(0.35f, 0.43f, 0.51f, 0.22f)))\n\t{\n\t\tUi()->ClosePopupMenus();\n\t\tm_PrismOpen = false;\n\t}')

menu = "src/game/client/components/menus.cpp"
replace(menu,
        '\t\tm_PrismOpen = !m_PrismOpen;\n\t\tUi()->SetActiveItem(nullptr);',
        '\t\tm_PrismOpen = !m_PrismOpen;\n\t\tif(!m_PrismOpen)\n\t\t\tUi()->ClosePopupMenus();\n\t\tUi()->SetActiveItem(nullptr);')
replace(menu,
        '\t\tif((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE)\n\t\t{\n\t\t\tm_PrismOpen = false;',
        '\t\tif((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE)\n\t\t{\n\t\t\tif(Ui()->IsPopupOpen())\n\t\t\t{\n\t\t\t\tUi()->ClosePopupMenus();\n\t\t\t\tUi()->ClearHotkeys();\n\t\t\t\treturn true;\n\t\t\t}\n\t\t\tm_PrismOpen = false;')
replace(menu,
        'if(m_PrismOpen && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))',
        'if(m_PrismOpen && !Ui()->IsPopupOpen() && Ui()->ConsumeHotkey(CUi::HOTKEY_TAB))')
print("Prism Phase 2 modal/typography refinements complete")
