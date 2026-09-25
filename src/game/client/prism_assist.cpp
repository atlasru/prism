// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

#include <algorithm>
#include <array>
#include <cmath>

void CGameClient::PrismComposeAssist(CNetObj_PlayerInput &Input, int ManualDirection, bool ManualJump, bool MacroDirectionOwned)
{
	if(!g_Config.m_PrismAimAssist && !g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismAimDebug && !g_Config.m_PrismFreezeDebug)
	{
		m_PrismAimTargetId = -1;
		m_PrismAimActive = false;
		m_PrismAimDebugReady = false;
		m_PrismAssistPathCount = 0;
		return;
	}
	if(!PrismInputAllowed() || m_Snap.m_LocalClientId < 0 || !m_Snap.m_pLocalCharacter)
	{
		m_PrismAimTargetId = -1;
		m_PrismAimActive = false;
		m_PrismAimDebugReady = false;
		m_PrismAssistPathCount = 0;
		return;
	}
	CGameWorld &World = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId) ? m_PredictedWorld : m_GameWorld;
	CCharacter *pLocal = World.GetCharacterById(m_Snap.m_LocalClientId);
	if(!pLocal)
	{
		m_PrismAimActive = false;
		m_PrismAimDebugReady = false;
		m_PrismAssistPathCount = 0;
		return;
	}
	const CCharacterCore Local = pLocal->GetCore();
	const bool PreviousAimActive = m_PrismAimActive;
	m_PrismAimActive = false;
	m_PrismAimDebugReady = true;
	m_PrismAimCandidateCount = 0;
	m_PrismAimDebugOrigin = Local.m_Pos;
	m_PrismAimDebugDirection = vec2((float)Input.m_TargetX, (float)Input.m_TargetY);
	if((g_Config.m_PrismAimAssist || g_Config.m_PrismAimDebug) && (!g_Config.m_PrismAimHookOnly || Input.m_Hook || g_Config.m_PrismAimDebug))
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
				CCharacter *pTarget = World.GetCharacterById(Id);
				if(!pTarget)
					continue;
				const CCharacterCore Target = pTarget->GetCore();
				const vec2 Delta = Target.m_Pos - Local.m_Pos;
				const float Distance = length(Delta);
				vec2 CollisionPos, BeforeCollision;
				const bool Visible = !Collision()->IntersectLine(Local.m_Pos, Target.m_Pos, &CollisionPos, &BeforeCollision);
				if(!PrismAssist::EligibleTarget(Current, Delta, FovRadians, MaxRange, Visible))
					continue;
				++m_PrismAimCandidateCount;
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
				const CCharacterCore Target = World.GetCharacterById(m_PrismAimTargetId)->GetCore();
				// Weapon-specific prediction is isolated here; hook leads target motion.
				const float Lead = g_Config.m_PrismAimPrediction * (Input.m_Hook ? 1.0f : 0.5f);
				m_PrismAimPredictedPos = Target.m_Pos + Target.m_Vel * Lead;
				const vec2 Base = PreviousAimActive && PreviousTarget == m_PrismAimTargetId ? m_PrismAimOutput + Current - m_PrismAimLastManual : Current;
				const vec2 Aim = PrismAssist::InterpolateAim(Base, m_PrismAimPredictedPos - Local.m_Pos,
					g_Config.m_PrismAimStrength / 100.0f, FovRadians / 4.0f);
				if(g_Config.m_PrismAimAssist && (!g_Config.m_PrismAimHookOnly || Input.m_Hook))
				{
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
	}
	else
		m_PrismAimTargetId = -1;

	m_PrismAssistPathCount = 0;
	m_PrismAssistSelected = 0;
	m_PrismAvoidJumpCooldown = std::max(0, m_PrismAvoidJumpCooldown - 1);
	if((!g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismFreezeDebug) || Local.m_Super || Local.m_Invincible || Local.m_IsInFreeze ||
		!m_PrismPredictor.Begin(World, m_Snap.m_LocalClientId))
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
	if(m_PrismAssistPathCount == 0 || m_aPrismAssistPaths[0].Safe() || m_aPrismAssistPaths[0].m_Unknown || g_Config.m_PrismFreezeAvoid != 2)
	{
		m_PrismAvoidLastDirection = 0;
		return;
	}
	const bool CanJump = !Input.m_Jump && !ManualJump && !m_PrismAvoidJumpCooldown && Local.m_Jumps > Local.m_JumpedTotal;
	if(CanJump)
		Simulate(Input.m_Direction, true);
	// A dangerous held direction may be corrected briefly when simulation
	// finds a safe alternative. The safe base path above never intervenes.
	{
		for(int Direction : {-1, 0, 1})
			if(Direction != Input.m_Direction)
				if(!MacroDirectionOwned)
					Simulate(Direction, Input.m_Jump != 0);
		if(CanJump)
			for(int Direction : {-1, 0, 1})
				if(Direction != Input.m_Direction && !MacroDirectionOwned)
					Simulate(Direction, true);
	}
	auto Correction = PrismAssist::SelectCorrection(m_aPrismAssistPaths.data(), m_PrismAssistPathCount, ManualDirection, ManualJump, 2, MacroDirectionOwned);
	if(Correction.m_Apply && m_PrismAvoidLastDirection && Correction.m_Direction != m_PrismAvoidLastDirection)
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
	if(!PrismInputAllowed() || (!g_Config.m_PrismFreezeDebug && !g_Config.m_PrismAimDebug && g_Config.m_PrismFreezeAvoid != 1))
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
				Path.m_aStates[j].m_NearHazard && i == 0 && g_Config.m_PrismFreezeDebug ? ColorRGBA(0.56f, 0.78f, 1.0f, 0.7f) :
				i == m_PrismAssistSelected && i > 0 ? Accent.WithAlpha(0.85f) : Accent.WithAlpha(i ? 0.22f : 0.48f));
			const IGraphics::CLineItem Line(Path.m_aStates[j - 1].m_Pos, Path.m_aStates[j].m_Pos);
			Graphics()->LinesDraw(&Line, 1);
		}
		if(g_Config.m_PrismFreezeDebug && Path.m_Count && i == 0)
		{
			const vec2 P = Path.m_aStates[Path.m_Count - 1].m_Pos;
			Graphics()->SetColor(Path.m_Unknown ? ColorRGBA(1, 0.75f, 0.24f, 1) :
				Path.m_FirstHazard >= 0 ? ColorRGBA(1, 0.28f, 0.20f, 1) : Accent.WithAlpha(1));
			const IGraphics::CLineItem aCross[] = {{P + vec2(-4, 0), P + vec2(4, 0)}, {P + vec2(0, -4), P + vec2(0, 4)}};
			Graphics()->LinesDraw(aCross, 2);
		}
	}
	if(g_Config.m_PrismAimDebug && m_PrismAimDebugReady)
	{
		const vec2 Origin = m_PrismAimDebugOrigin;
		const vec2 Direction = normalize(m_PrismAimDebugDirection);
		const float HalfAngle = g_Config.m_PrismAimFov * pi / 360.0f;
		const float Range = (float)g_Config.m_PrismAimRange;
		const float Angle = std::atan2(Direction.y, Direction.x);
		vec2 Previous = Origin + vec2(std::cos(Angle - HalfAngle), std::sin(Angle - HalfAngle)) * Range;
		Graphics()->SetColor(Accent.WithAlpha(0.58f));
		for(int i = 0; i <= 24; ++i)
		{
			const float A = Angle - HalfAngle + 2.0f * HalfAngle * i / 24;
			const vec2 End = Origin + vec2(std::cos(A), std::sin(A)) * Range;
			if(i == 0 || i == 24)
			{
				const IGraphics::CLineItem Edge(Origin, End);
				Graphics()->LinesDraw(&Edge, 1);
			}
			if(i > 0)
			{
				const IGraphics::CLineItem Arc(Previous, End);
				Graphics()->LinesDraw(&Arc, 1);
			}
			Previous = End;
		}
		if(m_PrismAimTargetId >= 0)
		{
			Graphics()->SetColor(Accent.WithAlpha(1.0f));
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
	if(g_Config.m_PrismAimDebug && m_PrismAimDebugReady)
	{
		char aStatus[80];
		str_format(aStatus, sizeof(aStatus), "Aim: %d in FOV  target: %d%s", m_PrismAimCandidateCount,
			m_PrismAimTargetId, m_PrismAimActive ? "  correcting" : "");
		TextRender()->Text(m_PrismAimDebugOrigin.x - 55, m_PrismAimDebugOrigin.y - 75, 9.0f, aStatus);
	}
	if(g_Config.m_PrismFreezeDebug && m_PrismAssistPathCount)
	{
		const auto &Base = m_aPrismAssistPaths[0];
		char aStatus[80];
		str_format(aStatus, sizeof(aStatus), "Freeze: %s%s  %d ticks  candidates: %d",
			PrismAssist::DebugStatus(Base),
			Base.Safe() && Base.m_MinSafetyMargin < 16.0f ? " (CLOSE)" : "",
			Base.m_Count - 1, m_PrismAssistPathCount);
		TextRender()->Text(Base.m_aStates[0].m_Pos.x - 55, Base.m_aStates[0].m_Pos.y - 62, 9.0f, aStatus);
	}
}
