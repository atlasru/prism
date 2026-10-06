#ifndef TEST_PRISM_FIXTURE_H
#define TEST_PRISM_FIXTURE_H
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

#endif
