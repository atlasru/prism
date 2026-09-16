#!/usr/bin/env python3
"""Apply the Phase 3 integration to the existing Phase 2 source, exactly once.

Intended for the temporary GitHub Actions integration job, not a runtime script.
Every replacement has a unique anchor and fails closed if upstream changed.
"""
from pathlib import Path


def edit(path, old, new, marker):
    p = Path(path)
    content = p.read_text(encoding="utf-8")
    if marker in content:
        return
    count = content.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one integration anchor, got {count}: {old[:100]!r}")
    p.write_text(content.replace(old, new, 1), encoding="utf-8")
    print(f"patched {path}: {marker}")


edit("src/engine/shared/config_variables.h", '#include "prism_variables.h"\n', '#include "prism_variables.h"\n#include "prism_qol_variables.h"\n', '#include "prism_qol_variables.h"')
edit("src/game/client/gameclient.h", '#include "render.h"\n', '#include "render.h"\n#include "prism_qol.h"\n', '#include "prism_qol.h"')
edit("src/game/client/gameclient.h", '\tCControls m_Controls;\n', '''\tCControls m_Controls;
\t// Phase 3: only client-side input composition, never prediction/physics/protocol.
\tPrismQol::CMacroEngine m_PrismMacros;
\tbool m_PrismMacroFireOwned = false;
\tbool m_PrismDummyFireOwned = false;
\tbool m_PrismLastAssisted = false;
\tint m_PrismDummyFire = 0;
\tint m_PrismHammerCounter = 0;
\tvoid PrismEmergencyStop();
''', 'PrismQol::CMacroEngine m_PrismMacros;')
edit("src/game/client/gameclient.cpp", '#include <engine/graphics.h>\n', '#include <engine/graphics.h>\n#include <engine/keys.h>\n', '#include <engine/keys.h>')
edit("src/game/client/gameclient.cpp", ' Console()->Register("prism_reset", "", CFGFLAG_CLIENT, [](IConsole::IResult *, void *) { Prism::Reset(g_Config); }, this, "Reset all Prism settings");\n', ''' Console()->Register("prism_reset", "", CFGFLAG_CLIENT, [](IConsole::IResult *, void *) { Prism::Reset(g_Config); }, this, "Reset all Prism settings");
 Console()->Register("prism_emergency_stop", "", CFGFLAG_CLIENT, [](IConsole::IResult *, void *pUserData) { static_cast<CGameClient *>(pUserData)->PrismEmergencyStop(); }, this, "Stop Double Tee and all active macros, releasing owned controls");
''', '"prism_emergency_stop"')
edit("src/game/client/gameclient.cpp", 'void CGameClient::OnUpdate()\n{\n\tHandleLanguageChanged();', '''void CGameClient::OnUpdate()
{
\tHandleLanguageChanged();
\tconst bool CanRunPrism = Client()->State() == IClient::STATE_ONLINE &&
\t\tKernel()->RequestInterface<IEngineGraphics>()->WindowActive() &&
\t\t!m_Menus.IsActive() && !m_Chat.IsActive() && !DemoPlayer()->IsPlaying();
\tconst int64_t PrismTimeMs = time_get() * 1000 / time_freq();
\tm_PrismMacros.Configure(0, g_Config.m_PrismMacro1, g_Config.m_PrismMacro1Bind, g_Config.m_PrismMacro1Mode, g_Config.m_PrismMacro1Enabled != 0);
\tm_PrismMacros.Configure(1, g_Config.m_PrismMacro2, g_Config.m_PrismMacro2Bind, g_Config.m_PrismMacro2Mode, g_Config.m_PrismMacro2Enabled != 0);
\tm_PrismMacros.Configure(2, g_Config.m_PrismMacro3, g_Config.m_PrismMacro3Bind, g_Config.m_PrismMacro3Mode, g_Config.m_PrismMacro3Enabled != 0);
\tm_PrismMacros.Configure(3, g_Config.m_PrismMacro4, g_Config.m_PrismMacro4Bind, g_Config.m_PrismMacro4Mode, g_Config.m_PrismMacro4Enabled != 0);
\tm_PrismMacros.Tick(PrismTimeMs, CanRunPrism);
\tif(!CanRunPrism)
\t\tm_PrismHammerCounter = 0;''', 'const bool CanRunPrism = Client()->State()')
edit("src/game/client/gameclient.cpp", 'void CGameClient::OnInput(const IInput::CEvent &Event)\n{\n', '''void CGameClient::OnInput(const IInput::CEvent &Event)
{
\tconst bool Pressed = (Event.m_Flags & IInput::FLAG_PRESS) != 0;
\tconst bool Released = (Event.m_Flags & IInput::FLAG_RELEASE) != 0;
\tif(Released)
\t\tm_PrismMacros.KeyEvent(Event.m_Key, false, false, time_get() * 1000 / time_freq());
\tif(Pressed && !(Event.m_Flags & IInput::FLAG_REPEAT) &&
\t\t(Event.m_Key == KEY_F12 || (g_Config.m_PrismStopBind && Event.m_Key == g_Config.m_PrismStopBind)) &&
\t\tClient()->State() == IClient::STATE_ONLINE)
\t{
\t\tPrismEmergencyStop();
\t\treturn;
\t}
\tif(Pressed && !m_Menus.IsActive() && !m_Chat.IsActive() && Client()->State() == IClient::STATE_ONLINE &&
\t\tKernel()->RequestInterface<IEngineGraphics>()->WindowActive())
\t{
\t\tif(!(Event.m_Flags & IInput::FLAG_REPEAT) && g_Config.m_PrismDoubleBind && Event.m_Key == g_Config.m_PrismDoubleBind)
\t\t{
\t\t\tg_Config.m_PrismDoubleEnabled ^= 1;
\t\t\tif(!g_Config.m_PrismDoubleEnabled) m_PrismHammerCounter = 0;
\t\t\treturn;
\t\t}
\t\tm_PrismMacros.KeyEvent(Event.m_Key, true, (Event.m_Flags & IInput::FLAG_REPEAT) != 0, time_get() * 1000 / time_freq());
\t}
''', 'PrismEmergencyStop();\n\t\treturn;')
edit("src/game/client/gameclient.cpp", 'void CGameClient::OnDummySwap()\n{', '''void CGameClient::PrismEmergencyStop()
{
\tg_Config.m_PrismDoubleEnabled = 0;
\tm_PrismHammerCounter = 0;
\tm_PrismMacros.Cancel();
\t// The next input snapshot releases any previously owned fire counter.
}

void CGameClient::OnDummySwap()
{''', 'void CGameClient::PrismEmergencyStop()')

