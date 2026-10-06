#include "prism_fixture.h"

#include <engine/shared/datafile.h>
#include <engine/shared/map.h>
#include <engine/storage.h>

#include <game/client/pathfinder/controller.h>
#include <game/client/pathfinder/search.h>
#include <game/client/prism_input.h>

#include <cstdlib>

using namespace PrismPath;

namespace
{
	void ExecuteInput(CGameWorld &World, CNetObj_PlayerInput Input)
	{
		++World.m_GameTick;
		World.GetCharacterById(0)->OnDirectInput(&Input);
		World.GetCharacterById(0)->OnPredictedInput(&Input);
		World.Tick();
	}
	bool ContinueAutopilot(CGameWorld &World, vec2 Destination, int Horizon = 100)
	{
		CController Controller(false);
		Controller.SetGoals({Destination});
		SSettings Settings;
		Settings.m_Autopilot = true;
		Settings.m_BudgetUs = 8000;
		Settings.m_Horizon = Horizon;
		for(int Tick = 0; Tick < 1000 && Controller.Status() != EStatus::FINISHED; ++Tick)
		{
			CNetObj_PlayerInput Input{};
			Input.m_TargetX = 100;
			Controller.Compose(&World, 0, Input, true, true, false, Settings);
			ExecuteInput(World, Input);
			if(CSimulator::CheckPosition(*World.Collision(), World.GetCharacterById(0)->Core()->m_Pos) != EReject::NONE)
				return false;
		}
		if(Controller.Status() != EStatus::FINISHED)
			ADD_FAILURE() << Controller.Reason() << " position " << World.GetCharacterById(0)->Core()->m_Pos.x << "," << World.GetCharacterById(0)->Core()->m_Pos.y;
		return Controller.Status() == EStatus::FINISHED;
	}
	SPlan Solve(CGameWorld &World, CMapAnalysis &Map, CNavigation &Nav, vec2 Destination, int Horizon = 100)
	{
		if(!Map.Begin(*World.Collision()))
			return {};
		while(!Map.Step()) {}
		if(!Nav.Begin(Map, {Destination}, true))
			return {};
		while(!Nav.Step()) {}
		CPhysicsSearch Search;
		SSettings Settings;
		Settings.m_Horizon = Horizon;
		if(!Search.Begin(World, 0, Map, Nav, Settings))
			return {};
		for(int i = 0; i < 200 && !Search.Step(0, 256); ++i) {}
		return Search.Result();
	}
}

TEST(Pathfinder, FireSuppressionDoesNotInventPresses)
{
	PrismInput::CFireComposer Fire;
	Fire.Reset(0, INPUT_STATE_MASK);
	EXPECT_EQ(Fire.Suppress(1, INPUT_STATE_MASK), 0);
	EXPECT_EQ(Fire.Suppress(2, INPUT_STATE_MASK), 0);
	EXPECT_EQ(Fire.Compose(2, false, INPUT_STATE_MASK), 0);
	EXPECT_EQ(Fire.Compose(3, false, INPUT_STATE_MASK), 1);
	EXPECT_EQ(Fire.Suppress(3, INPUT_STATE_MASK), 2);
	EXPECT_EQ(Fire.Compose(4, false, INPUT_STATE_MASK), 4);
}

