// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PATHFINDER_TYPES_H
#define GAME_CLIENT_PATHFINDER_TYPES_H

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace PrismPath
{
constexpr int MAX_HORIZON = 200;
constexpr int MAX_ACTION_TICKS = 12;
constexpr int MAX_ANCHORS = 6;
constexpr int MAX_NODES = 2048;
constexpr float UNREACHABLE = 1e20f;

enum class EStatus { IDLE, ANALYZING, PLANNING, EXECUTING, RECOVERING, REPLANNING, PAUSED, NO_ROUTE, FINISHED };
enum class EReject { NONE, DEATH, FREEZE, TELEPORT, DYNAMIC, BOUNDS, TIMEOUT, MISSING_CHARACTER };
const char *StatusName(EStatus Status);
const char *RejectName(EReject Reject);

struct SSettings
{
	int m_BudgetUs = 2000;
	int m_Horizon = 100;
	float m_ReplanDistance = 20;
	bool m_Autopilot = false;
	bool m_SafeRoutes = true;
	bool m_Momentum = true;
	bool m_ManualOverride = true;
};

struct SState
{
	vec2 m_Pos{}, m_Vel{}, m_HookPos{}, m_HookDir{}, m_HookTeleBase{};
	int m_Tick = 0, m_Jumped = 0, m_JumpedTotal = 0, m_Jumps = 2;
	int m_Direction = 0, m_HookState = HOOK_IDLE, m_HookTick = 0, m_HookedPlayer = -1;
	int m_FreezeTime = 0, m_Weapon = 0, m_TeleCheckpoint = 0, m_TuneZone = 0;
	int m_MoveRestrictions = 0;
	bool m_Grounded = false, m_InFreeze = false, m_DeepFrozen = false, m_LiveFrozen = false;
	unsigned m_Abilities = 0;
	uint64_t m_SwitchHash = 0;
	static SState Read(CGameWorld &World, int Id);
};

// Equality includes momentum, input edges, hook flight and remaining jumps.
// Quantization is for dominance only; simulation always keeps exact DDNet worlds.
struct SStateKey
{
	std::array<int, 27> m_aValues{};
	uint64_t m_SwitchHash = 0;
	bool operator==(const SStateKey &Other) const { return m_aValues == Other.m_aValues && m_SwitchHash == Other.m_SwitchHash; }
	static SStateKey From(const SState &State, bool JumpHeld, bool HookHeld);
};
struct SStateHash
{
	size_t operator()(const SStateKey &Key) const;
};

struct SAction
{
	int m_Direction = 0, m_Ticks = 8;
	bool m_Jump = false, m_Hook = false;
	vec2 m_Anchor{};
	// Fixed aim is essential for a flying hook; later attachment does not retarget it.
	vec2 m_Aim = vec2(1, 0);
	CNetObj_PlayerInput Input(int FireCounter = 0) const;
};

struct STrace
{
	std::array<SState, MAX_ACTION_TICKS + 1> m_aStates{};
	int m_Count = 0;
	EReject m_Reject = EReject::NONE;
	bool Safe() const { return m_Count > 1 && m_Reject == EReject::NONE; }
};
struct SPlan
{
	std::array<SState, MAX_HORIZON + 1> m_aStates{};
	std::array<CNetObj_PlayerInput, MAX_HORIZON> m_aInputs{};
	int m_Ticks = 0;
	vec2 m_HookPoint{}, m_Landing{};
	bool m_UsesHook = false, m_HasLanding = false;
	float m_Score = -UNREACHABLE;
	bool Valid() const { return m_Ticks > 0; }
};
struct SStats
{
	uint64_t m_Expanded = 0, m_Pruned = 0, m_Simulations = 0, m_Replans = 0, m_Plans = 0;
	uint64_t m_Timeouts = 0, m_Dead = 0, m_Frozen = 0, m_Unsupported = 0;
	double m_LastUs = 0, m_WorstUs = 0;
};

bool Diverged(const SState &Expected, const SState &Observed, float PositionThreshold);
void ReleaseInput(CNetObj_PlayerInput &Input);
}
#endif
