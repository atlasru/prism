// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_WEAPON_H
#define GAME_CLIENT_PRISM_WEAPON_H

#include "prism_assist.h"

#include <engine/shared/config.h>

namespace PrismWeapon
{
	constexpr int MAX_TARGETS = 8;
	constexpr int MAX_FLIGHT_TICKS = 100;
	struct SProfile
	{
		bool m_Enabled = false;
		int m_Activation = 0; // 0 physical Fire, 1 always while input is allowed
		int m_Fov = 30, m_Range = 500, m_Prediction = 100;
		int m_Correction = 8, m_Smoothing = 35, m_Priority = 0;
		bool m_Debug = false, m_Bounce = false;
	};
	inline SProfile Profile(int Weapon, const CConfig &Config)
	{
		SProfile Result;
#define PRISM_READ_PROFILE(Name) Result = {bool(Config.m_PrismAim##Name), Config.m_PrismAim##Name##Activation, Config.m_PrismAim##Name##Fov, Config.m_PrismAim##Name##Range, Config.m_PrismAim##Name##Prediction, Config.m_PrismAim##Name##Correction, Config.m_PrismAim##Name##Smoothing, Config.m_PrismAim##Name##Priority, bool(Config.m_PrismAim##Name##Debug), false}
		switch(Weapon)
		{
		case WEAPON_HAMMER: PRISM_READ_PROFILE(Hammer); break;
		case WEAPON_GUN: PRISM_READ_PROFILE(Gun); break;
		case WEAPON_SHOTGUN: PRISM_READ_PROFILE(Shotgun); break;
		case WEAPON_GRENADE: PRISM_READ_PROFILE(Grenade); break;
		case WEAPON_LASER:
			PRISM_READ_PROFILE(Laser);
			Result.m_Bounce = Config.m_PrismAimLaserBounce;
			break;
		default: break; // Ninja direction is a dash, not a supported aiming solver.
		}
#undef PRISM_READ_PROFILE
		return Result;
	}
	inline bool Active(const SProfile &Profile, bool PhysicalFire, bool AimOwned)
	{
		return Profile.m_Enabled && !AimOwned && (Profile.m_Activation == 1 || PhysicalFire);
	}
	struct SSolution
	{
		bool m_Valid = false;
		vec2 m_Aim = vec2(1, 0), m_Hit = vec2(0, 0);
		float m_Time = 0;
		int m_Bounces = 0;
	};
	// Solve CalcPos's parabola, including the character's projectile spawn offset.
	// Target velocity is in units/tick; projectile speed and time use units/second.
	inline SSolution Intercept(vec2 Origin, vec2 Target, vec2 Velocity, float Speed, float Curvature,
		float Lifetime, float Prediction = 1, float Offset = CCharacterCore::PhysicalSize() * 0.75f)
	{
		SSolution Result;
		if(Speed <= 0 || Lifetime <= 0)
			return Result;
		Lifetime = std::min(Lifetime, MAX_FLIGHT_TICKS / static_cast<float>(SERVER_TICK_SPEED));
		auto Delta = [&](float Time) { return Target + Velocity * (Time * static_cast<float>(SERVER_TICK_SPEED) * Prediction) - Origin -
						      vec2(0, Curvature / 10000 * Speed * Speed * Time * Time); };
		auto Error = [&](float Time) { return length(Delta(Time)) - (Offset + Speed * Time); };
		float PreviousTime = 0, PreviousError = Error(0);
		for(int Step = 1; Step <= MAX_FLIGHT_TICKS; ++Step)
		{
			const float Time = Lifetime * Step / MAX_FLIGHT_TICKS;
			const float CurrentError = Error(Time);
			if(PreviousError >= 0 && CurrentError <= 0)
			{
				float Low = PreviousTime, High = Time;
				for(int i = 0; i < 12; ++i)
				{
					const float Mid = (Low + High) * 0.5f;
					if(Error(Mid) > 0)
						Low = Mid;
					else
						High = Mid;
				}
				Result.m_Time = (Low + High) * 0.5f;
				Result.m_Hit = Target + Velocity * (Result.m_Time * static_cast<float>(SERVER_TICK_SPEED) * Prediction);
				Result.m_Aim = normalize(Delta(Result.m_Time)) * std::max(1.0f, distance(Origin, Result.m_Hit));
				Result.m_Valid = true;
				return Result;
			}
			PreviousTime = Time;
			PreviousError = CurrentError;
		}
		return Result;
	}
	inline bool ClearProjectile(const CCollision &Collision, vec2 Origin, const SSolution &Solution, float Speed, float Curvature)
	{
		const vec2 Direction = normalize(Solution.m_Aim);
		const vec2 Start = Origin + Direction * (CCharacterCore::PhysicalSize() * 0.75f);
		vec2 Previous = Start, Hit, Before;
		const int Steps = std::clamp(static_cast<int>(std::ceil(Solution.m_Time * static_cast<float>(SERVER_TICK_SPEED))), 1, MAX_FLIGHT_TICKS);
		for(int i = 1; i <= Steps; ++i)
		{
			const vec2 Position = CalcPos(Start, Direction, Curvature, Speed, Solution.m_Time * i / Steps);
			int Tele = 0;
			if(Collision.IntersectLineTeleWeapon(Previous, Position, &Hit, &Before, &Tele) || Tele)
				return false;
			Previous = Position;
		}
		return true;
	}
	inline bool HammerReach(vec2 Origin, vec2 Target, vec2 Aim)
	{
		return length(Aim) > 0.01f && distance(Origin + normalize(Aim) * (CCharacterCore::PhysicalSize() * 0.75f), Target) <
						      CCharacterCore::PhysicalSize() * 1.5f;
	}
	inline SSolution Solve(int Weapon, const SProfile &Profile, const CTuningParams &Tuning,
		bool DDRace, const CCollision &Collision, vec2 Origin, vec2 Target, vec2 Velocity)
	{
		SSolution Result;
		Result.m_Hit = Target;
		Result.m_Aim = Target - Origin;
		if(length(Result.m_Aim) < 1 || length(Result.m_Aim) > Profile.m_Range)
			return Result;
		if(Weapon == WEAPON_HAMMER)
		{
			Result.m_Hit += Velocity * (Profile.m_Prediction / 100.0f);
			Result.m_Aim = Result.m_Hit - Origin;
			Result.m_Valid = HammerReach(Origin, Result.m_Hit, Result.m_Aim);
			return Result;
		}
		if(Weapon == WEAPON_LASER || (Weapon == WEAPON_SHOTGUN && DDRace))
		{
			vec2 Hit, Before;
			int Tele = 0;
			Result.m_Valid = length(Result.m_Aim) <= static_cast<float>(Tuning.m_LaserReach) &&
					 !Collision.IntersectLineTeleWeapon(Origin, Target, &Hit, &Before, &Tele) && !Tele;
			return Result;
		}
		float Speed = Tuning.m_GunSpeed, Curvature = Tuning.m_GunCurvature, Lifetime = Tuning.m_GunLifetime;
		if(Weapon == WEAPON_GRENADE)
		{
			Speed = Tuning.m_GrenadeSpeed;
			Curvature = Tuning.m_GrenadeCurvature;
			Lifetime = Tuning.m_GrenadeLifetime;
		}
		if(Weapon == WEAPON_SHOTGUN)
		{
			Speed = Tuning.m_ShotgunSpeed;
			Curvature = Tuning.m_ShotgunCurvature;
			Lifetime = Tuning.m_ShotgunLifetime;
		}
		Result = Intercept(Origin, Target, Velocity, Speed, Curvature, Lifetime, Profile.m_Prediction / 100.0f);
		Result.m_Valid &= Result.m_Valid && length(Result.m_Hit - Origin) <= Profile.m_Range && ClearProjectile(Collision, Origin, Result, Speed, Curvature);
		return Result;
	}
	inline vec2 CorrectAim(vec2 Manual, const SSolution &Solution, const SProfile &Profile)
	{
		if(!Profile.m_Enabled || !Solution.m_Valid || !PrismAssist::WithinFov(Manual, Solution.m_Aim, Profile.m_Fov * pi / 180))
			return Manual;
		return PrismAssist::InterpolateAim(Manual, Solution.m_Aim, Profile.m_Smoothing / 100.0f, Profile.m_Correction * pi / 180);
	}
	// Separate fire decision state. A proven shot gets one edge and a cooldown.
	class CUnfreezeDecision
	{
		int m_NextTick = 0, m_LastTick = -1;

	public:
		void Reset() { *this = {}; }
		bool Ready(int Tick) const { return Tick >= m_NextTick && Tick != m_LastTick; }
		bool Commit(int Tick, int Cooldown, bool Confirmed, bool Owned, bool CanFire)
		{
			if(!Confirmed || Owned || !CanFire || !Ready(Tick))
				return false;
			m_LastTick = Tick;
			m_NextTick = Tick + std::max(2, Cooldown);
			return true;
		}
	};
	inline bool CanUnfreeze(int Weapon, bool Self, bool DDRace, bool OldLaser)
	{
		return DDRace && (Self ? Weapon == WEAPON_LASER && !OldLaser : Weapon == WEAPON_LASER || Weapon == WEAPON_HAMMER);
	}
}
#endif