TEST(Pathfinder, PhysicalHashSeparatesMomentumAndEdges)
{
	SState A;
	A.m_Pos = vec2(100, 100);
	SState B = A;
	B.m_Vel.x = 8;
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, false));
	B = A;
	B.m_JumpedTotal = 1;
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, false));
	B = A;
	B.m_HookState = HOOK_GRABBED;
	B.m_HookPos = vec2(100, 0);
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, false, true));
	B = A;
	B.m_Pos.x += 0.1f;
	EXPECT_EQ(SStateKey::From(A, false, false), SStateKey::From(B, false, false));
	EXPECT_FALSE(SStateKey::From(A, false, false) == SStateKey::From(B, true, false));
}
TEST(Pathfinder, DivergenceAndReleaseFailClosed)
{
	SState A, B;
	EXPECT_FALSE(Diverged(A, B, 20));
	B.m_Pos.x = 21;
	EXPECT_TRUE(Diverged(A, B, 20));
	B = A;
	B.m_HookState = HOOK_GRABBED;
	EXPECT_TRUE(Diverged(A, B, 20));
	CNetObj_PlayerInput Input{};
	Input.m_Direction = -1;
	Input.m_Jump = Input.m_Hook = 1;
	Input.m_Fire = 63;
	Input.m_WantedWeapon = 5;
	ReleaseInput(Input);
	EXPECT_EQ(Input.m_Direction, 0);
	EXPECT_EQ(Input.m_Jump, 0);
	EXPECT_EQ(Input.m_Hook, 0);
	EXPECT_EQ(Input.m_Fire, 0);
	EXPECT_EQ(Input.m_WantedWeapon, 0);
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
	m_Map.Set(20, 9, TILE_FINISH);
	m_Map.Set(30, 9, TILE_FINISH, true);
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
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source, Reference;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
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
	for(int X = 4; X <= 15; ++X)
		m_Map.Set(X, 3, X < 8 ? TILE_NOHOOK : TILE_SOLID);
	Init();
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	while(!Map.Step()) {}
	vec2 Points[MAX_ANCHORS];
	const int Count = Map.HookCandidates(m_Core.m_Pos, vec2(350, 170), 380, Points, MAX_ANCHORS);
	ASSERT_GT(Count, 0);
	for(int i = 0; i < Count; ++i)
	{
		vec2 Hit, Before;
		int Tele = 0;
		EXPECT_EQ(m_Collision.IntersectLineTeleHook(m_Core.m_Pos, m_Core.m_Pos + normalize(Points[i] - m_Core.m_Pos) * 380, &Hit, &Before, &Tele), TILE_SOLID);
		EXPECT_EQ(Tele, 0);
	}
}
TEST_F(CPrismPhysics, PathfinderFlatFloorDiscoversWalkingInputs)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	while(!Map.Step()) {}
	CNavigation Nav;
	ASSERT_TRUE(Nav.Begin(Map, {vec2(560, 305)}, true));
	while(!Nav.Step()) {}
	CPhysicsSearch Search;
	SSettings Settings;
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
	for(int X = 1; X < 40; ++X)
		m_Map.Set(X, 8, TILE_FREEZE, true);
	Init();
	EXPECT_EQ(CSimulator::CheckPosition(m_Collision, vec2(176, 289)), EReject::NONE);
	EXPECT_EQ(CSimulator::CheckPosition(m_Collision, vec2(176, 280)), EReject::FREEZE);
	EXPECT_EQ(CSimulator::CheckSegment(m_Collision, vec2(176, 305), vec2(560, 305)), EReject::NONE);
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	while(!Map.Step()) {}
	CNavigation Nav;
	ASSERT_TRUE(Nav.Begin(Map, {vec2(560, 305)}, true));
	while(!Nav.Step()) {}
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CPhysicsSearch Search;
	ASSERT_TRUE(Search.Begin(Source, 0, Map, Nav, {}));
	for(int i = 0; i < 200 && !Search.Step(0, 256); ++i) {}
	ASSERT_TRUE(Search.Result().Valid());
	EXPECT_GT(Search.Result().m_aStates[Search.Result().m_Ticks].m_Pos.x, 500);
}

