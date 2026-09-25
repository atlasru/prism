// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_ASSIST_H
#define GAME_CLIENT_PRISM_ASSIST_H

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace PrismAssist
{
constexpr int MAX_TICKS = 24;
constexpr int MAX_CANDIDATES = 7;
constexpr int MAX_PHASES = 3;
inline int ClampHorizon(int Horizon) { return std::clamp(Horizon, 1, MAX_TICKS); }
inline bool HazardTile(int Tile) { return Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE || Tile == TILE_DEATH; }
inline bool HazardLayers(int Game, int Front) { return HazardTile(Game) || HazardTile(Front); }
// GetCollisionAt returns only solid/death tiles. Freeze lives in the raw
// game/front layers and must be sampled by index instead.
inline bool HazardAt(const CCollision &Collision, vec2 Pos)
{
	const int Index = Collision.GetPureMapIndex(Pos);
	return HazardLayers(Collision.GetTileIndex(Index), Collision.GetFrontTileIndex(Index));
}
inline bool WithinFov(vec2 Direction, vec2 Target, float FovRadians)
{
	if(length(Direction) < 0.001f || length(Target) < 0.001f)
		return false;
	const float Delta = std::atan2(std::sin(std::atan2(Target.y, Target.x) - std::atan2(Direction.y, Direction.x)),
		std::cos(std::atan2(Target.y, Target.x) - std::atan2(Direction.y, Direction.x)));
	return std::abs(Delta) <= FovRadians / 2.0f;
}
inline bool EligibleTarget(vec2 Aim, vec2 Delta, float FovRadians, float MaxRange, bool Visible)
{
	return Visible && length(Delta) >= 1.0f && length(Delta) <= MaxRange && WithinFov(Aim, Delta, FovRadians);
}

struct SState
{
	vec2 m_Pos = vec2(0, 0);
	vec2 m_Vel = vec2(0, 0);
	int m_Tick = 0;
	int m_Jumped = 0;
	int m_HookState = HOOK_IDLE;
	bool m_Hazard = false;
	bool m_Unknown = false;
};

struct SAction
{
	int m_Direction = 0;
	bool m_Jump = false;
	int m_Ticks = 1;
};

struct STrajectory
{
	std::array<SState, MAX_TICKS + 1> m_aStates{};
	int m_Count = 0;
	int m_FirstHazard = -1;
	bool m_Unknown = false;
	float m_Score = 0;
	float m_MinSafetyMargin = 16.0f;
	int m_Direction = 0;
	bool m_Jump = false;
	bool Safe() const { return m_Count > 0 && m_FirstHazard == -1 && !m_Unknown; }
};

inline const char *DebugStatus(const STrajectory &Path)
{
	if(!Path.m_Count)
		return "NO PREDICTION";
	if(Path.m_Unknown)
		return "UNKNOWN";
	return Path.m_FirstHazard >= 0 ? "DANGER" : "SAFE";
}

inline float ScoreTrajectory(const STrajectory &Path, vec2 DesiredPos = vec2(0, 0), float DesiredWeight = 0.0f)
{
	if(Path.m_Unknown)
		return -10000.0f;
	if(Path.m_FirstHazard >= 0)
		return -1000.0f + Path.m_FirstHazard;
	if(!Path.m_Count)
		return -10000.0f;
	const SState &Final = Path.m_aStates[Path.m_Count - 1];
	return 100.0f + Path.m_MinSafetyMargin - length(Final.m_Vel) * 0.1f - distance(Final.m_Pos, DesiredPos) * DesiredWeight;
}

// Uses DDNet's character core, collision and tuning. Full game-world tile
// effects (switches, speedups, teleport outcomes) cannot be simulated by the
// core alone; an unknown tile invalidates the candidate for intervention.
class CPredictor
{
	CWorldCore m_World;
	CCharacterCore m_Initial;
	CCollision *m_pCollision = nullptr;
	CTeamsCore *m_pTeams = nullptr;
	int m_LocalId = -1;
	bool m_Ready = false;

	void Sample(vec2 Pos, bool &Hazard, bool &Unknown) const
	{
		const float Radius = CCharacterCore::PhysicalSize() / 3.0f;
		for(float X : {-Radius, Radius})
			for(float Y : {-Radius, Radius})
			{
				const vec2 P = Pos + vec2(X, Y);
				Hazard |= HazardAt(*m_pCollision, P);
				const int Index = m_pCollision->GetPureMapIndex(P);
				if(Index >= 0)
					Unknown |= m_pCollision->IsTeleport(Index) || m_pCollision->IsEvilTeleport(Index) || m_pCollision->IsSpeedup(Index) || m_pCollision->IsTune(Index);
			}
	}
	bool NearHazard(vec2 Pos) const
	{
		const float Radius = CCharacterCore::PhysicalSize() / 3.0f + 8.0f;
		for(float X : {-Radius, Radius})
			for(float Y : {-Radius, Radius})
				if(HazardAt(*m_pCollision, Pos + vec2(X, Y)))
					return true;
		return false;
	}

public:
	bool Begin(CGameWorld &Source, int LocalId)
	{
		m_Ready = false;
		CCharacter *pLocal = Source.GetCharacterById(LocalId);
		if(!pLocal || !Source.Collision())
			return false;
		m_pCollision = Source.Collision();
		m_pTeams = Source.Teams();
		m_LocalId = LocalId;
		m_Initial = pLocal->GetCore();
		m_World.m_vSwitchers = Source.m_Core.m_vSwitchers;
		m_World.m_pPrng = nullptr;
		std::copy(std::begin(Source.m_Core.m_apCharacters), std::end(Source.m_Core.m_apCharacters), std::begin(m_World.m_apCharacters));
		m_Ready = true;
		return true;
	}

	bool Simulate(const CNetObj_PlayerInput &Input, const SAction *pActions, int NumActions, int Horizon, STrajectory &Out)
	{
		Out = {};
		if(!m_Ready || !pActions || NumActions < 1 || NumActions > MAX_PHASES)
			return false;
		Horizon = ClampHorizon(Horizon);
		CCharacterCore Core = m_Initial;
		Core.SetCoreWorld(&m_World, m_pCollision, m_pTeams);
		m_World.m_apCharacters[m_LocalId] = &Core;
		Out.m_Count = 1;
		Out.m_aStates[0].m_Pos = Core.m_Pos;
		Out.m_aStates[0].m_Vel = Core.m_Vel;
		int Phase = 0;
		int Remaining = std::max(1, pActions[0].m_Ticks);
		for(int Tick = 1; Tick <= Horizon; ++Tick)
		{
			if(Remaining-- == 0 && Phase + 1 < NumActions)
			{
				++Phase;
				Remaining = std::max(1, pActions[Phase].m_Ticks) - 1;
			}
			Core.m_Input = Input;
			Core.m_Input.m_Direction = std::clamp(pActions[Phase].m_Direction, -1, 1);
			Core.m_Input.m_Jump = pActions[Phase].m_Jump && (Tick == 1 || Phase > 0 && Remaining == std::max(1, pActions[Phase].m_Ticks) - 1);
			const vec2 Previous = Core.m_Pos;
			Core.Tick(true);
			Core.Move();
			Core.Quantize();
			SState &State = Out.m_aStates[Tick];
			State.m_Pos = Core.m_Pos;
			State.m_Vel = Core.m_Vel;
			State.m_Tick = Tick;
			State.m_Jumped = Core.m_Jumped;
			State.m_HookState = Core.m_HookState;
			for(int Step = 0; Step <= 4; ++Step)
				Sample(mix(Previous, Core.m_Pos, Step / 4.0f), State.m_Hazard, State.m_Unknown);
			Out.m_Count = Tick + 1;
			if(NearHazard(State.m_Pos))
				Out.m_MinSafetyMargin = 8.0f;
			Out.m_Unknown |= State.m_Unknown;
			if(State.m_Hazard && Out.m_FirstHazard == -1)
				Out.m_FirstHazard = Tick;
			if(State.m_Hazard || State.m_Unknown)
				break;
		}
		Out.m_Score = ScoreTrajectory(Out);
		return true;
	}
};

struct SCorrection
{
	bool m_Apply = false;
	int m_Direction = 0;
	bool m_Jump = false;
	int m_Selected = 0;
};

inline SCorrection SelectCorrection(const STrajectory *pPaths, int Count, int ManualDirection, bool ManualJump, int Mode = 2, bool MacroDirectionOwned = false)
{
	SCorrection Result;
	if(Mode != 2 || !pPaths || Count < 2 || pPaths[0].Safe() || pPaths[0].m_Unknown)
		return Result;
	int BestCost = std::numeric_limits<int>::max();
	for(int i = 1; i < std::min(Count, MAX_CANDIDATES); ++i)
	{
		const auto &Path = pPaths[i];
		if(!Path.Safe() || ManualJump && !Path.m_Jump || MacroDirectionOwned && Path.m_Direction != pPaths[0].m_Direction)
			continue;
		// A held direction may be overridden only for a safe outcome. Prefer
		// preserving it, then a short jump, then braking, then reversal.
		const int Cost = (Path.m_Direction == pPaths[0].m_Direction ? 0 :
			Path.m_Direction == 0 ? 2 : Path.m_Direction == -ManualDirection ? 4 : 3) +
			(Path.m_Jump != pPaths[0].m_Jump ? 1 : 0);
		if(Cost < BestCost || Cost == BestCost && (Result.m_Selected == 0 || Path.m_Score > pPaths[Result.m_Selected].m_Score))
		{
			BestCost = Cost;
			Result = {true, Path.m_Direction, Path.m_Jump, i};
		}
	}
	return Result;
}

struct STargetCandidate
{
	int m_Id = -1;
	float m_Score = 0;
	float m_Angle = 0;
};

inline int SelectTarget(const STargetCandidate *pCandidates, int Count, int PreviousId, float Retention = 0.82f)
{
	int Best = -1, Previous = -1;
	for(int i = 0; i < Count; ++i)
	{
		if(Best == -1 || pCandidates[i].m_Score < pCandidates[Best].m_Score)
			Best = i;
		if(pCandidates[i].m_Id == PreviousId)
			Previous = i;
	}
	if(Previous >= 0 && Best >= 0 && pCandidates[Best].m_Score >= pCandidates[Previous].m_Score * Retention)
		return Previous;
	return Best;
}

inline vec2 InterpolateAim(vec2 Current, vec2 Desired, float Strength, float MaxAngleRadians)
{
	if(length(Current) < 0.001f || length(Desired) < 0.001f)
		return Current;
	const float A = std::atan2(Current.y, Current.x);
	const float B = std::atan2(Desired.y, Desired.x);
	const float Delta = std::atan2(std::sin(B - A), std::cos(B - A));
	const float Step = std::clamp(Delta * std::clamp(Strength, 0.0f, 1.0f), -MaxAngleRadians, MaxAngleRadians);
	const float Magnitude = length(Current) + (length(Desired) - length(Current)) * std::clamp(Strength, 0.0f, 1.0f);
	return vec2(std::cos(A + Step), std::sin(A + Step)) * Magnitude;
}
}
#endif
