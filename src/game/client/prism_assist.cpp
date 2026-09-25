// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

#include <algorithm>
#include <array>
#include <cmath>

void CGameClient::PrismComposeAssist(CNetObj_PlayerInput &Input, int ManualDirection, bool ManualJump, bool MacroDirectionOwned, bool ManualHook, bool MacroHookOwned)
{
	m_PrismManualHookHeld = ManualHook;
	m_PrismHookActive = false;
	m_PrismHookCandidateCount = 0;
	m_PrismHookDebugReady = false;
	if(!ManualHook || !g_Config.m_PrismHookAssist)
		m_PrismHookTargetId = -1;
	if(!g_Config.m_PrismHookAssist && !g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismHookDebug && !g_Config.m_PrismFreezeDebug)
	{
		m_PrismAssistPathCount = 0;
		m_PrismAvoidHookOwned = false;
		return;
	}
	if(!PrismInputAllowed() || m_Snap.m_LocalClientId < 0 || !m_Snap.m_pLocalCharacter)
	{
		m_PrismHookTargetId = -1;
		m_PrismAssistPathCount = 0;
		m_PrismAvoidHookOwned = false;
		return;
	}
	CGameWorld &World = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId) ? m_PredictedWorld : m_GameWorld;
	CCharacter *pLocal = World.GetCharacterById(m_Snap.m_LocalClientId);
	if(!pLocal)
	{
		m_PrismHookTargetId = -1;
		m_PrismAssistPathCount = 0;
		m_PrismAvoidHookOwned = false;
		return;
	}
	const CCharacterCore Local = pLocal->GetCore();
	const vec2 ManualAim((float)Input.m_TargetX, (float)Input.m_TargetY);
	m_PrismHookDebugReady = true;
	m_PrismHookDebugOrigin = Local.m_Pos;
	m_PrismHookDebugDirection = ManualAim;
	if(ManualHook && g_Config.m_PrismHookAssist)
	{
		std::array<PrismAssist::STargetCandidate, MAX_CLIENTS> aCandidates{};
		std::array<vec2, MAX_CLIENTS> aPredicted{};
		int Count = 0;
		const float Fov = g_Config.m_PrismHookFov * pi / 180.0f;
		for(int Id = 0; Id < MAX_CLIENTS; ++Id)
		{
			if(Id == m_aLocalIds[0] || Id == m_aLocalIds[1] || !m_Snap.m_aCharacters[Id].m_Active)
				continue;
			CCharacter *pTarget = World.GetCharacterById(Id);
			if(!pTarget)
				continue;
			const CCharacterCore Target = pTarget->GetCore();
			vec2 Hit, Before;
			if(Collision()->IntersectLine(Local.m_Pos, Target.m_Pos, &Hit, &Before))
				continue;
			const vec2 Predicted = PrismAssist::PredictedTarget(Target.m_Pos, Target.m_Vel, g_Config.m_PrismHookPrediction);
			const vec2 Delta = Predicted - Local.m_Pos;
			if(!PrismAssist::EligibleTarget(ManualAim, Delta, Fov, (float)g_Config.m_PrismHookRange,
				!Collision()->IntersectLine(Local.m_Pos, Predicted, &Hit, &Before)))
				continue;
			const float Angle = std::abs(std::atan2(ManualAim.x * Delta.y - ManualAim.y * Delta.x,
				ManualAim.x * Delta.x + ManualAim.y * Delta.y));
			aCandidates[Count] = {Id, Angle * 500.0f + distance(Delta, ManualAim) * 0.01f, Angle};
			aPredicted[Count++] = Predicted;
		}
		m_PrismHookCandidateCount = Count;
		const int Selected = PrismAssist::SelectTarget(aCandidates.data(), Count, m_PrismHookTargetId);
		const int PreviousId = m_PrismHookTargetId;
		const bool PreviousActive = m_PrismHookTargetId >= 0;
		m_PrismHookTargetId = Selected < 0 ? -1 : aCandidates[Selected].m_Id;
		if(Selected >= 0)
		{
			m_PrismHookPredictedPos = aPredicted[Selected];
			const vec2 Base = PreviousActive && PreviousId == m_PrismHookTargetId ?
				m_PrismHookOutput + ManualAim - m_PrismHookLastManual : ManualAim;
			const vec2 Aim = PrismAssist::HookAssistAim(Base, m_PrismHookPredictedPos - Local.m_Pos,
				ManualHook, g_Config.m_PrismHookAssist, true, g_Config.m_PrismHookStrength / 100.0f, Fov / 4.0f);
			m_PrismHookOutput = Aim;
			m_PrismHookLastManual = ManualAim;
			m_PrismHookActive = true;
			Input.m_TargetX = round_to_int(Aim.x);
			Input.m_TargetY = round_to_int(Aim.y);
			if(!Input.m_TargetX && !Input.m_TargetY)
				Input.m_TargetX = 1;
		}
	}
	m_PrismAssistPathCount = 0;
	m_PrismAssistSelected = 0;
	m_PrismAvoidHookCandidateCount = 0;
	m_PrismAvoidRouteCandidate = -1;
	m_PrismAvoidEmergencyCandidate = -1;
	m_PrismAvoidDeferred = false;
	m_PrismAvoidJumpCooldown = std::max(0, m_PrismAvoidJumpCooldown - 1);
	if(ManualDirection)
	{
		m_PrismAvoidIntentDirection = ManualDirection;
		m_PrismAvoidIntentTicks = 8;
	}
	else if(m_PrismAvoidIntentTicks > 0)
		--m_PrismAvoidIntentTicks;
	else
		m_PrismAvoidIntentDirection = 0;
	if((!g_Config.m_PrismFreezeAvoid && !g_Config.m_PrismFreezeDebug) || Local.m_Super || Local.m_Invincible || Local.m_IsInFreeze ||
		!m_PrismPredictor.Begin(World, m_Snap.m_LocalClientId))
	{
		m_PrismAvoidHookOwned = false;
		return;
	}
	const int Horizon = g_Config.m_PrismFreezeHorizon;
	const int Travel = PrismAssist::IntendedDirection(ManualDirection, Local.m_Vel.x, m_PrismAvoidIntentDirection, ManualHook, ManualAim);
	m_PrismAvoidInferredDirection = Travel;
	auto SimulateActions = [&](const PrismAssist::SAction *pActions, int Phases, const vec2 *pHookPoint = nullptr) {
		if(m_PrismAssistPathCount >= PrismAssist::MAX_CANDIDATES)
			return;
		auto &Path = m_aPrismAssistPaths[m_PrismAssistPathCount];
		if(m_PrismPredictor.Simulate(Input, pActions, Phases, Horizon, Path))
		{
			Path.m_Direction = pActions[0].m_Direction;
			Path.m_Jump = pActions[0].m_Jump;
			Path.m_HookPoint = pHookPoint ? *pHookPoint : vec2(0, 0);
			++m_PrismAssistPathCount;
		}
	};
	auto Simulate = [&](int Direction, bool Jump, const vec2 *pHookPoint = nullptr) {
		PrismAssist::SAction Action{Direction, Jump, Horizon};
		if(pHookPoint)
		{
			Action.m_Hook = true;
			Action.m_HookAim = *pHookPoint - Local.m_Pos;
		}
		SimulateActions(&Action, 1, pHookPoint);
	};
	Simulate(Input.m_Direction, Input.m_Jump != 0);
	if(m_PrismAssistPathCount == 0 || m_aPrismAssistPaths[0].Safe() || m_aPrismAssistPaths[0].m_Unknown || g_Config.m_PrismFreezeAvoid != 2)
	{
		m_PrismAvoidLastDirection = 0;
		m_PrismAvoidHookOwned = false;
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
	// Search only after a concrete hazard. Raycasts use the same hook collision
	// primitive as DDNet's core; reject teleports, blockers and duplicate faces.
	// Retain an active point first so a useful recovery is not retargeted each tick.
	if(!MacroHookOwned && (Local.m_HookState == HOOK_IDLE || m_PrismAvoidHookOwned))
	{
		std::array<vec2, PrismAssist::MAX_HOOK_CANDIDATES> aPoints{};
		int Points = 0;
		auto AddPoint = [&](vec2 Point) {
			if(Points >= (int)aPoints.size() || distance(Local.m_Pos, Point) < 48.0f)
				return;
			for(int i = 0; i < Points; ++i)
				if(distance(aPoints[i], Point) < 20.0f)
					return;
			aPoints[Points++] = Point;
		};
		if(m_PrismAvoidHookOwned && Local.m_HookState == HOOK_GRABBED && Local.HookedPlayer() == -1)
			AddPoint(Local.m_HookPos);
		else if(m_PrismAvoidHookOwned)
			AddPoint(m_PrismAvoidHookPoint);
		const int Travel = ManualDirection ? ManualDirection : Local.m_Vel.x < 0 ? -1 : 1;
		constexpr float aAngles[] = {0, -pi / 6, pi / 6, -pi / 3, pi / 3, -pi / 2, pi / 2, -2 * pi / 3, 2 * pi / 3, pi};
		for(float Angle : aAngles)
		{
			if(Points >= (int)aPoints.size())
				break;
			const vec2 Ray(std::cos(Angle) * Travel, std::sin(Angle));
			vec2 Hit, Before;
			int Tele = 0;
			const int Tile = Collision()->IntersectLineTeleHook(Local.m_Pos,
				Local.m_Pos + Ray * (float)Local.m_Tuning.m_HookLength, &Hit, &Before, &Tele);
			if(PrismAssist::HookableImpact(Tile, Tele))
				AddPoint(Hit);
		}
		m_PrismAvoidHookCandidateCount = Points;
		for(int i = 0; i < Points; ++i)
			Simulate(Input.m_Direction, Input.m_Jump != 0, &aPoints[i]);
		// A useful hook can pull through the narrow section and be released
		// before the end of the same prediction horizon.
		for(int i = 0; i < std::min(Points, 2) && Horizon > 8; ++i)
		{
			PrismAssist::SAction aPhases[2] = {{Input.m_Direction, Input.m_Jump != 0, 8, true, aPoints[i] - Local.m_Pos},
				{Input.m_Direction, false, Horizon - 8, false, vec2(0, 0)}};
			SimulateActions(aPhases, 2, &aPoints[i]);
		}
	}
	for(int i = 1; i < m_PrismAssistPathCount; ++i)
		if(m_aPrismAssistPaths[i].Safe() && (!m_aPrismAssistPaths[i].m_Hook || m_aPrismAssistPaths[i].m_HookAttached))
		{
			const bool Route = PrismAssist::PreservesRoute(m_aPrismAssistPaths[0], m_aPrismAssistPaths[i], Travel);
			if(Route && m_PrismAvoidRouteCandidate < 0)
				m_PrismAvoidRouteCandidate = i;
			if(!Route && m_PrismAvoidEmergencyCandidate < 0)
				m_PrismAvoidEmergencyCandidate = i;
		}
	auto Correction = PrismAssist::SelectCorrection(m_aPrismAssistPaths.data(), m_PrismAssistPathCount, ManualDirection, ManualJump, 2, MacroDirectionOwned, Travel);
	if(Correction.m_Apply && !Correction.m_Hook && m_PrismAvoidLastDirection && Correction.m_Direction != m_PrismAvoidLastDirection)
		for(int i = 1; i < m_PrismAssistPathCount; ++i)
			if(!m_aPrismAssistPaths[i].m_Hook && m_aPrismAssistPaths[i].Safe() && m_aPrismAssistPaths[i].m_Direction == m_PrismAvoidLastDirection &&
				m_aPrismAssistPaths[i].m_Jump == Correction.m_Jump &&
				PrismAssist::PreservesRoute(m_aPrismAssistPaths[0], m_aPrismAssistPaths[i], Travel) == Correction.m_RoutePreserving &&
				PrismAssist::RouteProgress(m_aPrismAssistPaths[i], Travel) >=
					PrismAssist::RouteProgress(m_aPrismAssistPaths[Correction.m_Selected], Travel) - 16.0f)
			{
				Correction.m_Direction = m_PrismAvoidLastDirection;
				Correction.m_Selected = i;
				break;
			}
	if(Correction.m_Apply && m_PrismAvoidHookOwned &&
		(Correction.m_Hook || Correction.m_Direction != Input.m_Direction))
		for(int i = 1; i < m_PrismAssistPathCount; ++i)
			if(m_aPrismAssistPaths[i].Safe() && m_aPrismAssistPaths[i].m_HookAttached &&
				distance(m_aPrismAssistPaths[i].m_HookPoint, m_PrismAvoidHookPoint) < 20.0f &&
				m_aPrismAssistPaths[i].m_Direction == Input.m_Direction &&
				PrismAssist::PreservesRoute(m_aPrismAssistPaths[0], m_aPrismAssistPaths[i], Travel) == Correction.m_RoutePreserving &&
				PrismAssist::RouteProgress(m_aPrismAssistPaths[i], Travel) >=
					PrismAssist::RouteProgress(m_aPrismAssistPaths[Correction.m_Selected], Travel) - 16.0f)
			{
				Correction = {true, m_aPrismAssistPaths[i].m_Direction, m_aPrismAssistPaths[i].m_Jump, i,
					true, m_aPrismAssistPaths[i].m_HookPoint, Correction.m_RoutePreserving};
				break;
			}
	if(Correction.m_Apply && !m_PrismAvoidHookOwned && m_aPrismAssistPaths[0].m_FirstHazard > 1)
	{
		const auto &Chosen = m_aPrismAssistPaths[Correction.m_Selected];
		if(Chosen.m_Phases + 1 <= PrismAssist::MAX_PHASES)
		{
			PrismAssist::SAction aDeferred[PrismAssist::MAX_PHASES]{};
			aDeferred[0] = {Input.m_Direction, Input.m_Jump != 0, 1};
			for(int i = 0; i < Chosen.m_Phases; ++i)
				aDeferred[i + 1] = Chosen.m_aActions[i];
			PrismAssist::STrajectory Deferred;
			if(m_PrismPredictor.Simulate(Input, aDeferred, Chosen.m_Phases + 1, Horizon, Deferred) &&
				PrismAssist::CanDeferCorrection(m_aPrismAssistPaths[0], Chosen, Deferred))
			{
				m_PrismAvoidDeferred = true;
				m_PrismAvoidLastDirection = 0;
				return;
			}
		}
	}
	if(Correction.m_Apply)
	{
		Input.m_Direction = Correction.m_Direction;
		Input.m_Jump = Correction.m_Jump;
		m_PrismAssistSelected = Correction.m_Selected;
		m_PrismAvoidLastDirection = Correction.m_Direction;
		if(Correction.m_Jump && !ManualJump)
			m_PrismAvoidJumpCooldown = 6;
		if(Correction.m_Hook)
		{
			m_PrismAvoidHookOwned = true;
			m_PrismAvoidHookPoint = Correction.m_HookPoint;
			Input.m_Hook = 1;
			const vec2 Aim = Correction.m_HookPoint - Local.m_Pos;
			Input.m_TargetX = round_to_int(Aim.x);
			Input.m_TargetY = round_to_int(Aim.y);
		}
		else
			m_PrismAvoidHookOwned = false;
	}
	else
	{
		m_PrismAvoidLastDirection = 0;
		m_PrismAvoidHookOwned = false;
	}
	return;
}

