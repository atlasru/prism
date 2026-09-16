// Prism additions, distributed under the zlib license in license.txt.
#include <game/client/prism.h>
#include <game/client/prism_qol.h>
#include <engine/shared/console.h>
#include <gtest/gtest.h>
#include <climits>
#include <memory>
#include <vector>

class PrismConfig : public ::testing::Test
{
protected:
 CConfig m_Config{};
 CConsole m_Console{CFGFLAG_CLIENT};
 std::vector<std::unique_ptr<SConfigVariable>> m_Variables;
 void SetUp() override
 {
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) m_Variables.emplace_back(new SIntConfigVariable(&m_Console, #ScriptName, SConfigVariable::VAR_INT, Flags, Desc, &m_Config.m_##Name, Def, Min, Max));
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) m_Variables.emplace_back(new SColorConfigVariable(&m_Console, #ScriptName, SConfigVariable::VAR_COLOR, Flags, Desc, &m_Config.m_##Name, Def));
#include <engine/shared/prism_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
  m_Variables.emplace_back(new SIntConfigVariable(&m_Console, "prism_preset", SConfigVariable::VAR_INT, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Preset", &m_Config.m_PrismPreset, 0, 0, 4));
  for(auto &Variable : m_Variables) Variable->Register();
  m_Console.StoreCommands(false);
 }
};

TEST_F(PrismConfig, DefaultRestoresOriginalVisualPath)
{
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 Prism::ApplyPreset(m_Config, Prism::DEFAULT);
 EXPECT_EQ(m_Config.m_PrismEnabled, 0);
 EXPECT_EQ(m_Config.m_PrismLocalGlow, 0);
 EXPECT_EQ(m_Config.m_PrismOtherHookGlow, 0);
 EXPECT_TRUE(Prism::Equal(Prism::Capture(m_Config), Prism::CVisualSettings{}));
}
TEST_F(PrismConfig, PresetsAndCustomAreStable)
{
 for(int Id = Prism::DEFAULT; Id <= Prism::CINEMATIC; ++Id)
 {
  Prism::ApplyPreset(m_Config, Id);
  Prism::Validate(m_Config);
  EXPECT_EQ(m_Config.m_PrismPreset, Id);
 }
 m_Config.m_PrismLocalGlowIntensity = 12;
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::CUSTOM);
 Prism::ApplyPreset(m_Config, Prism::CUSTOM);
 EXPECT_EQ(m_Config.m_PrismLocalGlowIntensity, 12);
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 EXPECT_EQ(m_Config.m_PrismLocalGlowIntensity, 65);
}
TEST_F(PrismConfig, CleanHasNoGlowAndOnlyLocalOutline)
{
 Prism::ApplyPreset(m_Config, Prism::CLEAN);
 EXPECT_EQ(m_Config.m_PrismLocalOutline, 1);
 EXPECT_EQ(m_Config.m_PrismOtherOutline, 0);
 EXPECT_EQ(m_Config.m_PrismLocalGlow + m_Config.m_PrismOtherGlow + m_Config.m_PrismLocalHookGlow + m_Config.m_PrismOtherHookGlow, 0);
}
TEST_F(PrismConfig, TogglePreservesEffectSettingsAndGameplayConfiguration)
{
 m_Config.m_ClPredict = 1;
 m_Config.m_ClAntiPing = 1;
 m_Config.m_ClShowhud = 0;
 m_Config.m_ClShowfps = 1;
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 const auto Before = Prism::Capture(m_Config);
 m_Config.m_PrismEnabled = 0;
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismEnabled, 0);
 m_Config.m_PrismEnabled = 1;
 EXPECT_TRUE(Prism::Equal(Before, Prism::Capture(m_Config)));
 Prism::Reset(m_Config);
 EXPECT_EQ(m_Config.m_ClPredict, 1);
 EXPECT_EQ(m_Config.m_ClAntiPing, 1);
 EXPECT_EQ(m_Config.m_ClShowhud, 0);
 EXPECT_EQ(m_Config.m_ClShowfps, 1);
}
TEST_F(PrismConfig, ConsoleClampsInvalidValues)
{
 m_Console.ExecuteLine("prism_local_glow_intensity 999999");
 EXPECT_EQ(m_Config.m_PrismLocalGlowIntensity, 100);
 m_Console.ExecuteLine("prism_other_outline_width -50");
 EXPECT_EQ(m_Config.m_PrismOtherOutlineWidth, 1);
 m_Console.ExecuteLine("prism_enabled -9");
 EXPECT_EQ(m_Config.m_PrismEnabled, 0);
 m_Console.ExecuteLine("prism_local_glow_intensity not_a_number");
 EXPECT_GE(m_Config.m_PrismLocalGlowIntensity, 0);
 EXPECT_LE(m_Config.m_PrismLocalGlowIntensity, 100);
 m_Config.m_PrismOtherGlowIntensity = INT_MAX;
 m_Config.m_PrismLocalOutlineWidth = INT_MIN;
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismOtherGlowIntensity, 100);
 EXPECT_EQ(m_Config.m_PrismLocalOutlineWidth, 1);
}
TEST_F(PrismConfig, SerializedSettingsRoundTripThroughDDNetConsole)
{
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 m_Console.ExecuteLine("prism_local_glow_intensity 37");
 Prism::Validate(m_Config);
 const auto Expected = Prism::Capture(m_Config);
 std::vector<std::string> Lines;
 for(auto &Variable : m_Variables)
 {
  char aLine[256];
  Variable->Serialize(aLine, sizeof(aLine));
  Lines.emplace_back(aLine);
 }
 Prism::Reset(m_Config);
 for(const auto &Line : Lines) m_Console.ExecuteLine(Line.c_str());
 Prism::Validate(m_Config);
 EXPECT_TRUE(Prism::Equal(Expected, Prism::Capture(m_Config)));
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::CUSTOM);
}
TEST_F(PrismConfig, ResetDoesNotTouchUnrelatedSettings)
{
 m_Config.m_ClPredict = 1;
 m_Config.m_PrismOverlay = 1;
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 Prism::Reset(m_Config);
 EXPECT_EQ(m_Config.m_PrismOverlay, 0);
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::DEFAULT);
 EXPECT_EQ(m_Config.m_ClPredict, 1);
}
TEST_F(PrismConfig, OverlayPreferencesClampAndBecomeCustom)
{
 Prism::ApplyPreset(m_Config, Prism::CINEMATIC);
 m_Console.ExecuteLine("prism_menu_scale 9999");
 m_Console.ExecuteLine("prism_panel_opacity -100");
 m_Console.ExecuteLine("prism_reduced_motion 8");
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismMenuScale, 120);
 EXPECT_EQ(m_Config.m_PrismPanelOpacity, 50);
 EXPECT_EQ(m_Config.m_PrismReducedMotion, 1);
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::CUSTOM);
 Prism::ApplyPreset(m_Config, Prism::DEFAULT);
 EXPECT_EQ(m_Config.m_PrismEnabled, 0);
 EXPECT_EQ(m_Config.m_PrismMenuScale, 100);
 EXPECT_EQ(m_Config.m_PrismPanelOpacity, 85);
 EXPECT_EQ(m_Config.m_PrismReducedMotion, 0);
}
TEST_F(PrismConfig, OverlayPreferencesSurviveConsoleSerialization)
{
 Prism::ApplyPreset(m_Config, Prism::CLEAN);
 m_Console.ExecuteLine("prism_menu_scale 112");
 m_Console.ExecuteLine("prism_panel_opacity 76");
 m_Console.ExecuteLine("prism_reduced_motion 1");
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::CUSTOM);
 std::vector<std::string> Lines;
 for(auto &Variable : m_Variables)
 {
  char aLine[256];
  Variable->Serialize(aLine, sizeof(aLine));
  Lines.emplace_back(aLine);
 }
 Prism::Reset(m_Config);
 for(const auto &Line : Lines) m_Console.ExecuteLine(Line.c_str());
 Prism::Validate(m_Config);
 EXPECT_EQ(m_Config.m_PrismMenuScale, 112);
 EXPECT_EQ(m_Config.m_PrismPanelOpacity, 76);
 EXPECT_EQ(m_Config.m_PrismReducedMotion, 1);
 EXPECT_EQ(m_Config.m_PrismPreset, Prism::CUSTOM);
}


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