TEST_F(CPrismPhysics, PathfinderDiscoversJumpOverDeathGap)
{
	for(int X = 8; X <= 12; ++X)
	{
		m_Map.Set(X, 10, TILE_AIR);
		m_Map.Set(X, 13, TILE_DEATH);
	}
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CMapAnalysis Map;
	CNavigation Nav;
	const SPlan Plan = Solve(Source, Map, Nav, vec2(560, 305));
	ASSERT_TRUE(Plan.Valid());
	bool Jumped = false;
	for(int i = 0; i < Plan.m_Ticks; ++i)
	{
		Jumped |= Plan.m_aInputs[i].m_Jump;
		ExecuteInput(Source, Plan.m_aInputs[i]);
		EXPECT_EQ(CSimulator::CheckPosition(m_Collision, Source.GetCharacterById(0)->Core()->m_Pos), EReject::NONE);
	}
	EXPECT_TRUE(Jumped);
	EXPECT_TRUE(ContinueAutopilot(Source, Nav.Goal(Source.GetCharacterById(0)->Core()->m_Pos), 200));
	EXPECT_TRUE(Nav.AtGoal(Source.GetCharacterById(0)->Core()->m_Pos)) << "end " << Source.GetCharacterById(0)->Core()->m_Pos.x << "," << Source.GetCharacterById(0)->Core()->m_Pos.y << " ticks " << Plan.m_Ticks;
}
TEST_F(CPrismPhysics, PathfinderHookLiftAndReleaseUsesAuthoritativeAttachment)
{
	// A 224-unit elevation with one normal jump. No map-specific production inputs.
	for(int X = 0; X < CMemoryMap::WIDTH; ++X)
	{
		m_Map.Set(X, 10, TILE_AIR);
		m_Map.Set(X, 20, TILE_SOLID);
	}
	for(int X = 9; X < 19; ++X)
		m_Map.Set(X, 13, TILE_SOLID);
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	auto *pChar = Character(Source, 0, vec2(176, 625));
	auto Core = pChar->GetCore();
	Core.m_Jumps = 1;
	pChar->SetCore(Core);
	CMapAnalysis Map;
	CNavigation Nav;
	const SPlan Plan = Solve(Source, Map, Nav, vec2(450, 401), 200);
	ASSERT_TRUE(Plan.Valid());
	bool Attached = false, Released = false;
	for(int i = 0; i < Plan.m_Ticks; ++i)
	{
		ExecuteInput(Source, Plan.m_aInputs[i]);
		if(Source.GetCharacterById(0)->Core()->m_HookState == HOOK_GRABBED)
			Attached = true;
		if(Attached && !Plan.m_aInputs[i].m_Hook)
			Released = true;
	}
	EXPECT_TRUE(Plan.m_UsesHook);
	EXPECT_TRUE(Attached);
	EXPECT_TRUE(Released);
	EXPECT_TRUE(ContinueAutopilot(Source, Nav.Goal(Source.GetCharacterById(0)->Core()->m_Pos), 200));
	EXPECT_TRUE(Nav.AtGoal(Source.GetCharacterById(0)->Core()->m_Pos)) << "end " << Source.GetCharacterById(0)->Core()->m_Pos.x << "," << Source.GetCharacterById(0)->Core()->m_Pos.y << " ticks " << Plan.m_Ticks;
}
TEST_F(CPrismPhysics, PathfinderAlternateCorridorAvoidsDirectFreezeWall)
{
	for(int Y = 6; Y < 10; ++Y)
		m_Map.Set(10, Y, TILE_FREEZE, true);
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CMapAnalysis Map;
	CNavigation Nav;
	const SPlan Plan = Solve(Source, Map, Nav, vec2(560, 305), 160);
	ASSERT_TRUE(Plan.Valid());
	float MinimumY = 305;
	for(int i = 0; i < Plan.m_Ticks; ++i)
	{
		ExecuteInput(Source, Plan.m_aInputs[i]);
		const vec2 P = Source.GetCharacterById(0)->Core()->m_Pos;
		MinimumY = std::min(MinimumY, P.y);
		EXPECT_EQ(CSimulator::CheckPosition(m_Collision, P), EReject::NONE);
	}
	EXPECT_LT(MinimumY, 6 * 32);
	EXPECT_TRUE(Nav.AtGoal(Source.GetCharacterById(0)->Core()->m_Pos)) << "end " << Source.GetCharacterById(0)->Core()->m_Pos.x << "," << Source.GetCharacterById(0)->Core()->m_Pos.y << " ticks " << Plan.m_Ticks;
}
TEST_F(CPrismPhysics, PathfinderAutopilotCompletesAndReplansAfterInputDivergence)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CController Controller(false);
	Controller.SetGoals({vec2(900, 305)});
	SSettings Settings;
	Settings.m_Autopilot = true;
	Settings.m_BudgetUs = 8000;
	bool Displaced = false;
	uint64_t ReplansBefore = 0;
	for(int Tick = 0; Tick < 600 && Controller.Status() != EStatus::FINISHED; ++Tick)
	{
		CNetObj_PlayerInput Input{};
		Input.m_TargetX = 100;
		Controller.Compose(&Source, 0, Input, true, true, false, Settings);
		if(!Displaced && Controller.Status() == EStatus::EXECUTING && Source.GetCharacterById(0)->Core()->m_Pos.x > 300)
		{
			ReplansBefore = Controller.Stats().m_Replans;
			// Unexpected external input, no position or velocity manipulation.
			Input.m_Direction = -1;
			Input.m_Jump = 1;
			Displaced = true;
		}
		ExecuteInput(Source, Input);
	}
	EXPECT_TRUE(Displaced);
	EXPECT_GT(Controller.Stats().m_Replans, ReplansBefore);
	EXPECT_EQ(Controller.Status(), EStatus::FINISHED) << Controller.Reason();
	EXPECT_FALSE(Controller.Owned());
	EXPECT_NEAR(Source.GetCharacterById(0)->Core()->m_Pos.x, 900, 24);
}
TEST_F(CPrismPhysics, PathfinderPauseDisableResetAndDisconnectReleaseOwnership)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CController C(false);
	C.SetGoals({vec2(900, 305)});
	SSettings Settings;
	Settings.m_Autopilot = true;
	Settings.m_BudgetUs = 8000;
	CNetObj_PlayerInput Input{};
	EXPECT_TRUE(C.Compose(&Source, 0, Input, true, true, false, Settings));
	EXPECT_TRUE(C.Owned());
	C.Pause();
	Input.m_Direction = 1;
	Input.m_Jump = Input.m_Hook = 1;
	Input.m_Fire = 1;
	EXPECT_TRUE(C.Compose(&Source, 0, Input, true, true, false, Settings));
	EXPECT_EQ(Input.m_Direction, 0);
	EXPECT_EQ(Input.m_Jump, 0);
	EXPECT_EQ(Input.m_Hook, 0);
	EXPECT_EQ(Input.m_Fire & 1, 0);
	C.Resume();
	C.Compose(&Source, 0, Input, true, true, false, Settings);
	EXPECT_TRUE(C.Compose(nullptr, -1, Input, true, false, false, Settings));
	EXPECT_FALSE(C.Owned());
	EXPECT_TRUE(C.Paused());
	C.Resume();
	C.Compose(&Source, 0, Input, true, true, false, Settings);
	EXPECT_TRUE(C.Compose(&Source, 0, Input, false, true, false, Settings));
	EXPECT_FALSE(C.Owned());
	C.Resume();
	C.Compose(&Source, 0, Input, true, true, false, Settings);
	C.Reset();
	EXPECT_TRUE(C.Compose(nullptr, -1, Input, false, false, false, Settings));
	EXPECT_FALSE(C.Map().Ready());
}
TEST_F(CPrismPhysics, PathfinderAssistAndManualOverrideLeavePhysicalInputUntouched)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CController C(false);
	C.SetGoals({vec2(900, 305)});
	SSettings Settings;
	CNetObj_PlayerInput Input{};
	Input.m_Direction = -1;
	Input.m_Hook = Input.m_Jump = 1;
	EXPECT_FALSE(C.Compose(&Source, 0, Input, true, true, false, Settings));
	EXPECT_EQ(Input.m_Direction, -1);
	EXPECT_EQ(Input.m_Hook, 1);
	EXPECT_EQ(Input.m_Jump, 1);
	Settings.m_Autopilot = true;
	EXPECT_FALSE(C.Compose(&Source, 0, Input, true, true, true, Settings));
	EXPECT_TRUE(C.Paused());
	EXPECT_FALSE(C.Owned());
	EXPECT_EQ(Input.m_Direction, -1);
	EXPECT_EQ(Input.m_Hook, 1);
	EXPECT_EQ(Input.m_Jump, 1);
}