# Integrate the dummy assistant before the existing stock Hammer Fly branch.
# It composes one snapshot, preserving the manual base input and never touching
# predicted positions, actual game objects, server code or network protocol.
edit("src/game/client/gameclient.cpp", '\tif(!g_Config.m_ClDummyHammer)\n\t{\n\t\tif(m_DummyFire != 0)', '''\tconst bool PrismAllowed = Client()->State() == IClient::STATE_ONLINE && Client()->DummyConnected() &&
\t\tKernel()->RequestInterface<IEngineGraphics>()->WindowActive() && !m_Menus.IsActive() &&
\t\t!m_Chat.IsActive() && !DemoPlayer()->IsPlaying();
\tconst bool Assisted = PrismAllowed && g_Config.m_PrismDoubleEnabled;
\tif(Assisted || m_PrismLastAssisted || m_PrismDummyFireOwned)
\t{
\t\tCNetObj_PlayerInput Input = m_DummyInput;
\t\tconst int Controlled = g_Config.m_ClDummy;
\t\tconst int Mode = std::clamp(g_Config.m_PrismDoubleMode, 0, 2);
\t\tif(Assisted && Mode != 1 && (!m_Snap.m_SpecInfo.m_Active || m_Snap.m_SpecInfo.m_SpectatorId < 0))
\t\t{
\t\t\tconst auto &Player = m_Controls.m_aInputData[Controlled];
\t\t\tInput.m_Direction = Player.m_Direction;
\t\t\tInput.m_Jump = Player.m_Jump;
\t\t\tInput.m_Hook = Player.m_Hook;
\t\t\tInput.m_TargetX = Player.m_TargetX;
\t\t\tInput.m_TargetY = Player.m_TargetY;
\t\t\tInput.m_PlayerFlags = Player.m_PlayerFlags;
\t\t\tif(!g_Config.m_ClDummyControl)
\t\t\t\tInput.m_WantedWeapon = Player.m_WantedWeapon;
\t\t}
\t\tbool HammerPulse = false;
\t\tif(Assisted && Mode != 0)
\t\t{
\t\t\tconst int Interval = std::clamp(g_Config.m_PrismDoubleInterval, 5, 100);
\t\t\tHammerPulse = m_PrismHammerCounter == 0;
\t\t\tm_PrismHammerCounter = (m_PrismHammerCounter + 1) % Interval;
\t\t\tif(HammerPulse)
\t\t\t{
\t\t\t\tInput.m_WantedWeapon = WEAPON_HAMMER + 1;
\t\t\t\tconst vec2 Dir = m_LocalCharacterPos - m_aClients[m_aLocalIds[!Controlled]].m_Predicted.m_Pos;
\t\t\t\tInput.m_TargetX = (int)Dir.x;
\t\t\t\tInput.m_TargetY = (int)Dir.y;
\t\t\t}
\t\t}
\t\telse m_PrismHammerCounter = 0;
\t\tif(HammerPulse && !(Input.m_Fire & 1))
\t\t{
\t\t\tm_PrismDummyFire = (m_PrismDummyFire + 1) & INPUT_STATE_MASK;
\t\t\tif(!(m_PrismDummyFire & 1)) m_PrismDummyFire = (m_PrismDummyFire + 1) & INPUT_STATE_MASK;
\t\t\tInput.m_Fire = m_PrismDummyFire;
\t\t\tm_PrismDummyFireOwned = true;
\t\t}
\t\telse if(m_PrismDummyFireOwned)
\t\t{
\t\t\tif(!(Input.m_Fire & 1))
\t\t\t{
\t\t\t\tm_PrismDummyFire = (m_PrismDummyFire + 1) & INPUT_STATE_MASK;
\t\t\t\tif(m_PrismDummyFire & 1) m_PrismDummyFire = (m_PrismDummyFire + 1) & INPUT_STATE_MASK;
\t\t\t\tInput.m_Fire = m_PrismDummyFire;
\t\t\t}
\t\t\tm_PrismDummyFireOwned = false;
\t\t}
\t\telse m_PrismDummyFire = Input.m_Fire;
\t\tm_PrismLastAssisted = Assisted;
\t\tmem_copy(pData, &Input, sizeof(Input));
\t\treturn sizeof(Input);
\t}
\tif(!g_Config.m_ClDummyHammer)
\t{
\t\tif(m_DummyFire != 0)''', 'const bool PrismAllowed = Client()->State()')

