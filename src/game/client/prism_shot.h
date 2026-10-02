// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_SHOT_H
#define GAME_CLIENT_PRISM_SHOT_H
#include "prism_weapon.h"

#include <base/time.h>

#include <game/client/prediction/entities/laser.h>

namespace PrismWeapon
{
	// Rare, bounded verification only. Clone game worlds without live parent links.
	// The actual FireWeapon/CLaser/Unfreeze/tile code decides whether rescue works.
	class CShotPredictor
	{
		CGameWorld m_Control, m_Shot;
		static void Prepare(CGameWorld &World, CGameWorld &Source)
		{
			World.CopyWorld(&Source, true);
			World.m_WorldConfig.m_PredictEvents = false;
			World.m_PredictedEvents.clear();
		}
		static void Step(CGameWorld &World, int LocalId, CNetObj_PlayerInput Input)
		{
			++World.m_GameTick;
			Input.m_Fire = (Input.m_Fire + (Input.m_Fire & 1)) & INPUT_STATE_MASK;
			Input.m_WantedWeapon = Input.m_NextWeapon = Input.m_PrevWeapon = 0;
			if(auto *pLocal = World.GetCharacterById(LocalId))
			{
				pLocal->OnDirectInput(&Input);
				pLocal->OnPredictedInput(&Input);
			}
			World.Tick();
		}

	public:
		double m_LastUs = 0;
		bool m_TimedOut = false;
		static bool Supported(CGameWorld &Source)
		{
			if(!Source.m_WorldConfig.m_IsDDRace || !Source.m_WorldConfig.m_PredictWeapons ||
				!Source.m_WorldConfig.m_PredictDDRace || !Source.m_WorldConfig.m_PredictTiles || !Source.m_WorldConfig.m_PredictFreeze)
				return false;
			int Count = 0;
			for(int Type = 0; Type < CGameWorld::NUM_ENTTYPES; ++Type)
				for(auto *pEntity = Source.FindFirst(Type); pEntity; pEntity = pEntity->TypeNext())
					if(++Count > 128 || Type == CGameWorld::ENTTYPE_DOOR || Type == CGameWorld::ENTTYPE_LIGHT || Type == CGameWorld::ENTTYPE_GUN)
						return false;
			return true;
		}
		bool VerifyUnfreeze(CGameWorld &Source, int LocalId, int TargetId, CNetObj_PlayerInput Input, vec2 Aim, int BudgetUs = 2000)
		{
			const int64_t Start = time_get_nanoseconds().count() / 1000;
			m_TimedOut = false;
			auto Finish = [&](bool Value) { m_LastUs = static_cast<double>(time_get_nanoseconds().count() / 1000 - Start); return Value; };
			CCharacter *pLocal = Source.GetCharacterById(LocalId), *pTarget = Source.GetCharacterById(TargetId);
			if(!Supported(Source) || !pLocal || !pTarget || pLocal->m_FreezeTime || pLocal->GetReloadTimer() ||
				pLocal->Core()->m_DeepFrozen || pLocal->Core()->m_LiveFrozen || pTarget->Core()->m_DeepFrozen || pTarget->Core()->m_LiveFrozen ||
				!CanUnfreeze(pLocal->GetActiveWeapon(), LocalId == TargetId, true, Source.m_WorldConfig.m_OldLaser))
				return Finish(false);
			Prepare(m_Control, Source);
			Prepare(m_Shot, Source);
			Input.m_WantedWeapon = Input.m_NextWeapon = Input.m_PrevWeapon = 0;
			Input.m_Fire = (Input.m_Fire + (Input.m_Fire & 1)) & INPUT_STATE_MASK;
			auto *pShooter = m_Shot.GetCharacterById(LocalId);
			pShooter->OnDirectInput(&Input);
			pShooter->OnDirectInput(&Input);
			Input.m_TargetX = round_to_int(Aim.x);
			Input.m_TargetY = round_to_int(Aim.y);
			if(!Input.m_TargetX && !Input.m_TargetY)
				return Finish(false);
			Input.m_Fire = (Input.m_Fire + 1) & INPUT_STATE_MASK;
			pShooter->OnPredictedInput(&Input);
			pShooter->OnDirectInput(&Input);
			if(pShooter->GetAttackTick() != Source.GameTick())
				return Finish(false);
			int UnfrozenTicks = 0;
			for(int Tick = 0; Tick < 48; ++Tick)
			{
				if((time_get_nanoseconds().count() / 1000 - Start) >= BudgetUs)
				{
					m_TimedOut = true;
					return Finish(false);
				}
				Step(m_Control, LocalId, Input);
				Step(m_Shot, LocalId, Input);
				auto *pBaseline = m_Control.GetCharacterById(TargetId), *pRescued = m_Shot.GetCharacterById(TargetId);
				if(!pBaseline || !pRescued || !m_Shot.GetCharacterById(LocalId))
					return Finish(false);
				const auto &Core = *pRescued->Core();
				if(pBaseline->m_FreezeTime > 0 && pRescued->m_FreezeTime == 0 && !Core.m_IsInFreeze && !Core.m_DeepFrozen && !Core.m_LiveFrozen)
				{
					if(++UnfrozenTicks >= 2)
						return Finish(true);
				}
				else
					UnfrozenTicks = 0;
				const int Index = Source.Collision()->GetPureMapIndex(Core.m_Pos);
				if(Source.Collision()->IsTeleport(Index) || Source.Collision()->IsSpeedup(Index) || Source.Collision()->IsTune(Index) || Source.Collision()->GetSwitchType(Index))
					return Finish(false);
			}
			return Finish(false);
		}
		bool VerifyLaser(CGameWorld &Source, int LocalId, int TargetId, CNetObj_PlayerInput Input, vec2 Aim, int BudgetUs = 1000)
		{
			if(!Supported(Source) || length(Aim) < 1)
				return false;
			const int64_t Start = time_get_nanoseconds().count() / 1000;
			Prepare(m_Shot, Source);
			auto *pLocal = m_Shot.GetCharacterById(LocalId);
			if(!pLocal)
				return false;
			const CTuningParams &Tuning = *m_Shot.GetTuning(pLocal->GetOverriddenTuneZone());
			// Remove old lasers so only the proposed ray can confirm this hit.
			while(auto *pEntity = m_Shot.FindFirst(CGameWorld::ENTTYPE_LASER))
				delete pEntity;
			new CLaser(&m_Shot, pLocal->Core()->m_Pos, normalize(Aim), Tuning.m_LaserReach, LocalId, pLocal->GetActiveWeapon());
			for(int Tick = 0; Tick < 24; ++Tick)
			{
				if((time_get_nanoseconds().count() / 1000 - Start) >= BudgetUs)
					return false;
				bool Found = false;
				for(auto *pEntity = m_Shot.FindFirst(CGameWorld::ENTTYPE_LASER); pEntity; pEntity = pEntity->TypeNext())
				{
					auto *pLaser = static_cast<CLaser *>(pEntity);
					Found = true;
					if(pLaser->GetHitClientId() >= 0)
						return pLaser->GetHitClientId() == TargetId;
				}
				if(!Found)
					return false;
				Step(m_Shot, LocalId, Input);
			}
			return false;
		}
	};

