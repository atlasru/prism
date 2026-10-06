#include "prism_fixture.h"
#include <game/client/pathfinder/search.h>

using namespace PrismPath;

TEST(Pathfinder, PhysicalHashSeparatesMomentumAndEdges)
{
	SState A;
	A.m_Pos = vec2(100, 100);
	SState B = A;
	B.m_Vel.x = 8;
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, false));
	B = A; B.m_JumpedTotal = 1;
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, false));
	B = A; B.m_HookState = HOOK_GRABBED; B.m_HookPos = vec2(100, 0);
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, true));
	B = A; B.m_Pos.x += 0.1f;
	EXPECT_EQ(SStateKey::From(A, false, false), SStateKey::From(B, false, false));
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, true, false));
}
TEST(Pathfinder, DivergenceAndReleaseFailClosed)
{
	SState A, B;
	EXPECT_FALSE(Diverged(A, B, 20));
	B.m_Pos.x = 21;
	EXPECT_TRUE(Diverged(A, B, 20));
	B = A; B.m_HookState = HOOK_GRABBED;
	EXPECT_TRUE(Diverged(A, B, 20));
	CNetObj_PlayerInput Input{};
	Input.m_Direction = -1; Input.m_Jump = Input.m_Hook = 1; Input.m_Fire = 63;
	Input.m_WantedWeapon = 5;
	ReleaseInput(Input);
	EXPECT_EQ(Input.m_Direction, 0); EXPECT_EQ(Input.m_Jump, 0); EXPECT_EQ(Input.m_Hook, 0);
	EXPECT_EQ(Input.m_Fire, 0); EXPECT_EQ(Input.m_WantedWeapon, 0);
}
TEST(Pathfinder, ScoringRetainsUsefulMomentumAndRejectsFreeze)
{
	SState Slow, Fast;
	Fast.m_Vel.x = 8;
	EXPECT_GT(ScoreState(Fast, 500, 400, vec2(500, 0), 20, true), ScoreState(Slow, 500, 400, vec2(500, 0), 20, true));
	Fast.m_FreezeTime = 150;
	EXPECT_EQ(ScoreState(Fast, 500, 400, vec2(500, 0), 20, true), -UNREACHABLE);
}
TEST_F(CPrismPhysics, PathfinderMapRegionsAndMultipleFinishes)
{
	m_Map.Set(20, 9, TILE_FINISH); m_Map.Set(30, 9, TILE_FINISH, true);
	Init();
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	EXPECT_FALSE(Map.Ready());
	while(!Map.Step(128)) {}
	EXPECT_EQ(Map.Finishes().size(), 2);
	EXPECT_GT(Map.Regions().size(), 1);
	const int Region = Map.RegionAt(m_Core.m_Pos);
	ASSERT_GE(Region, 0);
	EXPECT_TRUE(Map.Regions()[Region].m_Flags & FLOOR);
	CNavigation Nav;
	ASSERT_TRUE(Nav.Begin(Map, {vec2(656, 304), vec2(976, 304)}, true));
	while(!Nav.Step()) {}
	EXPECT_LT(Nav.Cost(m_Core.m_Pos), UNREACHABLE);
	EXPECT_EQ(Nav.Goal(m_Core.m_Pos), vec2(656, 304));
	EXPECT_GT(Nav.Route(m_Core.m_Pos).size(), 2);
}
TEST_F(CPrismPhysics, PathfinderSimulationMatchesPredictionAndLeavesSourceUntouched)
{
	Init();
	CMapBugs Bugs; std::array<CTuningParams, 256> Tunes;
	CGameWorld Source, Reference;
	World(Source, Bugs, Tunes.data()); Character(Source, 0, m_Core.m_Pos);
	CSimulator::Clone(Reference, Source);
	CSimulator Sim;
	Sim.Init(0);
	SAction Action{1, 12, true, false, vec2(0, 0), vec2(100, 0)};
	STrace Trace;
	ASSERT_TRUE(Sim.Simulate(Source, Action, Trace));
	for(int i = 1; i <= 12; ++i)
	{
		CNetObj_PlayerInput Input = Action.Input();
		++Reference.m_GameTick;
		Reference.GetCharacterById(0)->OnDirectInput(&Input);
		Reference.GetCharacterById(0)->OnPredictedInput(&Input);
		Reference.Tick();
		const SState Actual = SState::Read(Reference, 0);
		EXPECT_EQ(Trace.m_aStates[i].m_Pos, Actual.m_Pos);
		EXPECT_EQ(Trace.m_aStates[i].m_Vel, Actual.m_Vel);
		EXPECT_EQ(Trace.m_aStates[i].m_Jumped, Actual.m_Jumped);
	}
	EXPECT_EQ(Source.GameTick(), 100);
	EXPECT_EQ(Source.GetCharacterById(0)->Core()->m_Pos, m_Core.m_Pos);
	EXPECT_EQ(Source.m_pChild, nullptr);
	EXPECT_EQ(Sim.World().m_pParent, nullptr);
}
TEST_F(CPrismPhysics, PathfinderHookCandidatesRejectNoHookAndBlockedFaces)
{
	for(int X = 4; X <= 15; ++X) m_Map.Set(X, 3, X < 8 ? TILE_NOHOOK : TILE_SOLID);
	Init();
	CMapAnalysis Map; ASSERT_TRUE(Map.Begin(m_Collision)); while(!Map.Step()) {}
	vec2 Points[MAX_ANCHORS];
	const int Count = Map.HookCandidates(m_Core.m_Pos, vec2(350, 170), 380, Points, MAX_ANCHORS);
	ASSERT_GT(Count, 0);
	for(int i = 0; i < Count; ++i)
	{
		vec2 Hit, Before; int Tele = 0;
		EXPECT_EQ(m_Collision.IntersectLineTeleHook(m_Core.m_Pos, m_Core.m_Pos + normalize(Points[i] - m_Core.m_Pos) * 380, &Hit, &Before, &Tele), TILE_SOLID);
		EXPECT_EQ(Tele, 0);
	}
}
TEST_F(CPrismPhysics, PathfinderFlatFloorDiscoversWalkingInputs)
{
	Init();
	CMapBugs Bugs; std::array<CTuningParams, 256> Tunes;
	CGameWorld Source; World(Source, Bugs, Tunes.data()); Character(Source, 0, m_Core.m_Pos);
	CMapAnalysis Map; ASSERT_TRUE(Map.Begin(m_Collision)); while(!Map.Step()) {}
	CNavigation Nav; ASSERT_TRUE(Nav.Begin(Map, {vec2(560, 305)}, true)); while(!Nav.Step()) {}
	CPhysicsSearch Search; SSettings Settings;
	ASSERT_TRUE(Search.Begin(Source, 0, Map, Nav, Settings));
	for(int i = 0; i < 200 && !Search.Step(0, 256); ++i) {}
	ASSERT_TRUE(Search.Complete());
	const SPlan Plan = Search.Result();
	ASSERT_TRUE(Plan.Valid());
	EXPECT_TRUE(Nav.AtGoal(Plan.m_aStates[Plan.m_Ticks].m_Pos));
	EXPECT_GT(Search.Stats().m_Simulations, 1);
	EXPECT_LE(Search.Stats().m_Expanded, MAX_NODES);
	EXPECT_GT(Search.Stats().m_Pruned, 0);
	EXPECT_EQ(Source.GetCharacterById(0)->Core()->m_Pos, m_Core.m_Pos);
}
TEST_F(CPrismPhysics, PathfinderNarrowFreezeCorridorRemainsTraversable)
{
	for(int X = 1; X < 40; ++X) m_Map.Set(X, 8, TILE_FREEZE, true);
	Init();
	EXPECT_EQ(CSimulator::CheckPosition(m_Collision, vec2(176, 289)), EReject::NONE);
	EXPECT_EQ(CSimulator::CheckPosition(m_Collision, vec2(176, 280)), EReject::FREEZE);
	EXPECT_EQ(CSimulator::CheckSegment(m_Collision, vec2(176, 305), vec2(560, 305)), EReject::NONE);
	CMapAnalysis Map; ASSERT_TRUE(Map.Begin(m_Collision)); while(!Map.Step()) {}
	CNavigation Nav; ASSERT_TRUE(Nav.Begin(Map, {vec2(560, 305)}, true)); while(!Nav.Step()) {}
	CMapBugs Bugs; std::array<CTuningParams, 256> Tunes;
	CGameWorld Source; World(Source, Bugs, Tunes.data()); Character(Source, 0, m_Core.m_Pos);
	CPhysicsSearch Search; ASSERT_TRUE(Search.Begin(Source, 0, Map, Nav, {}));
	for(int i = 0; i < 200 && !Search.Step(0, 256); ++i) {}
	ASSERT_TRUE(Search.Result().Valid());
	EXPECT_GT(Search.Result().m_aStates[Search.Result().m_Ticks].m_Pos.x, 500);
}