void CGameClient::PrismRenderAssistDebug()
{
	if(!PrismInputAllowed() || (!g_Config.m_PrismFreezeDebug && !g_Config.m_PrismHookDebug && g_Config.m_PrismFreezeAvoid != 1))
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
				i == m_PrismAssistSelected && i > 0 && !m_PrismAvoidDeferred ? Accent.WithAlpha(0.95f) :
				i == m_PrismAvoidRouteCandidate ? ColorRGBA(0.30f, 0.91f, 0.70f, 0.65f) :
				i == m_PrismAvoidEmergencyCandidate ? ColorRGBA(1.0f, 0.65f, 0.30f, 0.50f) : Accent.WithAlpha(i ? 0.22f : 0.48f));
			const IGraphics::CLineItem Line(Path.m_aStates[j - 1].m_Pos, Path.m_aStates[j].m_Pos);
			Graphics()->LinesDraw(&Line, 1);
		}
		if(g_Config.m_PrismFreezeDebug && Path.m_Hook && Path.m_Count)
		{
			Graphics()->SetColor(i == m_PrismAssistSelected && m_PrismAvoidHookOwned ? Accent.WithAlpha(1) : ColorRGBA(0.48f, 0.82f, 1, 0.55f));
			const vec2 Point = Path.m_HookPoint;
			const IGraphics::CLineItem aPoint[] = {{Point + vec2(-4, -4), Point + vec2(4, 4)}, {Point + vec2(-4, 4), Point + vec2(4, -4)}};
			Graphics()->LinesDraw(aPoint, 2);
			if(i == m_PrismAssistSelected && m_PrismAvoidHookOwned)
			{
				const IGraphics::CLineItem Hook(Path.m_aStates[0].m_Pos, Point);
				Graphics()->LinesDraw(&Hook, 1);
			}
		}
		if(g_Config.m_PrismFreezeDebug && Path.m_Count && i == 0)
		{
			const vec2 P = Path.m_aStates[Path.m_FirstHazard >= 0 ? Path.m_FirstHazard : Path.m_Count - 1].m_Pos;
			Graphics()->SetColor(Path.m_Unknown ? ColorRGBA(1, 0.75f, 0.24f, 1) :
				Path.m_FirstHazard >= 0 ? ColorRGBA(1, 0.28f, 0.20f, 1) : Accent.WithAlpha(1));
			const IGraphics::CLineItem aCross[] = {{P + vec2(-4, 0), P + vec2(4, 0)}, {P + vec2(0, -4), P + vec2(0, 4)}};
			Graphics()->LinesDraw(aCross, 2);
			if(m_PrismAvoidInferredDirection)
			{
				Graphics()->SetColor(ColorRGBA(0.92f, 0.93f, 0.96f, 0.80f));
				const vec2 Origin = Path.m_aStates[0].m_Pos + vec2(0, -20);
				const float Sign = (float)m_PrismAvoidInferredDirection;
				const IGraphics::CLineItem aArrow[] = {{Origin, Origin + vec2(32 * Sign, 0)},
					{Origin + vec2(32 * Sign, 0), Origin + vec2(24 * Sign, -5)},
					{Origin + vec2(32 * Sign, 0), Origin + vec2(24 * Sign, 5)}};
				Graphics()->LinesDraw(aArrow, 3);
			}
		}
	}
	if(g_Config.m_PrismHookDebug && m_PrismHookDebugReady)
	{
		const vec2 Origin = m_PrismHookDebugOrigin;
		const vec2 Direction = normalize(m_PrismHookDebugDirection);
		const float Range = (float)g_Config.m_PrismHookRange;
		Graphics()->SetColor(ColorRGBA(0.71f, 0.78f, 0.86f, 0.65f));
		const IGraphics::CLineItem Manual(Origin, Origin + Direction * Range);
		Graphics()->LinesDraw(&Manual, 1);
		if(m_PrismManualHookHeld)
		{
			const float Angle = std::atan2(Direction.y, Direction.x);
			for(int Sign : {-1, 1})
			{
				const float A = Angle + Sign * g_Config.m_PrismHookFov * pi / 360.0f;
				const vec2 End = Origin + vec2(std::cos(A), std::sin(A)) * Range;
				Graphics()->SetColor(Accent.WithAlpha(0.30f));
				const IGraphics::CLineItem Edge(Origin, End);
				Graphics()->LinesDraw(&Edge, 1);
			}
			if(m_PrismHookTargetId >= 0)
			{
				Graphics()->SetColor(Accent.WithAlpha(0.95f));
				for(const vec2 Offset : {vec2(-5.0f, -5.0f), vec2(-5.0f, 5.0f)})
				{
					const IGraphics::CLineItem Cross(m_PrismHookPredictedPos + Offset, m_PrismHookPredictedPos - Offset);
					Graphics()->LinesDraw(&Cross, 1);
				}
				const IGraphics::CLineItem Corrected(Origin, Origin + normalize(m_PrismHookOutput) * Range);
				Graphics()->LinesDraw(&Corrected, 1);
			}
		}
	}
	Graphics()->LinesEnd();
	Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	if(g_Config.m_PrismHookDebug && m_PrismHookDebugReady)
	{
		char aStatus[96];
		str_format(aStatus, sizeof(aStatus), "Hook Assist: %s  target: %d  eligible: %d",
			!m_PrismManualHookHeld ? "INACTIVE (hold Hook)" : m_PrismHookActive ? "CORRECTING" : "NO TARGET",
			m_PrismHookTargetId, m_PrismHookCandidateCount);
		TextRender()->Text(m_PrismHookDebugOrigin.x - 55, m_PrismHookDebugOrigin.y - 75, 9.0f, aStatus);
	}
	if(g_Config.m_PrismFreezeDebug && m_PrismAssistPathCount)
	{
		const auto &Base = m_aPrismAssistPaths[0];
		char aStatus[192];
		str_format(aStatus, sizeof(aStatus), "Freeze: %s%s  %d ticks  route %d emergency %d  %s  hook %d  own D%d J%d H%d A%d",
			PrismAssist::DebugStatus(Base),
			Base.Safe() && Base.m_MinSafetyMargin < 16.0f ? " (CLOSE)" : "",
			Base.m_Count - 1, m_PrismAvoidRouteCandidate, m_PrismAvoidEmergencyCandidate,
			m_PrismAvoidDeferred ? "WAIT" : m_PrismAssistSelected ? "CORRECT" : "MANUAL", m_PrismAvoidHookCandidateCount,
			m_PrismAssistSelected > 0 && m_aPrismAssistPaths[m_PrismAssistSelected].m_Direction != Base.m_Direction,
			m_PrismAssistSelected > 0 && m_aPrismAssistPaths[m_PrismAssistSelected].m_Jump != Base.m_Jump,
			m_PrismAvoidHookOwned, m_PrismAvoidHookOwned);
		TextRender()->Text(Base.m_aStates[0].m_Pos.x - 55, Base.m_aStates[0].m_Pos.y - 62, 9.0f, aStatus);
	}
}
