// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

#include <algorithm>
#include <array>
#include <cmath>

bool CGameClient::PrismComposeAssist(CNetObj_PlayerInput &Input, int ManualDirection, bool ManualJump, bool MacroDirectionOwned, bool MacroFirePulse, bool MacroHookOwned)
{
	if(!g_Config.m_PrismTriggerEnabled && !g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismTriggerDebug && !g_Config.m_PrismFreezeDebug)
	{
		m_PrismTriggerTargetId = -1;
		m_PrismTriggerHookOwned = false;
		m_PrismTriggerDebugReady = false;
		m_PrismTriggerOutputActive = false;
		m_PrismAssistPathCount = 0;
		return false;
	}
	if(!PrismInputAllowed() || m_Snap.m_LocalClientId < 0 || !m_Snap.m_pLocalCharacter)
	{
		m_PrismTriggerTargetId = -1;
		m_PrismTriggerHookOwned = false;
		m_PrismTriggerDebugReady = false;
		m_PrismTriggerOutputActive = false;
		m_PrismAssistPathCount = 0;
		return false;
	}
	CGameWorld &World = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId) ? m_PredictedWorld : m_GameWorld;
	CCharacter *pLocal = World.GetCharacterById(m_Snap.m_LocalClientId);
	if(!pLocal)
	{
		m_PrismTriggerTargetId = -1;
		m_PrismTriggerHookOwned = false;
		m_PrismTriggerDebugReady = false;
		m_PrismTriggerOutputActive = false;
		m_PrismAssistPathCount = 0;
		return false;
	}
	const CCharacterCore Local = pLocal->GetCore();
	bool FirePulse = false;
	m_PrismTriggerOutputActive = false;
	m_PrismTriggerMatched = false;
	m_PrismTriggerDebugReady = true;
	m_PrismTriggerCandidateCount = 0;
	m_PrismTriggerDebugOrigin = Local.m_Pos;
	m_PrismTriggerDebugDirection = vec2((float)Input.m_TargetX, (float)Input.m_TargetY);
	if(g_Config.m_PrismTriggerEnabled || g_Config.m_PrismTriggerDebug)
	{
		const vec2 Current((float)Input.m_TargetX, (float)Input.m_TargetY);
		std::array<PrismAssist::STargetCandidate, MAX_CLIENTS> aCandidates{};
		std::array<vec2, MAX_CLIENTS> aPredicted{};
		std::array<bool, MAX_CLIENTS> aMatched{};
		int Count = 0;
		const float Range = (float)g_Config.m_PrismTriggerRange;
		const float Tolerance = g_Config.m_PrismTriggerTolerance * pi / 180.0f;
		for(int Id = 0; Id < MAX_CLIENTS; ++Id)
		{
			if(Id == m_aLocalIds[0] || Id == m_aLocalIds[1] || !m_Snap.m_aCharacters[Id].m_Active)
				continue;
			CCharacter *pTarget = World.GetCharacterById(Id);
			if(!pTarget)
				continue;
			const CCharacterCore Target = pTarget->GetCore();
			vec2 CollisionPos, BeforeCollision;
			if(Collision()->IntersectLine(Local.m_Pos, Target.m_Pos, &CollisionPos, &BeforeCollision))
				continue;
			const vec2 Predicted = PrismAssist::PredictedTarget(Target.m_Pos, Target.m_Vel, g_Config.m_PrismTriggerPrediction);
			const vec2 Delta = Predicted - Local.m_Pos;
			if(length(Delta) < 1.0f || length(Delta) > Range ||
				Collision()->IntersectLine(Local.m_Pos, Predicted, &CollisionPos, &BeforeCollision))
				continue;
			const bool Matched = PrismAssist::TriggerAligned(Current, Delta, Range, Tolerance, true);
			const float Angle = std::abs(std::atan2(Current.x * Delta.y - Current.y * Delta.x,
				Current.x * Delta.x + Current.y * Delta.y));
			aCandidates[Count] = {Id, Angle * 500.0f + (Matched ? 0.0f : 10000.0f), Angle};
			aPredicted[Count] = Predicted;
			aMatched[Count] = Matched;
			++Count;
		}
		m_PrismTriggerCandidateCount = Count;
		const int Selected = PrismAssist::SelectTarget(aCandidates.data(), Count, m_PrismTriggerTargetId);
		m_PrismTriggerTargetId = Selected >= 0 ? aCandidates[Selected].m_Id : -1;
		if(Selected >= 0)
		{
			m_PrismTriggerPredictedPos = aPredicted[Selected];
			m_PrismTriggerMatched = aMatched[Selected];
		}
		const int Tick = Client()->GameTick(g_Config.m_ClDummy);
		const bool Ready = PrismAssist::TriggerReady(m_PrismTriggerMatched, Tick, m_PrismTriggerLastTick, g_Config.m_PrismTriggerCooldown);
		const auto Trigger = PrismAssist::ComposeTrigger(Input, g_Config.m_PrismTriggerEnabled, m_PrismTriggerMatched,
			Ready, g_Config.m_PrismTriggerAction, MacroFirePulse, MacroHookOwned, m_PrismTriggerHookOwned);
		if(Trigger.m_FirePulse || Trigger.m_HookOwned && !m_PrismTriggerHookOwned)
			m_PrismTriggerLastTick = Tick;
		FirePulse = Trigger.m_FirePulse;
		m_PrismTriggerOutputActive = FirePulse || Trigger.m_HookOwned;
		m_PrismTriggerHookOwned = Trigger.m_HookOwned;
	}
	else
	{
		m_PrismTriggerTargetId = -1;
		m_PrismTriggerHookOwned = false;
	}

	m_PrismAssistPathCount = 0;
	m_PrismAssistSelected = 0;
	m_PrismAvoidJumpCooldown = std::max(0, m_PrismAvoidJumpCooldown - 1);
	if((!g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismFreezeDebug) || Local.m_Super || Local.m_Invincible || Local.m_IsInFreeze ||
		!m_PrismPredictor.Begin(World, m_Snap.m_LocalClientId))
		return FirePulse;
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
		return FirePulse;
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
	return FirePulse;
}

