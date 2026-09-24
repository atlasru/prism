// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

#include <algorithm>
#include <array>
#include <cmath>

void CGameClient::PrismComposeAssist(CNetObj_PlayerInput &Input, int ManualDirection, bool ManualJump)
{
	if(!g_Config.m_PrismAimAssist && !g_Config.m_PrismFreezeAvoid)
	{
		m_PrismAimTargetId = -1;
		m_PrismAimActive = false;
		m_PrismAssistPathCount = 0;
		return;
	}
	if(!PrismInputAllowed() || m_Snap.m_LocalClientId < 0 || !m_Snap.m_pLocalCharacter)
	{
		m_PrismAimTargetId = -1;
		m_PrismAimActive = false;
		m_PrismAssistPathCount = 0;
		return;
	}
	CCharacter *pLocal = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId);
	if(!pLocal)
	{
		m_PrismAimActive = false;
		return;
	}
	const CCharacterCore Local = pLocal->GetCore();
	const bool PreviousAimActive = m_PrismAimActive;
	m_PrismAimActive = false;
	if(g_Config.m_PrismAimAssist && (!g_Config.m_PrismAimHookOnly || Input.m_Hook))
	{
		const vec2 Current((float)Input.m_TargetX, (float)Input.m_TargetY);
		if(length(Current) > 0.001f)
		{
			std::array<PrismAssist::STargetCandidate, MAX_CLIENTS> aCandidates{};
			int Count = 0;
			const float MaxRange = (float)g_Config.m_PrismAimRange;
			const float FovRadians = g_Config.m_PrismAimFov * pi / 180.0f;
			for(int Id = 0; Id < MAX_CLIENTS; ++Id)
			{
				if(Id == m_aLocalIds[0] || Id == m_aLocalIds[1] || !m_Snap.m_aCharacters[Id].m_Active)
					continue;
				CCharacter *pTarget = m_PredictedWorld.GetCharacterById(Id);
				if(!pTarget)
					continue;
				const CCharacterCore Target = pTarget->GetCore();
				const vec2 Delta = Target.m_Pos - Local.m_Pos;
				const float Distance = length(Delta);
				if(Distance < 1.0f || Distance > MaxRange)
					continue;
				vec2 CollisionPos, BeforeCollision;
				if(Collision()->IntersectLine(Local.m_Pos, Target.m_Pos, &CollisionPos, &BeforeCollision))
					continue;
				if(!PrismAssist::WithinFov(Current, Delta, FovRadians))
					continue;
				const float Angle = std::abs(std::atan2(std::sin(std::atan2(Delta.y, Delta.x) - std::atan2(Current.y, Current.x)),
						std::cos(std::atan2(Delta.y, Delta.x) - std::atan2(Current.y, Current.x))));
				const float Score = g_Config.m_PrismAimTargetMode == 1 ? Distance :
					g_Config.m_PrismAimTargetMode == 2 ? Angle * 500.0f : distance(Delta, Current);
				aCandidates[Count++] = {Id, Score, Angle};
			}
			const int PreviousTarget = m_PrismAimTargetId;
			const int Selected = PrismAssist::SelectTarget(aCandidates.data(), Count, PreviousTarget);
			m_PrismAimTargetId = Selected >= 0 ? aCandidates[Selected].m_Id : -1;
			if(m_PrismAimTargetId >= 0)
			{
				const CCharacterCore Target = m_PredictedWorld.GetCharacterById(m_PrismAimTargetId)->GetCore();
				// Weapon-specific prediction is isolated here; hook leads target motion.
				const float Lead = g_Config.m_PrismAimPrediction * (Input.m_Hook ? 1.0f : 0.5f);
				m_PrismAimPredictedPos = Target.m_Pos + Target.m_Vel * Lead;
				const vec2 Base = PreviousAimActive && PreviousTarget == m_PrismAimTargetId ? m_PrismAimOutput + Current - m_PrismAimLastManual : Current;
				const vec2 Aim = PrismAssist::InterpolateAim(Base, m_PrismAimPredictedPos - Local.m_Pos,
					g_Config.m_PrismAimStrength / 100.0f, FovRadians / 4.0f);
				m_PrismAimOutput = Aim;
				m_PrismAimLastManual = Current;
				m_PrismAimActive = true;
				Input.m_TargetX = round_to_int(Aim.x);
				Input.m_TargetY = round_to_int(Aim.y);
				if(!Input.m_TargetX && !Input.m_TargetY)
					Input.m_TargetX = 1;
			}
		}
	}
	else
		m_PrismAimTargetId = -1;

	m_PrismAssistPathCount = 0;
	m_PrismAssistSelected = 0;
	m_PrismAvoidJumpCooldown = std::max(0, m_PrismAvoidJumpCooldown - 1);
	if(!g_Config.m_PrismFreezeAvoid || Local.m_Super || Local.m_Invincible || Local.m_IsInFreeze ||
		!m_PrismPredictor.Begin(m_PredictedWorld, m_Snap.m_LocalClientId))
		return;
	const int Horizon = g_Config.m_PrismFreezeHorizon;
	auto Simulate = [&](int Direction, bool Jump) {
		if(m_PrismAssistPathCount >= PrismAssist::MAX_CANDIDATES)
			return;
		auto &Path = m_aPrismAssistPaths[m_PrismAssistPathCount];
		const PrismAssist::SAction Action{Direction, Jump, Horizon};
		if(m_PrismPredictor.Simulate(Input, &Action, 1, Horizon, Path))
		{
			Path.m_Direction = Direction;
			Path.m_Jump = Jump;
			++m_PrismAssistPathCount;
		}
	};
	Simulate(Input.m_Direction, Input.m_Jump != 0);
	if(m_PrismAssistPathCount == 0 || m_aPrismAssistPaths[0].Safe() || m_aPrismAssistPaths[0].m_Unknown || g_Config.m_PrismFreezeAvoid == 1)
	{
		m_PrismAvoidLastDirection = 0;
		return;
	}
	const bool CanJump = !Input.m_Jump && !ManualJump && !m_PrismAvoidJumpCooldown && Local.m_Jumps > Local.m_JumpedTotal;
	if(CanJump)
		Simulate(Input.m_Direction, true);
	if(!ManualDirection)
	{
		for(int Direction : {-1, 0, 1})
			if(Direction != Input.m_Direction)
				Simulate(Direction, Input.m_Jump != 0);
		if(CanJump)
			for(int Direction : {-1, 0, 1})
				if(Direction != Input.m_Direction)
					Simulate(Direction, true);
	}
	auto Correction = PrismAssist::SelectCorrection(m_aPrismAssistPaths.data(), m_PrismAssistPathCount, ManualDirection, ManualJump);
	if(Correction.m_Apply && !ManualDirection && m_PrismAvoidLastDirection && Correction.m_Direction != m_PrismAvoidLastDirection)
		for(int i = 1; i < m_PrismAssistPathCount; ++i)
			if(m_aPrismAssistPaths[i].Safe() && m_aPrismAssistPaths[i].m_Direction == m_PrismAvoidLastDirection &&
				m_aPrismAssistPaths[i].m_Jump == Correction.m_Jump)
			{
				Correction.m_Direction = m_PrismAvoidLastDirection;
				Correction.m_Selected = i;
				break;
			}
	if(Correction.m_Apply)
	{
		Input.m_Direction = Correction.m_Direction;
		Input.m_Jump = Correction.m_Jump;
		m_PrismAssistSelected = Correction.m_Selected;
		m_PrismAvoidLastDirection = Correction.m_Direction;
		if(Correction.m_Jump && !ManualJump)
			m_PrismAvoidJumpCooldown = 6;
	}
	else
		m_PrismAvoidLastDirection = 0;
}

