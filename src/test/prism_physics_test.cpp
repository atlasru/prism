// Prism additions, distributed under the zlib license in license.txt.
#include <engine/map.h>

#include <game/client/prism_route.h>
#include <game/client/prism_shot.h>
#include <game/layers.h>
#include <game/mapbugs.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>

class CMemoryMap : public IMap
{
public:
	static constexpr int WIDTH = 64, HEIGHT = 24;
	std::array<CTile, WIDTH * HEIGHT> m_aGame{}, m_aFront{};
	CMapItemGroup m_Group{};
	CMapItemLayerTilemap m_Game{}, m_Front{};
	CMemoryMap()
	{
		m_Group.m_Version = 3;
		m_Group.m_NumLayers = 2;
		m_Game.m_Layer.m_Type = m_Front.m_Layer.m_Type = LAYERTYPE_TILES;
		m_Game.m_Version = m_Front.m_Version = 3;
		m_Game.m_Width = m_Front.m_Width = WIDTH;
		m_Game.m_Height = m_Front.m_Height = HEIGHT;
		m_Game.m_Flags = TILESLAYERFLAG_GAME;
		m_Front.m_Flags = TILESLAYERFLAG_FRONT;
		m_Game.m_Data = 0;
		m_Front.m_Front = m_Front.m_Data = 1;
		for(int X = 0; X < WIDTH; ++X)
			Set(X, 10, TILE_SOLID);
	}
	void Set(int X, int Y, int Tile, bool Front = false) { (Front ? m_aFront : m_aGame)[Y * WIDTH + X].m_Index = Tile; }
	int GetDataSize(int) const override { return sizeof(m_aGame); }
	void *GetData(int Index) override { return Index ? m_aFront.data() : m_aGame.data(); }
	void *GetDataSwapped(int Index) override { return GetData(Index); }
	const char *GetDataString(int) override { return ""; }
	void UnloadData(int) override {}
	int NumData() const override { return 2; }
	int GetItemSize(int Index) override { return Index ? sizeof(m_Game) : sizeof(m_Group); }
	void *GetItem(int Index, int *pType = nullptr, int *pId = nullptr, CUuid *pUuid = nullptr) override
	{
		if(pType)
			*pType = Index ? MAPITEMTYPE_LAYER : MAPITEMTYPE_GROUP;
		if(pId)
			*pId = Index;
		return Index == 0 ? static_cast<void *>(&m_Group) : Index == 1 ? static_cast<void *>(&m_Game) :
										 static_cast<void *>(&m_Front);
	}
	void GetType(int Type, int *pStart, int *pNum) override
	{
		*pStart = Type == MAPITEMTYPE_GROUP ? 0 : 1;
		*pNum = Type == MAPITEMTYPE_GROUP ? 1 : Type == MAPITEMTYPE_LAYER ? 2 :
										    0;
	}
	int FindItemIndex(int, int) override { return -1; }
	void *FindItem(int, int) override { return nullptr; }
	int NumItems() const override { return 3; }
	bool Load(const char *, IStorage *, const char *, int) override { return false; }
	bool Load(IStorage *, const char *, int) override { return false; }
	void Unload() override {}
	bool IsLoaded() const override { return true; }
	IOHANDLE File() const override { return nullptr; }
	const char *FullName() const override { return "prism_fixture"; }
	const char *BaseName() const override { return "prism_fixture"; }
	const char *Path() const override { return ""; }
	SHA256_DIGEST Sha256() const override { return {}; }
	unsigned Crc() const override { return 0; }
	int Size() const override { return sizeof(m_aGame) * 2; }
};
class CPrismPhysics : public ::testing::Test
{
public:
	CMemoryMap m_Map;
	CLayers m_Layers;
	CCollision m_Collision;
	CTeamsCore m_Teams;
	CTuningParams m_Tuning;
	CWorldCore m_CoreWorld;
	CCharacterCore m_Core;
	PrismAssist::CPredictor m_Predictor;
	CNetObj_PlayerInput m_Input{};
	PrismRoute::SIntent m_Intent;
	void Init()
	{
		m_Layers.Init(&m_Map, false, false);
		m_Collision.Init(&m_Layers);
		m_Teams.Reset();
		m_Core.Init(&m_CoreWorld, &m_Collision, &m_Teams);
		m_Core.Reset();
		m_Core.m_Id = 0;
		m_Core.m_Pos = vec2(176, 305);
		m_Core.m_Tuning = m_Tuning;
		m_Input.m_Direction = 1;
		m_Input.m_TargetX = 100;
		m_Intent.m_Pos = m_Core.m_Pos;
		m_Intent.m_Direction = 1;
		m_Intent.m_Aim = vec2(100, 0);
		m_Predictor.BeginCore(m_Core, m_Collision, m_Teams);
	}
	const PrismRoute::SRoute &Plan(PrismRoute::CIntentPlanner &Planner, int Tick = 100)
	{
		// Deterministic no-timeout clock; production uses wall-clock deadline.
		return Planner.Plan(m_Predictor, m_Input, m_Intent, Tick, 100, 4000, nullptr, 0, [] { return int64_t(0); });
	}
	void World(CGameWorld &World, CMapBugs &Bugs, CTuningParams *pTunes)
	{
		World.Init(&m_Collision, pTunes, &Bugs);
		World.m_WorldConfig = {};
		World.m_WorldConfig.m_IsDDRace = true;
		World.m_WorldConfig.m_InfiniteAmmo = true;
		World.m_WorldConfig.m_PredictWeapons = World.m_WorldConfig.m_PredictDDRace = World.m_WorldConfig.m_PredictTiles = true;
		World.m_WorldConfig.m_PredictFreeze = 1;
		World.m_GameTick = 100;
		World.m_LocalClientId = 0;
	}
	CCharacter *Character(CGameWorld &World, int Id, vec2 Pos, int Weapon = WEAPON_LASER)
	{
		CNetObj_Character Net{};
		Net.m_X = Pos.x;
		Net.m_Y = Pos.y;
		Net.m_Weapon = Weapon;
		Net.m_HookState = HOOK_IDLE;
		Net.m_AmmoCount = -1;
		auto *pCharacter = new CCharacter(&World, Id, &Net);
		World.InsertEntity(pCharacter);
		pCharacter->GiveAllWeapons();
		pCharacter->SetActiveWeapon(Weapon);
		pCharacter->SetWeaponAmmo(Weapon, -1);
		auto Core = pCharacter->GetCore();
		Core.m_Pos = Pos;
		Core.m_Vel = vec2(0, 0);
		Core.m_Tuning = m_Tuning;
		pCharacter->SetCore(Core);
		pCharacter->m_IsLocal = Id == 0;
		World.m_Teams.Team(Id, 0);
		return pCharacter;
	}
};
TEST_F(CPrismPhysics, SafeStraightRouteNoCorrectionAndCacheHit)
{
	Init();
	PrismRoute::CIntentPlanner Planner;
	const auto &Route = Plan(Planner);
	ASSERT_TRUE(Route.Valid());
	Planner.Recovery(false);
	EXPECT_EQ(Route.m_Mode, PrismRoute::EMode::CONTINUE);
	EXPECT_GT(Route.m_Waypoint.x, m_Core.m_Pos.x + 500);
	EXPECT_EQ(Planner.Stats().m_Candidates, 1);
	Plan(Planner, 101);
	EXPECT_EQ(Planner.Stats().m_CacheHits, 1);
	PrismAssist::STrajectory Path;
	const PrismAssist::SAction Action{1, false, 24};
	ASSERT_TRUE(m_Predictor.Simulate(m_Input, &Action, 1, 24, Path));
	EXPECT_TRUE(Path.Safe());
}
TEST_F(CPrismPhysics, LowClearanceFreezeCorridor)
{
	for(int X = 1; X < CMemoryMap::WIDTH - 1; ++X)
		m_Map.Set(X, 8, TILE_FREEZE, true);
	Init();
	PrismRoute::CIntentPlanner Planner;
	ASSERT_TRUE(Plan(Planner).Valid());
	EXPECT_EQ(Planner.Stats().m_Candidates, 1);
}
TEST_F(CPrismPhysics, OneTileHighCorridorAndOneTileOpening)
{
	for(int X = 1; X < CMemoryMap::WIDTH - 1; ++X)
		m_Map.Set(X, 8, TILE_SOLID);
	for(int Y = 1; Y < 10; ++Y)
		if(Y != 9)
			m_Map.Set(12, Y, TILE_SOLID);
	Init();
	PrismRoute::CIntentPlanner Planner;
	ASSERT_TRUE(Plan(Planner).Valid());
	EXPECT_GT(Planner.Route().m_Waypoint.x, 12 * 32);
}
TEST_F(CPrismPhysics, DangerousManualHasPhysicallyReachableJumpContinuation)
{
	m_Map.Set(8, 9, TILE_FREEZE, true);
	Init();
	PrismAssist::STrajectory Manual;
	const PrismAssist::SAction Action{1, false, 24};
	ASSERT_TRUE(m_Predictor.Simulate(m_Input, &Action, 1, 24, Manual));
	EXPECT_GE(Manual.m_FirstHazard, 0);
	PrismRoute::CIntentPlanner Planner;
	ASSERT_TRUE(Plan(Planner).Valid());
	EXPECT_GT(Planner.Route().m_Waypoint.x, 9 * 32);
	EXPECT_GT(Planner.Stats().m_Candidates, 1);
	PrismAssist::STrajectory Paths[2];
	Paths[0] = Manual;
	const PrismAssist::SAction Jump{1, true, 24};
	m_Predictor.Simulate(m_Input, &Jump, 1, 24, Paths[1]);
	Paths[1].m_Direction = 1;
	Paths[1].m_Jump = true;
	EXPECT_TRUE(PrismRoute::SelectCorrection(Paths, 2, 1, false, false, 1, Planner.Route(), 0).m_Apply);
}
TEST_F(CPrismPhysics, ImpossibleRouteFailsWithoutInventingReachability)
{
	for(int X = 0; X < CMemoryMap::WIDTH; ++X)
		for(int Y = 0; Y < 10; ++Y)
			m_Map.Set(X, Y, TILE_FREEZE, true);
	Init();
	PrismRoute::CIntentPlanner Planner;
	EXPECT_FALSE(Plan(Planner).Valid());
	EXPECT_LE(Planner.Stats().m_Candidates, PrismRoute::MAX_SEARCH);
}
TEST_F(CPrismPhysics, DeadlineRejectsPartialResultAndFallsBack)
{
	Init();
	PrismRoute::CIntentPlanner Planner;
	int64_t Clock = 0;
	const auto &Route = Planner.Plan(m_Predictor, m_Input, m_Intent, 100, 100, 250, nullptr, 0, [&] { Clock += 300; return Clock; });
	EXPECT_FALSE(Route.Valid());
	EXPECT_EQ(Route.m_Mode, PrismRoute::EMode::BUDGET);
	EXPECT_EQ(Planner.Stats().m_Timeouts, 1);
}
TEST_F(CPrismPhysics, EmergencyKeepsDestinationAndRejoins)
{
	Init();
	PrismRoute::CIntentPlanner Planner;
	const vec2 Waypoint = Plan(Planner).m_Waypoint;
	Planner.Recovery(true);
	m_Intent.m_Pos += vec2(0, -15);
	Plan(Planner, 106);
	EXPECT_EQ(Planner.Route().m_Waypoint, Waypoint);
	EXPECT_EQ(Planner.Route().m_Mode, PrismRoute::EMode::REJOIN);
}
TEST_F(CPrismPhysics, ProjectileCollisionAndGrenadeArc)
{
	for(int Y = 0; Y < 10; ++Y)
		m_Map.Set(10, Y, TILE_SOLID);
	Init();
	PrismWeapon::SProfile Profile;
	Profile.m_Range = 800;
	EXPECT_FALSE(PrismWeapon::Solve(WEAPON_GUN, Profile, m_Tuning, true, m_Collision, vec2(200, 260), vec2(500, 260), vec2(0, 0)).m_Valid);
	const auto Arc = PrismWeapon::Intercept(vec2(0, 0), vec2(300, 0), vec2(0, 0), m_Tuning.m_GrenadeSpeed, m_Tuning.m_GrenadeCurvature, m_Tuning.m_GrenadeLifetime);
	ASSERT_TRUE(Arc.m_Valid);
	EXPECT_LT(Arc.m_Aim.y, 0);
}
TEST_F(CPrismPhysics, DetachedCloneDoesNotChangeLivePredictionLinks)
{
	Init();
	CGameWorld World, Child, Trial;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	auto *pLocal = Character(World, 0, vec2(176, 305));
	Child.CopyWorld(&World);
	auto *pChild = pLocal->m_pChild;
	Trial.CopyWorld(&World, true);
	EXPECT_EQ(World.m_pChild, &Child);
	EXPECT_EQ(pLocal->m_pChild, pChild);
	EXPECT_EQ(Trial.m_pParent, nullptr);
	EXPECT_TRUE(Child.m_IsValidCopy);
}
TEST_F(CPrismPhysics, FrozenShooterAndInvalidWeaponCannotAutoFire)
{
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	auto *pLocal = Character(World, 0, vec2(176, 305));
	auto *pTarget = Character(World, 1, vec2(250, 305));
	pLocal->m_FreezeTime = pTarget->m_FreezeTime = 100;
	PrismWeapon::CShotPredictor Predictor;
	EXPECT_FALSE(Predictor.VerifyUnfreeze(World, 0, 1, m_Input, vec2(100, 0)));
	pLocal->m_FreezeTime = 0;
	pLocal->SetActiveWeapon(WEAPON_GRENADE);
	EXPECT_FALSE(Predictor.VerifyUnfreeze(World, 0, 1, m_Input, vec2(100, 0)));
}
TEST_F(CPrismPhysics, ConfirmedOtherPlayerLaserAndHammerUnfreeze)
{
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	auto *pLocal = Character(World, 0, vec2(176, 305));
	auto *pTarget = Character(World, 1, vec2(250, 305));
	pTarget->m_FreezeTime = 100;
	PrismWeapon::CShotPredictor Predictor;
	m_Input.m_Direction = 0;
	EXPECT_TRUE(Predictor.VerifyUnfreeze(World, 0, 1, m_Input, vec2(100, 0), 100000));
	EXPECT_EQ(pTarget->m_FreezeTime, 100);
	EXPECT_EQ(World.m_GameTick, 100);
	pLocal->SetActiveWeapon(WEAPON_HAMMER);
	auto Core = pTarget->GetCore();
	Core.m_Pos.x = 220;
	pTarget->SetCore(Core);
	pTarget->m_Pos = Core.m_Pos;
	EXPECT_TRUE(Predictor.VerifyUnfreeze(World, 0, 1, m_Input, vec2(100, 0), 100000));
}
TEST_F(CPrismPhysics, RejectBlockedRescueAndNoEffectShot)
{
	for(int Y = 0; Y < 10; ++Y)
		m_Map.Set(7, Y, TILE_SOLID);
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	Character(World, 0, vec2(176, 305));
	auto *pTarget = Character(World, 1, vec2(280, 305));
	pTarget->m_FreezeTime = 100;
	PrismWeapon::CShotPredictor Predictor;
	m_Input.m_Direction = 0;
	EXPECT_FALSE(Predictor.VerifyUnfreeze(World, 0, 1, m_Input, vec2(100, 0), 100000));
	EXPECT_FALSE(Predictor.VerifyUnfreeze(World, 0, 0, m_Input, vec2(100, 0), 100000));
}
TEST_F(CPrismPhysics, ConfirmedPreemptiveSelfLaserBounceUnfreeze)
{
	for(int Y = 0; Y < 10; ++Y)
		m_Map.Set(14, Y, TILE_SOLID);
	m_Map.Set(8, 6, TILE_FREEZE, true);
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	for(auto &Tune : aTunes)
		Tune.m_Gravity = 0;
	auto *pLocal = Character(World, 0, vec2(235, 200));
	auto Core = pLocal->GetCore();
	Core.m_Vel = vec2(12, 0);
	Core.m_Tuning.m_Gravity = 0;
	pLocal->SetCore(Core);
	m_Input.m_Direction = 1;
	PrismWeapon::CShotPredictor Predictor;
	std::array<vec2, 4> aAims{};
	const int Count = PrismWeapon::BounceAims(m_Collision, vec2(235, 200), vec2(265, 200), vec2(100, 0), m_Tuning.m_LaserReach, pi / 4, aAims.data(), aAims.size());
	ASSERT_GT(Count, 0);
	bool Confirmed = false;
	for(int i = 0; i < Count; ++i)
		Confirmed |= Predictor.VerifyUnfreeze(World, 0, 0, m_Input, aAims[i], 100000);
	EXPECT_TRUE(Confirmed);
	EXPECT_EQ(pLocal->m_FreezeTime, 0);
	EXPECT_EQ(pLocal->Core()->m_Pos, vec2(235, 200));
}
TEST_F(CPrismPhysics, CoreSimulationNeverMutatesOtherPlayerState)
{
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	Character(World, 0, vec2(176, 305));
	auto *pOther = Character(World, 1, vec2(195, 305));
	const auto Before = pOther->GetCore();
	ASSERT_TRUE(m_Predictor.Begin(World, 0));
	PrismAssist::SAction Action{1, false, 24, true, vec2(100, 0)};
	PrismAssist::STrajectory Path;
	m_Predictor.Simulate(m_Input, &Action, 1, 24, Path);
	EXPECT_EQ(pOther->Core()->m_Pos, Before.m_Pos);
	EXPECT_EQ(pOther->Core()->m_Vel, Before.m_Vel);
}
TEST_F(CPrismPhysics, PlannerPerformanceBoundedSearchAndCache)
{
	Init();
	PrismRoute::CIntentPlanner Planner;
	for(int Tick = 0; Tick < 1000; ++Tick)
		Planner.Plan(m_Predictor, m_Input, m_Intent, Tick, 100, 4000, nullptr, 0);
	EXPECT_GT(Planner.Stats().m_Plans, 0);
	EXPECT_GT(Planner.Stats().m_CacheHits, 0);
	EXPECT_LE(Planner.Stats().m_Candidates, PrismRoute::MAX_SEARCH);
	std::printf("PRISM_BENCH plans=%llu cache=%llu avg_us=%.3f worst_us=%.3f candidates=%d ticks=%d timeouts=%llu\n",
		static_cast<unsigned long long>(Planner.Stats().m_Plans), static_cast<unsigned long long>(Planner.Stats().m_CacheHits),
		Planner.Stats().AverageUs(), Planner.Stats().m_WorstUs, Planner.Stats().m_Candidates, Planner.Stats().m_SimulatedTicks,
		static_cast<unsigned long long>(Planner.Stats().m_Timeouts));
}
TEST_F(CPrismPhysics, TickInputSequenceReplayIsDeterministic)
{
	Init();
	std::array<CNetObj_PlayerInput, 50> aInputs{};
	for(int Tick = 0; Tick < 50; ++Tick)
	{
		aInputs[Tick].m_Direction = Tick < 20 ? 1 : -1;
		aInputs[Tick].m_TargetX = 100;
		aInputs[Tick].m_Jump = Tick >= 8 && Tick < 12;
	}
	PrismAssist::STrajectory First, Second;
	ASSERT_TRUE(m_Predictor.SimulateInputs(aInputs.data(), aInputs.size(), First));
	ASSERT_TRUE(m_Predictor.SimulateInputs(aInputs.data(), aInputs.size(), Second));
	ASSERT_EQ(First.m_Count, 51);
	ASSERT_EQ(First.m_Count, Second.m_Count);
	for(int Tick = 0; Tick < First.m_Count; ++Tick)
	{
		EXPECT_EQ(First.m_aStates[Tick].m_Pos, Second.m_aStates[Tick].m_Pos);
		EXPECT_EQ(First.m_aStates[Tick].m_Vel, Second.m_aStates[Tick].m_Vel);
	}
	EXPECT_FALSE(m_Predictor.SimulateInputs(nullptr, 50, Second));
}
TEST_F(CPrismPhysics, PlannerPerformanceRecoverySearch)
{
	m_Map.Set(8, 9, TILE_FREEZE, true);
	Init();
	PrismRoute::CIntentPlanner Planner;
	for(int Tick = 0; Tick < 1000; ++Tick)
		Planner.Plan(m_Predictor, m_Input, m_Intent, Tick, 100, 1500, nullptr, 0);
	EXPECT_LE(Planner.Stats().m_Candidates, PrismRoute::MAX_SEARCH);
	EXPECT_LE(Planner.Stats().m_SimulatedTicks, PrismRoute::MAX_SIMULATED_TICKS);
	std::printf("PRISM_BENCH_RECOVERY plans=%llu cache=%llu avg_us=%.3f worst_us=%.3f candidates=%d ticks=%d timeouts=%llu\n",
		static_cast<unsigned long long>(Planner.Stats().m_Plans), static_cast<unsigned long long>(Planner.Stats().m_CacheHits),
		Planner.Stats().AverageUs(), Planner.Stats().m_WorstUs, Planner.Stats().m_Candidates, Planner.Stats().m_SimulatedTicks,
		static_cast<unsigned long long>(Planner.Stats().m_Timeouts));
}
TEST_F(CPrismPhysics, ActualLaserSolverRejectsFirstOtherHitAndDisabledHits)
{
	Init();
	CGameWorld World;
	CMapBugs Bugs;
	CTuningParams aTunes[256];
	this->World(World, Bugs, aTunes);
	auto *pLocal = Character(World, 0, vec2(176, 305));
	Character(World, 1, vec2(250, 305));
	Character(World, 2, vec2(350, 305));
	PrismWeapon::CShotPredictor Predictor;
	m_Input.m_Direction = 0;
	EXPECT_TRUE(Predictor.VerifyLaser(World, 0, 1, m_Input, vec2(100, 0), 100000));
	EXPECT_FALSE(Predictor.VerifyLaser(World, 0, 2, m_Input, vec2(100, 0), 100000));
	auto Core = pLocal->GetCore();
	Core.m_LaserHitDisabled = true;
	pLocal->SetCore(Core);
	EXPECT_FALSE(Predictor.VerifyLaser(World, 0, 1, m_Input, vec2(100, 0), 100000));
}
int main(int argc, char **argv)
{
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) g_Config.m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) g_Config.m_##Name = Def;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) std::strncpy(g_Config.m_##Name, Def, Len);
#include <engine/shared/config_variables.h>
#undef MACRO_CONFIG_INT
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_STR
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