void CGameClient::PrismRenderAssistDebug()
{
	if(!PrismInputAllowed() || (!g_Config.m_PrismFreezeDebug && !g_Config.m_PrismTriggerDebug && g_Config.m_PrismFreezeAvoid != 1))
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
	if(g_Config.m_PrismTriggerDebug && m_PrismTriggerDebugReady)
	{
		const vec2 Origin = m_PrismTriggerDebugOrigin;
		const vec2 Direction = normalize(m_PrismTriggerDebugDirection);
		const float HalfAngle = g_Config.m_PrismTriggerTolerance * pi / 180.0f;
		const float Range = (float)g_Config.m_PrismTriggerRange;
		const float Angle = std::atan2(Direction.y, Direction.x);
		Graphics()->SetColor(m_PrismTriggerMatched ? Accent.WithAlpha(0.95f) : Accent.WithAlpha(0.58f));
		const IGraphics::CLineItem Ray(Origin, Origin + Direction * Range);
		Graphics()->LinesDraw(&Ray, 1);
		for(int i : {-1, 1})
		{
			const float A = Angle + HalfAngle * i;
			const vec2 End = Origin + vec2(std::cos(A), std::sin(A)) * Range;
			Graphics()->SetColor(Accent.WithAlpha(0.3f));
			const IGraphics::CLineItem Edge(Origin, End);
			Graphics()->LinesDraw(&Edge, 1);
		}
		if(m_PrismTriggerTargetId >= 0)
		{
			Graphics()->SetColor(m_PrismTriggerMatched ? Accent.WithAlpha(1.0f) : ColorRGBA(0.65f, 0.73f, 0.81f, 0.9f));
			for(const vec2 Offset : {vec2(-5.0f, -5.0f), vec2(-5.0f, 5.0f)})
			{
				const IGraphics::CLineItem Cross(m_PrismTriggerPredictedPos + Offset, m_PrismTriggerPredictedPos - Offset);
				Graphics()->LinesDraw(&Cross, 1);
			}
		}
	}
	Graphics()->LinesEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	if(g_Config.m_PrismTriggerDebug && m_PrismTriggerDebugReady)
	{
		char aStatus[96];
		str_format(aStatus, sizeof(aStatus), "Trigger %s: %s  target: %d  visible: %d%s",
			g_Config.m_PrismTriggerAction ? "hook" : "fire", m_PrismTriggerMatched ? "ALIGNED" : "WAITING",
			m_PrismTriggerTargetId, m_PrismTriggerCandidateCount, m_PrismTriggerOutputActive ? "  ACTIVE" : "");
		TextRender()->Text(m_PrismTriggerDebugOrigin.x - 55, m_PrismTriggerDebugOrigin.y - 75, 9.0f, aStatus);
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