TEST_F(CPrismPhysics, PathfinderRealTutorialMapDiscoversAndExecutesLocalRoute)
{
	auto pStorage = CreateLocalStorage();
	ASSERT_NE(pStorage, nullptr);
	CMap Loaded;
	ASSERT_TRUE(Loaded.Load(pStorage.get(), PRISM_TEST_MAP_DIR "/Tutorial.map", IStorage::TYPE_ABSOLUTE));
	m_Layers.Init(&Loaded, false, false);
	m_Collision.Init(&m_Layers);
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	while(!Map.Step()) {}
	ASSERT_FALSE(Map.Finishes().empty());
	std::vector<vec2> Spawns;
	for(int i = 0; i < Map.Width() * Map.Height(); ++i)
		if(Map.Cells()[i].m_Flags & SPAWN)
			Spawns.push_back(m_Collision.GetPos(i));
	ASSERT_FALSE(Spawns.empty());
	const vec2 Spawn = Spawns[0];
	vec2 Destination = Spawn;
	float Best = UNREACHABLE;
	// Discover a nearby safe standing target from geometry, not a saved solution.
	for(int i = 0; i < Map.Width() * Map.Height(); ++i)
	{
		const auto &Cell = Map.Cells()[i];
		const vec2 P = m_Collision.GetPos(i);
		const float D = distance(P, Spawn);
		if(!Cell.m_Passable || !(Cell.m_Flags & FLOOR) || D < 160 || D > 300)
			continue;
		CNavigation Nav;
		if(!Nav.Begin(Map, {P}, true))
			continue;
		while(!Nav.Step()) {}
		const float Cost = Nav.Cost(Spawn);
		if(Cost < Best)
		{
			Best = Cost;
			Destination = P;
		}
	}
	ASSERT_LT(Best, UNREACHABLE);
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, Spawn);
	EXPECT_TRUE(ContinueAutopilot(Source, Destination, 160));
	EXPECT_LT(distance(Source.GetCharacterById(0)->Core()->m_Pos, Destination), 24);
	std::printf("Tutorial map %dx%d: %zu regions, %zu finishes, %zu teleport inputs; autonomous local displacement %.0f units\n", Map.Width(), Map.Height(), Map.Regions().size(), Map.Finishes().size(), Map.Teleports().size(), distance(Spawn, Source.GetCharacterById(0)->Core()->m_Pos));
}

