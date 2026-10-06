#include "simulator.h"

#include <base/time.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>

namespace PrismPath
{
int64_t NowUs() { return time_get_nanoseconds().count() / 1000; }
const char *CSimulator::UnsupportedWorld(CGameWorld &World, int Id)
{
	if(!World.Collision() || !World.GetCharacterById(Id)) return "missing prediction world";
	if(!World.m_WorldConfig.m_IsDDRace || !World.m_WorldConfig.m_PredictDDRace || !World.m_WorldConfig.m_PredictTiles ||
		!World.m_WorldConfig.m_PredictFreeze) return "enable DDNet tile and freeze prediction";
	if(World.GetCharacterById(Id)->Core()->m_Super || World.GetCharacterById(Id)->Core()->m_Invincible)
		return "super/invincible physics are outside solo traversal";
	int Count = 0;
	for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; ++Type)
		for(auto *pEntity = World.FindFirst(Type); pEntity; pEntity = pEntity->TypeNext())
		{
			// Door copies write into shared collision; unknown server obstacles cannot be predicted safely.
			if(Type != CGameWorld::ENTTYPE_CHARACTER && Type != CGameWorld::ENTTYPE_PICKUP)
				return "dynamic entities require isolated collision prediction";
			if(++Count > 32) return "prediction entity limit";
		}
	return nullptr;
}
void CSimulator::Clone(CGameWorld &To, CGameWorld &From)
{
	To.CopyWorld(&From, true);
	To.m_LocalClientId = From.m_LocalClientId;
	To.m_WorldConfig.m_PredictEvents = false;
	To.m_PredictedEvents.clear();
	To.m_Core.m_pPrng = nullptr;
}
EReject CSimulator::CheckPosition(const CCollision &Collision, vec2 P)
{
	if(!std::isfinite(P.x) || !std::isfinite(P.y) || P.x < 0 || P.y < 0 ||
		P.x >= Collision.GetWidth() * 32 || P.y >= Collision.GetHeight() * 32) return EReject::BOUNDS;
	const int Index = Collision.GetPureMapIndex(P);
	if(Collision.IsTeleport(Index) || Collision.IsEvilTeleport(Index) || Collision.IsCheckTeleport(Index) ||
		Collision.IsCheckEvilTeleport(Index) || Collision.IsTeleportHook(Index)) return EReject::TELEPORT;
	// Freeze is applied at the center. Neighboring freeze is allowed, including 1x1 corridors.
	for(int Tile : {Collision.GetTileIndex(Index), Collision.GetFrontTileIndex(Index)})
	{
		if(Tile == TILE_DEATH) return EReject::DEATH;
		if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE) return EReject::FREEZE;
	}
	const float R = CCharacterCore::PhysicalSize() / 3;
	for(float X : {-R, R})
		for(float Y : {-R, R})
			if(Collision.GetCollisionAt(P.x + X, P.y + Y) == TILE_DEATH ||
				Collision.GetFrontCollisionAt(P.x + X, P.y + Y) == TILE_DEATH) return EReject::DEATH;
	return EReject::NONE;
}
EReject CSimulator::CheckSegment(const CCollision &Collision, vec2 From, vec2 To)
{
	const int Samples = std::max(1, static_cast<int>(std::ceil(distance(From, To) / 4)));
	if(Samples > 256) return EReject::DYNAMIC;
	for(int i = 1; i <= Samples; ++i)
	{
		const EReject Reject = CheckPosition(Collision, mix(From, To, i / static_cast<float>(Samples)));
		if(Reject != EReject::NONE) return Reject;
	}
	return EReject::NONE;
}
bool CSimulator::Step(CGameWorld &World, CNetObj_PlayerInput Input, SState &State, EReject &Reject)
{
	auto *pChar = World.GetCharacterById(m_Id);
	if(!pChar) { Reject = EReject::MISSING_CHARACTER; return false; }
	const vec2 Previous = pChar->Core()->m_Pos;
	Input.m_Fire = (m_FireCounter + (m_FireCounter & 1)) & INPUT_STATE_MASK;
	Input.m_WantedWeapon = Input.m_NextWeapon = Input.m_PrevWeapon = 0;
	++World.m_GameTick;
	pChar->OnDirectInput(&Input);
	pChar->OnPredictedInput(&Input);
	World.Tick();
	if(!World.GetCharacterById(m_Id)) { Reject = EReject::MISSING_CHARACTER; return false; }
	State = SState::Read(World, m_Id);
	Reject = CheckSegment(*World.Collision(), Previous, State.m_Pos);
	if(Reject == EReject::NONE && (State.m_FreezeTime || State.m_DeepFrozen || State.m_LiveFrozen)) Reject = EReject::FREEZE;
	if(Reject == EReject::NONE && State.m_HookedPlayer >= 0) Reject = EReject::DYNAMIC;
	return Reject == EReject::NONE;
}
bool CSimulator::Simulate(CGameWorld &From, const SAction &Action, STrace &Trace, int64_t Deadline)
{
	Trace = {};
	Clone(m_Work, From);
	Trace.m_aStates[Trace.m_Count++] = SState::Read(m_Work, m_Id);
	for(int i = 0; i < std::clamp(Action.m_Ticks, 1, MAX_ACTION_TICKS); ++i)
	{
		if(Deadline && NowUs() >= Deadline) { Trace.m_Reject = EReject::TIMEOUT; return false; }
		SState State;
		const bool Safe = Step(m_Work, Action.Input(m_FireCounter), State, Trace.m_Reject);
		Trace.m_aStates[Trace.m_Count++] = State;
		if(!Safe) return false;
	}
	return Trace.Safe();
}
}
