#!/usr/bin/env python3
"""One-shot Phase 3 corrective patch: never write macro-owned state into manual controls."""
from pathlib import Path


def replace(path, before, after, marker):
    file = Path(path)
    text = file.read_text(encoding="utf-8")
    if marker in text:
        return
    count = text.count(before)
    if count != 1:
        raise RuntimeError(f"{path}: expected exactly one match; found {count}: {before[:100]!r}")
    file.write_text(text.replace(before, after, 1), encoding="utf-8")
    print(f"fixed {path}: {marker}")


replace("src/game/client/components/menus_settings_prism.cpp", '#include "menus.h"\n', '#include "menus.h"\n#include <game/client/gameclient.h>\n', '#include <game/client/gameclient.h>')
replace("src/game/client/components/controls.h", '\tCNetObj_PlayerInput m_aLastData[NUM_DUMMIES];', '\tCNetObj_PlayerInput m_aLastData[NUM_DUMMIES];\n\tCNetObj_PlayerInput m_aPrismLastOutput[NUM_DUMMIES] = {};', 'm_aPrismLastOutput[NUM_DUMMIES]')
replace("src/game/client/gameclient.h", '\tbool m_PrismMacroFireOwned = false;', '\tbool m_PrismMacroFireOwned = false;\n\tint m_PrismMacroFireCounter = 0;\n\tint m_PrismMacroLastManualFire = 0;', 'int m_PrismMacroFireCounter = 0;')
replace("src/game/client/gameclient.cpp", '''\t\tm_DummyInput = {};
\t\tm_HammerInput = {};''', '''\t\tm_DummyInput = {};
\t\tm_HammerInput = {};''', 'noop') if False else None
replace("src/game/client/gameclient.cpp", '\tm_DummyInput = {};\n\tm_HammerInput = {};\n\tm_DummyFire = 0;', '''\tm_DummyInput = {};
\tm_HammerInput = {};
\tm_DummyFire = 0;
\tm_PrismMacros.Cancel();
\tm_PrismMacroFireOwned = false;
\tm_PrismMacroFireCounter = 0;
\tm_PrismMacroLastManualFire = 0;
\tm_PrismLastAssisted = false;
\tm_PrismDummyFireOwned = false;
\tm_PrismDummyFire = 0;
\tm_PrismHammerCounter = 0;''', 'm_PrismMacroLastManualFire = 0;')
replace("src/game/client/gameclient.cpp", '\t\t!m_Menus.IsActive() && !m_Chat.IsActive() && !DemoPlayer()->IsPlaying();', '\t\t!m_Menus.IsActive() && !m_Chat.IsActive() && !m_GameConsole.IsActive() && !DemoPlayer()->IsPlaying();', '!m_GameConsole.IsActive() && !DemoPlayer()->IsPlaying();')
replace("src/game/client/gameclient.cpp", 'if(Pressed && !m_Menus.IsActive() && !m_Chat.IsActive() && Client()->State() == IClient::STATE_ONLINE &&', 'if(Pressed && !m_Menus.IsActive() && !m_Chat.IsActive() && !m_GameConsole.IsActive() && Client()->State() == IClient::STATE_ONLINE &&', 'if(Pressed && !m_Menus.IsActive() && !m_Chat.IsActive() && !m_GameConsole.IsActive()')
replace("src/game/client/gameclient.cpp", '''\tif(m_aLocalIds[!g_Config.m_ClDummy] < 0)
\t{
\t\treturn 0;
\t}

\tconst bool PrismAllowed''', '''\tif(m_aLocalIds[!g_Config.m_ClDummy] < 0)
\t{
\t\treturn 0;
\t}

\tconst bool PrismAllowed''', 'noop2') if False else None
replace("src/game/client/gameclient.cpp", 'const bool Assisted = PrismAllowed && g_Config.m_PrismDoubleEnabled;', 'const bool Assisted = PrismAllowed && g_Config.m_PrismDoubleEnabled && m_aLocalIds[0] >= 0 && m_aLocalIds[1] >= 0;', 'm_aLocalIds[1] >= 0;')
replace("src/game/client/components/controls.cpp", '''\tm_aLastData[Dummy].m_Jump = 0;
\tm_aInputData[Dummy] = m_aLastData[Dummy];''', '''\tm_aLastData[Dummy].m_Jump = 0;
\tm_aInputData[Dummy] = m_aLastData[Dummy];
\tm_aPrismLastOutput[Dummy] = m_aInputData[Dummy];''', 'm_aPrismLastOutput[Dummy] = m_aInputData[Dummy];')
replace("src/game/client/components/controls.cpp", '''\tbool Send = m_aLastData[g_Config.m_ClDummy].m_PlayerFlags != m_aInputData[g_Config.m_ClDummy].m_PlayerFlags;''', '''\tbool Send = m_aLastData[g_Config.m_ClDummy].m_PlayerFlags != m_aInputData[g_Config.m_ClDummy].m_PlayerFlags;
\tCNetObj_PlayerInput PrismComposedInput = m_aInputData[g_Config.m_ClDummy];''', 'CNetObj_PlayerInput PrismComposedInput =')
old = '''\t\t// Prism macros own only their added inputs; manual directions and buttons win.
\t\tconst int Owned = GameClient()->m_PrismMacros.Owned();
\t\tif(m_aInputData[g_Config.m_ClDummy].m_Direction == 0)
\t\t{
\t\t\tif((Owned & PrismQol::OWN_LEFT) && !(Owned & PrismQol::OWN_RIGHT)) m_aInputData[g_Config.m_ClDummy].m_Direction = -1;
\t\t\tif((Owned & PrismQol::OWN_RIGHT) && !(Owned & PrismQol::OWN_LEFT)) m_aInputData[g_Config.m_ClDummy].m_Direction = 1;
\t\t}
\t\tif(Owned & PrismQol::OWN_JUMP) m_aInputData[g_Config.m_ClDummy].m_Jump = 1;
\t\tif(Owned & PrismQol::OWN_HOOK) m_aInputData[g_Config.m_ClDummy].m_Hook = 1;
\t\tif(GameClient()->m_PrismMacros.TakeFirePulse())
\t\t{
\t\t\tif(!(m_aInputData[g_Config.m_ClDummy].m_Fire & 1))
\t\t\t{
\t\t\t\tm_aInputData[g_Config.m_ClDummy].m_Fire = (m_aInputData[g_Config.m_ClDummy].m_Fire + 1) & INPUT_STATE_MASK;
\t\t\t\tGameClient()->m_PrismMacroFireOwned = true;
\t\t\t}
\t\t}
\t\telse if(GameClient()->m_PrismMacroFireOwned)
\t\t{
\t\t\tif(m_aInputData[g_Config.m_ClDummy].m_Fire & 1)
\t\t\t\tm_aInputData[g_Config.m_ClDummy].m_Fire = (m_aInputData[g_Config.m_ClDummy].m_Fire + 1) & INPUT_STATE_MASK;
\t\t\tGameClient()->m_PrismMacroFireOwned = false;
\t\t}
'''
new = '''\t\t// Compose a separate output snapshot; NEVER mutate m_aInputData or m_aLastData.
\t\t// Those buffers hold physical inputs and feed DDNet's original dummy copying.
\t\tPrismComposedInput = m_aInputData[g_Config.m_ClDummy];
\t\tconst int Owned = GameClient()->m_PrismMacros.Owned();
\t\tif(PrismComposedInput.m_Direction == 0)
\t\t{
\t\t\tif((Owned & PrismQol::OWN_LEFT) && !(Owned & PrismQol::OWN_RIGHT)) PrismComposedInput.m_Direction = -1;
\t\t\tif((Owned & PrismQol::OWN_RIGHT) && !(Owned & PrismQol::OWN_LEFT)) PrismComposedInput.m_Direction = 1;
\t\t}
\t\tif(Owned & PrismQol::OWN_JUMP) PrismComposedInput.m_Jump = 1;
\t\tif(Owned & PrismQol::OWN_HOOK) PrismComposedInput.m_Hook = 1;
\t\t// Fire counters must remain monotonic even after a synthetic press/release.
\t\t// Apply physical counter deltas, then one optional macro pulse.
\t\tconst int ManualFire = m_aInputData[g_Config.m_ClDummy].m_Fire & INPUT_STATE_MASK;
\t\tconst int ManualDelta = (ManualFire - GameClient()->m_PrismMacroLastManualFire) & INPUT_STATE_MASK;
\t\tGameClient()->m_PrismMacroFireLastPhysical = ManualFire;
\t\tGameClient()->m_PrismMacroFireCounter = (GameClient()->m_PrismMacroFireCounter + ManualDelta) & INPUT_STATE_MASK;
\t\tGameClient()->m_PrismMacroLastManualFire = ManualFire;
\t\tconst bool Pulse = GameClient()->m_PrismMacros.TakeFirePulse();
\t\tif(ManualFire & 1)
\t\t{
\t\t\tif(!(GameClient()->m_PrismMacroFireCounter & 1))
\t\t\t\tGameClient()->m_PrismMacroFireCounter = (GameClient()->m_PrismMacroFireCounter + 1) & INPUT_STATE_MASK;
\t\t\tGameClient()->m_PrismMacroFireOwned = false;
\t\t}
\t\telse if(Pulse && !GameClient()->m_PrismMacroFireOwned)
\t\t{
\t\t\tif(!(GameClient()->m_PrismMacroFireCounter & 1))
\t\t\t\tGameClient()->m_PrismMacroFireCounter = (GameClient()->m_PrismMacroFireCounter + 1) & INPUT_STATE_MASK;
\t\t\tGameClient()->m_PrismMacroFireOwned = true;
\t\t}
\t\telse
\t\t{
\t\t\tif(GameClient()->m_PrismMacroFireCounter & 1)
\t\t\t\tGameClient()->m_PrismMacroFireCounter = (GameClient()->m_PrismMacroFireCounter + 1) & INPUT_STATE_MASK;
\t\t\tGameClient()->m_PrismMacroFireOwned = false;
\t\t}
\t\tPrismComposedInput.m_Fire = GameClient()->m_PrismMacroFireCounter;
'''
new = new.replace('\t\tGameClient()->m_PrismMacroFireLastPhysical = ManualFire;\n', '')
replace("src/game/client/components/controls.cpp", old, new, '// Compose a separate output snapshot')
replace("src/game/client/components/controls.cpp", '''\t// copy and return size
\tm_aLastData[g_Config.m_ClDummy] = m_aInputData[g_Config.m_ClDummy];''', '''\t// Release exactly Prism-owned input when entering UI, losing focus or stopping.
\tif(!(m_aInputData[g_Config.m_ClDummy].m_PlayerFlags & PLAYERFLAG_PLAYING))
\t{
\t\tPrismComposedInput = m_aInputData[g_Config.m_ClDummy];
\t\tif(GameClient()->m_PrismMacroFireOwned && (GameClient()->m_PrismMacroFireCounter & 1))
\t\t\tGameClient()->m_PrismMacroFireCounter = (GameClient()->m_PrismMacroFireCounter + 1) & INPUT_STATE_MASK;
\t\tGameClient()->m_PrismMacroFireOwned = false;
\t\tGameClient()->m_PrismMacroLastManualFire = PrismComposedInput.m_Fire & INPUT_STATE_MASK;
\t\tPrismComposedInput.m_Fire = GameClient()->m_PrismMacroFireCounter;
\t}
\tconst int Dummy = g_Config.m_ClDummy;
\tSend = Send || PrismComposedInput.m_Direction != m_aPrismLastOutput[Dummy].m_Direction;
\tSend = Send || PrismComposedInput.m_Jump != m_aPrismLastOutput[Dummy].m_Jump;
\tSend = Send || PrismComposedInput.m_Hook != m_aPrismLastOutput[Dummy].m_Hook;
\tSend = Send || PrismComposedInput.m_Fire != m_aPrismLastOutput[Dummy].m_Fire;
\tm_aPrismLastOutput[Dummy] = PrismComposedInput;
\t// Preserve the DDNet physical input state and its original dummy-copy deltas.
\tm_aLastData[Dummy] = m_aInputData[Dummy];''', '// Preserve the DDNet physical input state')
replace("src/game/client/components/controls.cpp", '\tmem_copy(pData, &m_aInputData[g_Config.m_ClDummy], sizeof(m_aInputData[0]));\n\treturn sizeof(m_aInputData[0]);\n}', '\tmem_copy(pData, &PrismComposedInput, sizeof(PrismComposedInput));\n\treturn sizeof(PrismComposedInput);\n}', 'mem_copy(pData, &PrismComposedInput, sizeof(PrismComposedInput));')
print("Corrective patch complete: isolated macro input ownership and fixed incomplete type")