TEST_F(CPrismPhysics, PathfinderMultiStageGapAndHookAutopilot)
{
	for(int X = 0; X < CMemoryMap::WIDTH; ++X)
	{
		m_Map.Set(X, 10, TILE_AIR);
		m_Map.Set(X, 20, TILE_SOLID);
	}
	for(int X = 8; X <= 12; ++X)
	{
		m_Map.Set(X, 20, TILE_AIR);
		m_Map.Set(X, 23, TILE_DEATH);
	}
	for(int X = 18; X <= 30; ++X)
		m_Map.Set(X, 13, TILE_SOLID);
	m_Map.Set(5, 19, ENTITY_SPAWN + ENTITY_OFFSET);
	m_Map.Set(6, 19, TILE_START);
	m_Map.Set(24, 12, TILE_FINISH);
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	auto *pChar = Character(Source, 0, vec2(176, 625));
	auto Core = pChar->GetCore();
	Core.m_Jumps = 1;
	pChar->SetCore(Core);
	CController Controller(false);
	SSettings Settings;
	Settings.m_Autopilot = true;
	Settings.m_BudgetUs = 8000;
	Settings.m_Horizon = 200;
	bool Attached = false, Released = false, Jumped = false;
	for(int Tick = 0; Tick < 1500 && Controller.Status() != EStatus::FINISHED; ++Tick)
	{
		CNetObj_PlayerInput Input{};
		Input.m_TargetX = 100;
		Controller.Compose(&Source, 0, Input, true, true, false, Settings);
		ExecuteInput(Source, Input);
		Jumped |= Input.m_Jump != 0;
		Attached |= Source.GetCharacterById(0)->Core()->m_HookState == HOOK_GRABBED;
		Released |= Attached && !Input.m_Hook;
		ASSERT_EQ(CSimulator::CheckPosition(m_Collision, Source.GetCharacterById(0)->Core()->m_Pos), EReject::NONE);
	}
	EXPECT_EQ(Controller.Status(), EStatus::FINISHED) << Controller.Reason() << " position " << Source.GetCharacterById(0)->Core()->m_Pos.x << "," << Source.GetCharacterById(0)->Core()->m_Pos.y;
	EXPECT_TRUE(Jumped);
	EXPECT_TRUE(Attached);
	EXPECT_TRUE(Released);
	// Optional export for exercising the actual network client/server, never consumed by production search.
	if(const char *pFile = std::getenv("PRISM_PATHFINDER_EXPORT_MAP"))
	{
		auto Storage = CreateLocalStorage();
		CDataFileWriter Writer;
		ASSERT_TRUE(Writer.Open(Storage.get(), pFile, IStorage::TYPE_ABSOLUTE));
		CMapItemVersion Version{1};
		CMapItemInfo Info{1, -1, -1, -1, -1};
		Writer.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Version), &Version);
		Writer.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Info), &Info);
		m_Map.m_Group.m_ParallaxX = m_Map.m_Group.m_ParallaxY = 100;
		m_Map.m_Game.m_Color = m_Map.m_Front.m_Color = {255, 255, 255, 255};
		m_Map.m_Game.m_Image = m_Map.m_Front.m_Image = -1;
		m_Map.m_Game.m_ColorEnv = m_Map.m_Front.m_ColorEnv = -1;
		Writer.AddItem(MAPITEMTYPE_GROUP, 0, sizeof(m_Map.m_Group), &m_Map.m_Group);
		Writer.AddItem(MAPITEMTYPE_LAYER, 0, sizeof(m_Map.m_Game), &m_Map.m_Game);
		Writer.AddItem(MAPITEMTYPE_LAYER, 1, sizeof(m_Map.m_Front), &m_Map.m_Front);
		Writer.AddData(sizeof(m_Map.m_aGame), m_Map.m_aGame.data());
		Writer.AddData(sizeof(m_Map.m_aFront), m_Map.m_aFront.data());
		Writer.Finish();
	}
}
TEST_F(CPrismPhysics, PathfinderTimedSwitchPhaseChangesDominanceKey)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	Source.Switchers().resize(1);
	auto &Switch = Source.Switchers()[0];
	Switch.m_aStatus[0] = true;
	Switch.m_aType[0] = TILE_SWITCHTIMEDOPEN;
	Switch.m_aEndTick[0] = 150;
	const auto Before = SStateKey::From(SState::Read(Source, 0), false, false);
	++Source.m_GameTick;
	EXPECT_FALSE(Before == SStateKey::From(SState::Read(Source, 0), false, false));
}

