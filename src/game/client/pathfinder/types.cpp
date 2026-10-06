#include "types.h"

#include <game/collision.h>

#include <algorithm>

namespace PrismPath
{
	const char *StatusName(EStatus Status)
	{
		static const char *s_apNames[] = {"Idle", "Analyzing", "Planning", "Executing", "Recovering", "Replanning", "Paused", "No Route", "Finished"};
		return s_apNames[static_cast<int>(Status)];
	}
	const char *RejectName(EReject Reject)
	{
		static const char *s_apNames[] = {"safe", "death collision", "freeze collision", "unsupported teleport", "unsupported dynamic mechanic", "map boundary", "search budget", "missing character"};
		return s_apNames[static_cast<int>(Reject)];
	}
	SState SState::Read(CGameWorld &World, int Id)
	{
		SState State;
		auto *pChar = World.GetCharacterById(Id);
		if(!pChar)
			return State;
		const auto &Core = *pChar->Core();
		State.m_Pos = Core.m_Pos;
		State.m_Vel = Core.m_Vel;
		State.m_HookPos = Core.m_HookPos;
		State.m_HookDir = Core.m_HookDir;
		State.m_HookTeleBase = Core.m_HookTeleBase;
		State.m_Tick = World.GameTick();
		State.m_Jumped = Core.m_Jumped;
		State.m_JumpedTotal = Core.m_JumpedTotal;
		State.m_Jumps = Core.m_Jumps;
		State.m_Direction = Core.m_Direction;
		State.m_HookState = Core.m_HookState;
		State.m_HookTick = Core.m_HookTick;
		State.m_HookedPlayer = Core.HookedPlayer();
		State.m_FreezeTime = pChar->m_FreezeTime;
		State.m_Weapon = Core.m_ActiveWeapon;
		State.m_TeleCheckpoint = pChar->m_TeleCheckpoint;
		State.m_TuneZone = pChar->GetOverriddenTuneZone();
		State.m_MoveRestrictions = World.Collision()->GetMoveRestrictions(Core.m_Pos);
		State.m_Grounded = pChar->IsGrounded();
		State.m_InFreeze = Core.m_IsInFreeze;
		State.m_DeepFrozen = Core.m_DeepFrozen;
		State.m_LiveFrozen = Core.m_LiveFrozen;
		State.m_Abilities = Core.m_EndlessJump | (Core.m_EndlessHook << 1) | (Core.m_Solo << 2) | (Core.m_CollisionDisabled << 3) |
				    (Core.m_HookHitDisabled << 4) | (Core.m_Jetpack << 5);
		uint64_t Hash = 1469598103934665603ULL;
		for(const auto &Switcher : World.Switchers())
		{
			const int Team = pChar->Team();
			if(Team >= 0 && Team < MAX_CLIENTS)
			{
				Hash = (Hash ^ Switcher.m_aStatus[Team]) * 1099511628211ULL;
				Hash = (Hash ^ static_cast<unsigned>(Switcher.m_aEndTick[Team])) * 1099511628211ULL;
			}
		}
		State.m_SwitchHash = Hash;
		return State;
	}
	SStateKey SStateKey::From(const SState &S, bool JumpHeld, bool HookHeld)
	{
		auto Q = [](float V, float Step) { return static_cast<int>(std::floor(V / Step)); };
		SStateKey Key;
		Key.m_aValues = {Q(S.m_Pos.x, 4), Q(S.m_Pos.y, 4), Q(S.m_Vel.x, 0.5f), Q(S.m_Vel.y, 0.5f),
			S.m_Jumped, S.m_JumpedTotal, S.m_Jumps, S.m_Direction, S.m_HookState,
			Q(S.m_HookPos.x, 4), Q(S.m_HookPos.y, 4), Q(S.m_HookDir.x, 0.05f), Q(S.m_HookDir.y, 0.05f),
			S.m_HookTick / 2, S.m_HookedPlayer, S.m_FreezeTime, S.m_Weapon, S.m_TeleCheckpoint, S.m_TuneZone,
			S.m_MoveRestrictions, int(S.m_Grounded), int(S.m_InFreeze) | (S.m_DeepFrozen << 1) | (S.m_LiveFrozen << 2),
			static_cast<int>(S.m_Abilities), int(JumpHeld) | (HookHeld << 1), Q(S.m_HookTeleBase.x, 4), Q(S.m_HookTeleBase.y, 4),
			// Preserve switch timer/other-entity phase when dynamic interactions exist.
			0};
		Key.m_SwitchHash = S.m_SwitchHash;
		return Key;
	}
	size_t SStateHash::operator()(const SStateKey &Key) const
	{
		uint64_t Hash = Key.m_SwitchHash;
		for(int Value : Key.m_aValues)
			Hash = (Hash ^ static_cast<uint32_t>(Value)) * 1099511628211ULL;
		return static_cast<size_t>(Hash);
	}
	CNetObj_PlayerInput SAction::Input(int FireCounter) const
	{
		CNetObj_PlayerInput Result{};
		Result.m_Direction = m_Direction;
		Result.m_Jump = m_Jump;
		Result.m_Hook = m_Hook;
		Result.m_TargetX = round_to_int(m_Aim.x);
		Result.m_TargetY = round_to_int(m_Aim.y);
		if(!Result.m_TargetX && !Result.m_TargetY)
			Result.m_TargetX = 1;
		Result.m_Fire = (FireCounter + (FireCounter & 1)) & INPUT_STATE_MASK;
		return Result;
	}
	bool Diverged(const SState &A, const SState &B, float Threshold)
	{
		return distance(A.m_Pos, B.m_Pos) > Threshold || distance(A.m_Vel, B.m_Vel) > 4.0f ||
		       A.m_FreezeTime != B.m_FreezeTime || A.m_DeepFrozen != B.m_DeepFrozen || A.m_LiveFrozen != B.m_LiveFrozen ||
		       A.m_JumpedTotal != B.m_JumpedTotal || A.m_TuneZone != B.m_TuneZone || A.m_Abilities != B.m_Abilities ||
		       A.m_HookState != B.m_HookState || (A.m_HookState == HOOK_GRABBED && distance(A.m_HookPos, B.m_HookPos) > 8);
	}
	void ReleaseInput(CNetObj_PlayerInput &Input)
	{
		Input.m_Direction = Input.m_Jump = Input.m_Hook = 0;
		Input.m_Fire = (Input.m_Fire + (Input.m_Fire & 1)) & INPUT_STATE_MASK;
		Input.m_WantedWeapon = Input.m_NextWeapon = Input.m_PrevWeapon = 0;
	}
}