	inline int BounceAims(const CCollision &Collision, vec2 Origin, vec2 Target, vec2 Manual, float Reach,
		float FovRadians, vec2 *pAims, int Capacity)
	{
		int Count = 0;
		const float Angle = std::atan2(Manual.y, Manual.x);
		for(float Offset : {0.0f, -pi / 6, pi / 6, -pi / 3, pi / 3, -pi / 2, pi / 2, pi})
		{
			if(Count >= Capacity)
				break;
			const vec2 Direction(std::cos(Angle + Offset), std::sin(Angle + Offset));
			vec2 Hit, Before;
			int Tele = 0;
			const int Tile = Collision.IntersectLineTeleWeapon(Origin, Origin + Direction * Reach, &Hit, &Before, &Tele);
			if(!Tile || Tile == -1 || Tele)
				continue;
			vec2 Position = Before, Reflected = Direction * 4;
			Collision.MovePoint(&Position, &Reflected, 1, nullptr);
			vec2 Mirror = Target;
			if(Direction.x * Reflected.x < 0)
				Mirror.x = 2 * Before.x - Target.x;
			if(Direction.y * Reflected.y < 0)
				Mirror.y = 2 * Before.y - Target.y;
			const vec2 Aim = Mirror - Origin;
			if(length(Aim) < 1 || length(Aim) > Reach || !PrismAssist::WithinFov(Manual, Aim, FovRadians))
				continue;
			bool Duplicate = false;
			for(int i = 0; i < Count; ++i)
				Duplicate |= distance(normalize(pAims[i]), normalize(Aim)) < 0.03f;
			if(!Duplicate)
				pAims[Count++] = Aim;
		}
		return Count;
	}
}
#endif