TEST_F(CPrismPhysics, PathfinderSearchYieldsAtDeadlineAndResumesWithoutLosingRoot)
{
	Init();
	CMapBugs Bugs;
	std::array<CTuningParams, 256> Tunes;
	CGameWorld Source;
	World(Source, Bugs, Tunes.data());
	Character(Source, 0, m_Core.m_Pos);
	CMapAnalysis Map;
	ASSERT_TRUE(Map.Begin(m_Collision));
	Map.Step(1024, NowUs() - 1);
	EXPECT_EQ(Map.Progress(), 0);
	while(!Map.Step()) {}
	CNavigation Nav;
	ASSERT_TRUE(Nav.Begin(Map, {vec2(560, 305)}, true));
	while(!Nav.Step()) {}
	CPhysicsSearch Search;
	ASSERT_TRUE(Search.Begin(Source, 0, Map, Nav, {}));
	EXPECT_FALSE(Search.Step(NowUs() - 1));
	EXPECT_EQ(Search.Stats().m_Expanded, 0);
	EXPECT_GT(Search.Stats().m_Timeouts, 0);
	for(int i = 0; i < 200 && !Search.Step(0, 256); ++i) {}
	EXPECT_TRUE(Search.Result().Valid());
	EXPECT_TRUE(Nav.AtGoal(Search.Result().m_aStates[Search.Result().m_Ticks].m_Pos));
}