edit("src/game/client/components/controls.cpp", '\t\t// check if we need to send input\n', '''\t\t// Prism macros own only their added inputs; manual directions and buttons win.
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
\t\t// check if we need to send input
''', '// Prism macros own only their added inputs;')

edit("src/game/client/gameclient.cpp", 'void CGameClient::OnDummyDisconnect()\n{', '''void CGameClient::OnDummyDisconnect()
{
\tm_PrismMacros.Cancel();
\tm_PrismLastAssisted = false;
\tm_PrismDummyFireOwned = false;
\tm_PrismHammerCounter = 0;''', 'm_PrismLastAssisted = false;\n\tm_PrismDummyFireOwned = false;')

# The HUD is an isolated component addition. No change to DDNet's cursor/input.
edit("src/game/client/components/hud.cpp", '#include "hud.h"\n', '#include "hud.h"\n#include <game/client/prism_qol.h>\n', '#include <game/client/prism_qol.h>')
edit("src/game/client/components/hud.cpp", '\tRenderCursor();\n}\n\nvoid CHud::OnMessage', '\tRenderPrismModules();\n\tRenderCursor();\n}\n\nvoid CHud::OnMessage', '\tRenderPrismModules();\n\tRenderCursor();')
edit("src/game/client/components/hud.h", 'class CHud : public CComponent\n{', 'class CHud : public CComponent\n{\n\tvoid RenderPrismModules();', 'void RenderPrismModules();')

