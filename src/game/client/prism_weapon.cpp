// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

void CGameClient::PrismResetPlanning()
{
	m_PrismPlanner.Reset();
	m_PrismUnfreezeDecision.Reset();
	m_PrismAimTarget = m_PrismAimWeapon = -1;
	m_PrismAssistTick = m_PrismWeaponTick = m_PrismUnfreezeSearchTick = -1;
	m_PrismRecentMotion = vec2(0, 0);
	m_PrismWeaponDebug = m_PrismUnfreezeConfirmed = false;
	m_PrismWeaponApplied = false;
}

bool CGameClient::PrismComposeWeapons(CNetObj_PlayerInput &Input, bool ManualFire, bool MacroFireOwned, bool ManualPress)
{
	// One outgoing aim owner: macros/physical Hook/Freeze recovery, then rescue,
	// then active weapon profile, then unchanged manual aim. Fire has one composer.
	const bool AimOwned = MacroFireOwned || Input.m_Hook || m_PrismAvoidHookOwned || m_PrismHookActive;
	if(!PrismInputAllowed() || m_Snap.m_LocalClientId < 0 || !m_Snap.m_pLocalCharacter)
	{
		m_PrismAimTarget = -1;
		return false;
	}
	CGameWorld &World = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId) ? m_PredictedWorld : m_GameWorld;
	auto *pLocal = World.GetCharacterById(m_Snap.m_LocalClientId);
	if(!pLocal || AimOwned)
	{
		m_PrismAimTarget = -1;
		return false;
	}
	const auto &Local = *pLocal->Core();
	const int Weapon = pLocal->GetActiveWeapon();
	if(Input.m_WantedWeapon > 0 && Input.m_WantedWeapon - 1 != Weapon)
		return false;
	const auto Profile = PrismWeapon::Profile(Weapon, g_Config);
	const bool AimEnabled = PrismWeapon::Active(Profile, ManualFire, AimOwned);
	const bool RescueEnabled = g_Config.m_PrismUnfreezeSelf || g_Config.m_PrismUnfreezeOthers;
	if(!AimEnabled && !RescueEnabled)
	{
		m_PrismAimTarget = -1;
		return false;
	}
	if(pLocal->m_FreezeTime || Local.m_DeepFrozen || Local.m_LiveFrozen || Weapon < 0 || Weapon >= NUM_WEAPONS ||
		!pLocal->GetWeaponGot(Weapon) || !pLocal->GetWeaponAmmo(Weapon))
		return false;
	if(m_PrismAimWeapon != Weapon)
	{
		m_PrismAimTarget = -1;
		m_PrismAimWeapon = Weapon;
	}
	const int Tick = World.GameTick();
	// Input can be composed more often than physics ticks at high FPS. No repeated
	// searches, smoothing accumulation, or synthetic press within the same tick.
	const vec2 Manual(Input.m_TargetX, Input.m_TargetY);
	if(Tick == m_PrismWeaponTick)
	{
		if(m_PrismWeaponApplied && Manual == m_PrismWeaponLastManual)
		{
			Input.m_TargetX = round_to_int(m_PrismWeaponAim.x);
			Input.m_TargetY = round_to_int(m_PrismWeaponAim.y);
		}
		return false;
	}
	m_PrismWeaponTick = Tick;
	m_PrismWeaponDebug = m_PrismUnfreezeConfirmed = false;
	m_PrismWeaponLastManual = Manual;
	m_PrismWeaponApplied = false;
	const CTuningParams &Tuning = *World.GetTuning(pLocal->GetOverriddenTuneZone());
	m_PrismWeaponOrigin = Local.m_Pos;
	auto Apply = [&](vec2 Aim, vec2 Hit, bool Rescue) {
		Input.m_TargetX = round_to_int(Aim.x);
		Input.m_TargetY = round_to_int(Aim.y);
		if(!Input.m_TargetX && !Input.m_TargetY)
			Input.m_TargetX = 1;
		m_PrismWeaponAim = Aim;
		m_PrismWeaponHit = Hit;
		m_PrismWeaponApplied = true;
		m_PrismWeaponDebug = Rescue ? g_Config.m_PrismUnfreezeDebug : Profile.m_Debug;
		m_PrismUnfreezeConfirmed = Rescue;
	};
	const int64_t SearchStart = time_get_nanoseconds().count() / 1000;
	auto RemainingUs = [&] { return std::max(0, 2500 - static_cast<int>((time_get_nanoseconds().count() / 1000 - SearchStart))); };
	const bool CanRescueNow = RescueEnabled && !(Weapon == WEAPON_HAMMER && ManualFire && !ManualPress) && m_PrismUnfreezeDecision.Ready(Tick) && !pLocal->GetReloadTimer() &&
				  (Tick - m_PrismUnfreezeSearchTick >= 5 || m_PrismUnfreezeSearchTick < 0) && PrismWeapon::CShotPredictor::Supported(World);
	if(CanRescueNow)
	{
		m_PrismUnfreezeSearchTick = Tick;
		const float Fov = g_Config.m_PrismUnfreezeFov * pi / 180;
		if(g_Config.m_PrismUnfreezeOthers && (ManualFire || g_Config.m_PrismUnfreezeActivation) &&
			PrismWeapon::CanUnfreeze(Weapon, false, World.m_WorldConfig.m_IsDDRace, World.m_WorldConfig.m_OldLaser))
		{
			std::array<PrismAssist::STargetCandidate, MAX_CLIENTS> aTargets{};
			int Count = 0;
			for(int Id = 0; Id < MAX_CLIENTS; ++Id)
			{
				auto *pTarget = World.GetCharacterById(Id);
				if(Id == m_Snap.m_LocalClientId || !pTarget || pTarget->m_FreezeTime <= 0 || !pLocal->SameTeam(Id) ||
					!pLocal->CanCollide(Id) || pTarget->Core()->m_DeepFrozen || pTarget->Core()->m_LiveFrozen)
					continue;
				const vec2 Delta = pTarget->Core()->m_Pos - Local.m_Pos;
				if(!PrismAssist::EligibleTarget(Manual, Delta, Fov, g_Config.m_PrismUnfreezeRange, true))
					continue;
				const float Angle = std::abs(std::atan2(Manual.x * Delta.y - Manual.y * Delta.x, dot(Manual, Delta)));
				aTargets[Count++] = {Id, g_Config.m_PrismUnfreezePriority ? length(Delta) : Angle * 1000, Angle};
			}
			std::sort(aTargets.begin(), aTargets.begin() + Count, [](const auto &A, const auto &B) { return A.m_Score < B.m_Score; });
			for(int i = 0; i < std::min(Count, 3) && RemainingUs() > 100; ++i)
			{
				auto *pTarget = World.GetCharacterById(aTargets[i].m_Id);
				const vec2 Hit = pTarget->Core()->m_Pos;
				const vec2 Aim = Hit - Local.m_Pos;
				if(!m_PrismShotPredictor.VerifyUnfreeze(World, m_Snap.m_LocalClientId, aTargets[i].m_Id, Input, Aim, RemainingUs()))
					continue;
				if(m_PrismUnfreezeDecision.Commit(Tick, std::max(g_Config.m_PrismUnfreezeCooldown, static_cast<int>(Tuning.GetWeaponFireDelay(Weapon) * SERVER_TICK_SPEED) + 1), true, AimOwned, true))
				{
					Apply(Aim, Hit, true);
					return true;
				}
			}
		}
		if(g_Config.m_PrismUnfreezeSelf && RemainingUs() > 100 &&
			PrismWeapon::CanUnfreeze(Weapon, true, World.m_WorldConfig.m_IsDDRace, World.m_WorldConfig.m_OldLaser) &&
			m_PrismPredictor.Begin(World, m_Snap.m_LocalClientId))
		{
			PrismAssist::STrajectory Path;
			const PrismAssist::SAction Action{Input.m_Direction, bool(Input.m_Jump), PrismAssist::MAX_TICKS};
			m_PrismPredictor.Simulate(Input, &Action, 1, PrismAssist::MAX_TICKS, Path);
			if(Path.m_FirstHazard >= 0 && !Path.m_Unknown)
			{
				const vec2 Target = Path.m_aStates[Path.m_FirstHazard].m_Pos;
				std::array<vec2, 4> aAims{};
				const int Count = PrismWeapon::BounceAims(*Collision(), Local.m_Pos, Target, Manual, Tuning.m_LaserReach, Fov, aAims.data(), aAims.size());
				for(int i = 0; i < Count && RemainingUs() > 100; ++i)
					if(m_PrismShotPredictor.VerifyUnfreeze(World, m_Snap.m_LocalClientId, m_Snap.m_LocalClientId, Input, aAims[i], RemainingUs()) &&
						m_PrismUnfreezeDecision.Commit(Tick, std::max(g_Config.m_PrismUnfreezeCooldown, static_cast<int>(Tuning.GetWeaponFireDelay(Weapon) * SERVER_TICK_SPEED) + 1), true, AimOwned, true))
					{
						Apply(aAims[i], Target, true);
						return true;
					}
			}
		}
	}
	if(!AimEnabled || RemainingUs() <= 100 || (Weapon == WEAPON_GUN && Local.m_Jetpack))
		return false;
	std::array<PrismAssist::STargetCandidate, MAX_CLIENTS> aTargets{};
	int Count = 0;
	for(int Id = 0; Id < MAX_CLIENTS; ++Id)
	{
		auto *pTarget = World.GetCharacterById(Id);
		if(Id == m_aLocalIds[0] || Id == m_aLocalIds[1] || !pTarget || !pLocal->CanCollide(Id) ||
			!m_Snap.m_aCharacters[Id].m_Active)
			continue;
		const vec2 Delta = pTarget->Core()->m_Pos - Local.m_Pos;
		if(!PrismAssist::EligibleTarget(Manual, Delta, Profile.m_Fov * pi / 180, Profile.m_Range, true))
			continue;
		const float Angle = std::abs(std::atan2(Manual.x * Delta.y - Manual.y * Delta.x, dot(Manual, Delta)));
		const float Score = (Profile.m_Priority ? length(Delta) : Angle * 1000 + length(Delta) * 0.01f) * (Id == m_PrismAimTarget ? 0.82f : 1);
		aTargets[Count++] = {Id, Score, Angle};
	}
	std::sort(aTargets.begin(), aTargets.begin() + Count, [](const auto &A, const auto &B) { return A.m_Score < B.m_Score; });
	const int PreviousId = m_PrismAimTarget;
	m_PrismAimTarget = -1;
	for(int i = 0; i < std::min(Count, PrismWeapon::MAX_TARGETS) && RemainingUs() > 100; ++i)
	{
		auto *pTarget = World.GetCharacterById(aTargets[i].m_Id);
		const auto &Target = *pTarget->Core();
		auto Solution = PrismWeapon::Solve(Weapon, Profile, Tuning, World.m_WorldConfig.m_IsDDRace, *Collision(), Local.m_Pos, Target.m_Pos, Target.m_Vel);
		if(!Solution.m_Valid && Weapon == WEAPON_LASER && Profile.m_Bounce)
		{
			std::array<vec2, 3> aAims{};
			const int Aims = PrismWeapon::BounceAims(*Collision(), Local.m_Pos, Target.m_Pos, Manual, Tuning.m_LaserReach, Profile.m_Fov * pi / 180, aAims.data(), aAims.size());
			for(int j = 0; j < Aims && RemainingUs() > 100; ++j)
				if(m_PrismShotPredictor.VerifyLaser(World, m_Snap.m_LocalClientId, aTargets[i].m_Id, Input, aAims[j], RemainingUs()))
				{
					Solution.m_Valid = true;
					Solution.m_Aim = aAims[j];
					Solution.m_Hit = Target.m_Pos;
					Solution.m_Bounces = 1;
					break;
				}
		}
		if(!Solution.m_Valid || !PrismAssist::WithinFov(Manual, Solution.m_Aim, Profile.m_Fov * pi / 180))
			continue;
		const vec2 Base = PreviousId == aTargets[i].m_Id ? m_PrismAimOutput + Manual - m_PrismAimManual : Manual;
		const vec2 Aim = PrismWeapon::CorrectAim(Base, Solution, Profile);
		// Confirm a reachable target and reject obstructed outgoing corrections.
		// Aiming can converge over ticks; only rescue owns an automatic fire edge.
		if(Weapon == WEAPON_HAMMER && (!PrismWeapon::HammerReach(Local.m_Pos, Solution.m_Hit, Aim) || pLocal->HammerHitDisabled()))
			continue;
		if(Weapon == WEAPON_LASER || (Weapon == WEAPON_SHOTGUN && World.m_WorldConfig.m_IsDDRace))
		{
			if(!m_PrismShotPredictor.VerifyLaser(World, m_Snap.m_LocalClientId, aTargets[i].m_Id, Input, Solution.m_Aim, RemainingUs()))
				continue;
			if(!Solution.m_Bounces)
			{
				vec2 Hit, Before;
				int Tele = 0;
				if(Collision()->IntersectLineTeleWeapon(Local.m_Pos, Local.m_Pos + normalize(Aim) * distance(Local.m_Pos, Solution.m_Hit), &Hit, &Before, &Tele) || Tele)
					continue;
			}
		}
		if(Weapon == WEAPON_GUN || Weapon == WEAPON_GRENADE || (Weapon == WEAPON_SHOTGUN && !World.m_WorldConfig.m_IsDDRace))
		{
			const float Speed = Weapon == WEAPON_GRENADE ? Tuning.m_GrenadeSpeed : Weapon == WEAPON_SHOTGUN ? Tuning.m_ShotgunSpeed :
															  Tuning.m_GunSpeed;
			const float Curvature = Weapon == WEAPON_GRENADE ? Tuning.m_GrenadeCurvature : Weapon == WEAPON_SHOTGUN ? Tuning.m_ShotgunCurvature :
																  Tuning.m_GunCurvature;
			Solution.m_Aim = Aim;
			if(!PrismWeapon::ClearProjectile(*Collision(), Local.m_Pos, Solution, Speed, Curvature))
				continue;
		}
		m_PrismAimTarget = aTargets[i].m_Id;
		m_PrismAimOutput = Aim;
		m_PrismAimManual = Manual;
		Apply(Aim, Solution.m_Hit, false);
		return false; // Aiming never generates fire.
	}
	return false;
}