void CGameClient::PrismRenderAssistDebug()
{
	if(!PrismInputAllowed() || !g_Config.m_PrismFreezeDebug && !g_Config.m_PrismAimDebug && g_Config.m_PrismFreezeAvoid != 1)
		return;
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_PrismThemeAccent));
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	for(int i = 0; i < m_PrismAssistPathCount; ++i)
	{
		if(i > 0 && !g_Config.m_PrismFreezeDebug)
			continue;
		const auto &Path = m_aPrismAssistPaths[i];
		if(i == 0 && !g_Config.m_PrismFreezeDebug && Path.m_FirstHazard < 0 && !Path.m_Unknown)
			continue;
		for(int j = 1; j < Path.m_Count; ++j)
		{
			const bool Dangerous = Path.m_aStates[j].m_Hazard;
			Graphics()->SetColor(Dangerous ? ColorRGBA(1.0f, 0.28f, 0.20f, 0.9f) :
				Path.m_aStates[j].m_Unknown ? ColorRGBA(1.0f, 0.75f, 0.24f, 0.75f) :
				i == m_PrismAssistSelected && i > 0 ? Accent.WithAlpha(0.85f) : Accent.WithAlpha(i ? 0.22f : 0.48f));
			const IGraphics::CLineItem Line(Path.m_aStates[j - 1].m_Pos, Path.m_aStates[j].m_Pos);
			Graphics()->LinesDraw(&Line, 1);
		}
	}
	if(g_Config.m_PrismAimDebug && m_PrismAimTargetId >= 0)
	{
		CCharacter *pLocal = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId);
		if(pLocal)
		{
			const vec2 Origin = pLocal->GetCore().m_Pos;
			Graphics()->SetColor(Accent.WithAlpha(0.7f));
			const IGraphics::CLineItem Aim(Origin, m_PrismAimPredictedPos);
			Graphics()->LinesDraw(&Aim, 1);
			for(const vec2 Offset : {vec2(-5.0f, -5.0f), vec2(-5.0f, 5.0f)})
			{
				const IGraphics::CLineItem Cross(m_PrismAimPredictedPos + Offset, m_PrismAimPredictedPos - Offset);
				Graphics()->LinesDraw(&Cross, 1);
			}
		}
	}
	Graphics()->LinesEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
}