HUD = r'''
// Dedicated Prism HUD: fixed stack buffers, normalized coordinates and no lists
// of "staff" inferred from names, clan tags, skins or chat messages.
void CHud::RenderPrismModules()
{
    if(!g_Config.m_PrismHudEnabled || Client()->State() != IClient::STATE_ONLINE)
        return;
    const int Scale = std::clamp(g_Config.m_PrismHudScale, 65, 150);
    const float Opacity = std::clamp(g_Config.m_PrismHudOpacity, 20, 100) / 100.0f;
    struct SModule
    {
        int *m_pEnabled;
        int *m_pX;
        int *m_pY;
    };
    SModule aModules[] = {
        {&g_Config.m_PrismHudHotkeys, &g_Config.m_PrismHudHotkeysX, &g_Config.m_PrismHudHotkeysY},
        {&g_Config.m_PrismHudIdentity, &g_Config.m_PrismHudIdentityX, &g_Config.m_PrismHudIdentityY},
        {&g_Config.m_PrismHudPerformance, &g_Config.m_PrismHudPerformanceX, &g_Config.m_PrismHudPerformanceY},
        {&g_Config.m_PrismHudDummy, &g_Config.m_PrismHudDummyX, &g_Config.m_PrismHudDummyY},
        {&g_Config.m_PrismHudStaff, &g_Config.m_PrismHudStaffX, &g_Config.m_PrismHudStaffY},
    };
    for(int i = 0; i < 5; ++i)
    {
        if(!*aModules[i].m_pEnabled)
            continue;
        char aText[192] = {};
        switch(i)
        {
        case 0:
            str_format(aText, sizeof(aText), "Hotkeys  %s | macros %d", g_Config.m_PrismDoubleEnabled ? "Double Tee ON" : "Double Tee OFF", GameClient()->m_PrismMacros.ActiveCount());
            break;
        case 1:
            str_copy(aText, "PRISM  /  0.1.0  Phase 3");
            break;
        case 2:
            str_format(aText, sizeof(aText), "Performance  %.0f FPS  %.2f ms", 1.0f / std::max(0.00001f, Client()->FrameTimeAverage()), Client()->FrameTimeAverage() * 1000.0f);
            break;
        case 3:
            str_format(aText, sizeof(aText), "Dummy  %s | %s", Client()->DummyConnected() ? "connected" : "offline", g_Config.m_PrismDoubleEnabled ? "assistant ON" : "assistant OFF");
            break;
        case 4:
        {
            int Verified = 0;
            const char *pVerifiedName = nullptr;
            for(int Id = 0; Id < MAX_CLIENTS; ++Id)
            {
                // Auth level comes exclusively from server-supplied client state.
                // No guesses based on nicknames or third-party staff lists.
                if(GameClient()->m_aClients[Id].m_Active && GameClient()->m_aClients[Id].m_AuthLevel > 0 && GameClient()->m_Snap.m_apPlayerInfos[Id])
                {
                    ++Verified;
                    if(!pVerifiedName) pVerifiedName = GameClient()->m_aClients[Id].m_aName;
                }
            }
            if(Verified)
                str_format(aText, sizeof(aText), "Verified server staff: %d | %s", Verified, pVerifiedName);
            else
                str_copy(aText, "Staff: no server-verified status available");
            break;
        }
        }
        const float Width = 185.0f * Scale / 100.0f;
        const float Height = 18.0f * Scale / 100.0f;
        const float X = PrismQol::HudCoordinate(*aModules[i].m_pX, m_Width, Width);
        const float Y = PrismQol::HudCoordinate(*aModules[i].m_pY, m_Height, Height);
        Graphics()->DrawRect(X, Y, Width, Height, ColorRGBA(0.065f, 0.08f, 0.11f, 0.73f * Opacity), IGraphics::CORNER_ALL, 4.0f);
        Graphics()->DrawRect(X + 1.0f, Y + 1.0f, Width - 2.0f, 1.0f, ColorRGBA(0.8f, 0.86f, 0.95f, 0.10f * Opacity), IGraphics::CORNER_ALL, 1.0f);
        TextRender()->TextColor(0.91f, 0.94f, 0.98f, Opacity);
        TextRender()->Text(X + 5.0f, Y + 5.0f * Scale / 100.0f, 7.0f * Scale / 100.0f, aText, Width - 9.0f);
    }
    TextRender()->TextColor(TextRender()->DefaultTextColor());
}
'''
p = Path("src/game/client/components/hud.cpp")
s = p.read_text(encoding="utf-8")
if "void CHud::RenderPrismModules()" not in s:
    p.write_text(s + HUD, encoding="utf-8")
    print("patched HUD module renderer")

# Focused tests are appended to the existing Prism test translation unit;
# never alter the already validated Phase 2 tests.
p = Path("src/test/prism_test.cpp")
s = p.read_text(encoding="utf-8")
if "TEST(PrismMacro, ParseRejectsUnboundedOrMalformedActions)" not in s:
    s = s.replace('#include <game/client/prism.h>\n', '#include <game/client/prism.h>\n#include <game/client/prism_qol.h>\n', 1)
    s += r'''

TEST(PrismMacro, ParseRejectsUnboundedOrMalformedActions)
{
    PrismQol::SSequence Sequence;
    EXPECT_FALSE(PrismQol::Parse("11:0", Sequence));
    EXPECT_FALSE(PrismQol::Parse("1:5001", Sequence));
    EXPECT_FALSE(PrismQol::Parse("1:2;", Sequence));
    EXPECT_FALSE(PrismQol::Parse("fire", Sequence));
    EXPECT_FALSE(PrismQol::Parse("1:1;1:1;1:1;1:1;1:1;1:1;1:1;1:1;1:1", Sequence));
    EXPECT_TRUE(PrismQol::Parse("1:1;2:5;9:0", Sequence));
    EXPECT_EQ(Sequence.m_Count, 3);
}
TEST(PrismMacro, SerializationRoundTrip)
{
    PrismQol::SSequence Sequence;
    ASSERT_TRUE(PrismQol::Parse("1:25;2:45;9:0", Sequence));
    char aText[512];
    ASSERT_TRUE(PrismQol::Encode(Sequence, aText, sizeof(aText)));
    EXPECT_STREQ(aText, "1:25;2:45;9:0");
    EXPECT_FALSE(PrismQol::Encode(Sequence, aText, 3));
}
TEST(PrismMacro, OnceRunsNonBlockingAndReleasesOwnedInputs)
{
    PrismQol::CMacroEngine Engine;
    Engine.Configure(0, "1:30;2:10", 44, PrismQol::ONCE, true);
    Engine.KeyEvent(44, true, false, 100);
    Engine.Tick(100, true);
    EXPECT_EQ(Engine.Owned(), PrismQol::OWN_LEFT);
    Engine.Tick(129, true);
    EXPECT_EQ(Engine.Owned(), PrismQol::OWN_LEFT);
    Engine.Tick(130, true);
    EXPECT_EQ(Engine.Owned(), 0);
    Engine.Tick(141, true);
    EXPECT_EQ(Engine.ActiveCount(), 0);
}
TEST(PrismMacro, HoldStopsAndCleansUpOnRelease)
{
    PrismQol::CMacroEngine Engine;
    Engine.Configure(1, "5:40;0:40", 45, PrismQol::HOLD, true);
    Engine.KeyEvent(45, true, false, 0);
    Engine.Tick(0, true);
    EXPECT_EQ(Engine.Owned(), PrismQol::OWN_JUMP);
    Engine.KeyEvent(45, false, false, 1);
    EXPECT_EQ(Engine.Owned(), 0);
    EXPECT_EQ(Engine.ActiveCount(), 0);
}
TEST(PrismMacro, ToggleStopsOnSecondPress)
{
    PrismQol::CMacroEngine Engine;
    Engine.Configure(0, "7:100", 46, PrismQol::TOGGLE, true);
    Engine.KeyEvent(46, true, false, 0);
    Engine.Tick(0, true);
    Engine.KeyEvent(46, false, false, 1);
    Engine.KeyEvent(46, true, false, 2);
    EXPECT_EQ(Engine.ActiveCount(), 0);
    EXPECT_EQ(Engine.Owned(), 0);
}
TEST(PrismMacro, FocusLossAndConfigChangeCancelOwnedInput)
{
    PrismQol::CMacroEngine Engine;
    Engine.Configure(0, "3:100", 47, PrismQol::TOGGLE, true);
    Engine.KeyEvent(47, true, false, 0);
    Engine.Tick(0, true);
    EXPECT_EQ(Engine.Owned(), PrismQol::OWN_RIGHT);
    Engine.Tick(10, false);
    EXPECT_EQ(Engine.Owned(), 0);
    Engine.Configure(0, "3:100", 47, PrismQol::TOGGLE, false);
    EXPECT_EQ(Engine.ActiveCount(), 0);
}
TEST(PrismMacro, FireIsSinglePulse)
{
    PrismQol::CMacroEngine Engine;
    Engine.Configure(0, "9:50", 48, PrismQol::ONCE, true);
    Engine.KeyEvent(48, true, false, 0);
    Engine.Tick(0, true);
    EXPECT_TRUE(Engine.TakeFirePulse());
    EXPECT_FALSE(Engine.TakeFirePulse());
}
TEST(PrismHud, NormalizedPositionsRemainWithinBounds)
{
    EXPECT_FLOAT_EQ(PrismQol::HudCoordinate(10000, 300, 40), 260);
    EXPECT_FLOAT_EQ(PrismQol::HudCoordinate(-500, 300, 40), 0);
    EXPECT_EQ(PrismQol::HudNormalize(150, 300), 5000);
    EXPECT_EQ(PrismQol::HudNormalize(0, 0), 0);
}
'''
    p.write_text(s, encoding="utf-8")
    print("appended Phase 3 focused tests")
print("Phase 3 backend integration complete")
