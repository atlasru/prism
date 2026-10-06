// Prism additions, distributed under the zlib license in license.txt.
#include "gameclient.h"

#include <base/log.h>

#include <engine/graphics.h>
#include <engine/textrender.h>

bool CGameClient::PrismComposePathfinder(CNetObj_PlayerInput &Input, const CNetObj_PlayerInput &Manual, bool ManualActivity)
{
	if(!g_Config.m_PrismSoloEnabled && !m_PrismSolo.Enabled() && !m_PrismSolo.PendingRelease())
		return false;
	PrismPath::SSettings Settings;
	Settings.m_Autopilot = g_Config.m_PrismSoloMode == 1;
	Settings.m_BudgetUs = g_Config.m_PrismSoloBudget;
	Settings.m_Horizon = g_Config.m_PrismSoloHorizon;
	Settings.m_ReplanDistance = g_Config.m_PrismSoloThreshold;
	Settings.m_SafeRoutes = g_Config.m_PrismSoloSafe;
	Settings.m_Momentum = g_Config.m_PrismSoloMomentum;
	Settings.m_ManualOverride = g_Config.m_PrismSoloOverride;
	Settings.m_RaceStarted = m_Snap.m_pGameInfoObj && (m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME);
	if(g_Config.m_PrismSoloEnabled)
	{
		if(g_Config.m_PrismSoloTarget)
			m_PrismSolo.SetGoals({vec2(g_Config.m_PrismSoloTargetX, g_Config.m_PrismSoloTargetY)});
		else
			m_PrismSolo.SetGoals({});
	}
	CGameWorld *pWorld = nullptr;
	if(g_Config.m_PrismSoloEnabled && m_Snap.m_LocalClientId >= 0 && m_Snap.m_pLocalCharacter)
		pWorld = m_PredictedWorld.GetCharacterById(m_Snap.m_LocalClientId) ? &m_PredictedWorld : &m_GameWorld;
	const bool Owned = m_PrismSolo.Compose(pWorld, m_Snap.m_LocalClientId, Input, g_Config.m_PrismSoloEnabled,
		PrismInputAllowed(), ManualActivity, Settings);
	if(Owned)
	{
		m_PrismMacros.Cancel();
		m_PrismAvoidHookOwned = false;
		m_PrismHookActive = false;
		m_PrismHookTargetId = -1;
		m_PrismAssistPathCount = 0;
		m_PrismWeaponDebug = m_PrismWeaponApplied = false;
	}
	if(g_Config.m_PrismSoloLog && (m_PrismSoloLoggedStatus != m_PrismSolo.Status() || m_PrismSoloLoggedReason != m_PrismSolo.Reason()))
	{
		m_PrismSoloLoggedStatus = m_PrismSolo.Status();
		m_PrismSoloLoggedReason = m_PrismSolo.Reason();
		const auto &S = m_PrismSolo.Stats();
		log_info("prism-pathfinder", "%s: %s; expanded=%llu pruned=%llu simulations=%llu death=%llu freeze=%llu unknown=%llu replans=%llu search=%.3fms pos=%.1f,%.1f",
			PrismPath::StatusName(m_PrismSolo.Status()), m_PrismSolo.Reason(), (unsigned long long)S.m_Expanded, (unsigned long long)S.m_Pruned,
			(unsigned long long)S.m_Simulations, (unsigned long long)S.m_Dead, (unsigned long long)S.m_Frozen,
			(unsigned long long)S.m_Unsupported, (unsigned long long)S.m_Replans, S.m_LastUs / 1000, m_PrismSolo.Observed().m_Pos.x, m_PrismSolo.Observed().m_Pos.y);
	}
	return Owned;
}
void CGameClient::PrismPathfinderTarget(bool Cursor)
{
	const vec2 P = Cursor ? m_Controls.m_aTargetPos[g_Config.m_ClDummy] : m_LocalCharacterPos;
	g_Config.m_PrismSoloTarget = 1;
	g_Config.m_PrismSoloTargetX = std::max(0, round_to_int(P.x));
	g_Config.m_PrismSoloTargetY = std::max(0, round_to_int(P.y));
	m_PrismSolo.SetGoals({P});
	m_PrismSolo.Replan("manual destination");
}
void CGameClient::PrismRenderPathfinder()
{
	if(!g_Config.m_PrismSoloEnabled || Client()->State() != IClient::STATE_ONLINE || !m_PrismSolo.Map().Ready())
		return;
	if(!g_Config.m_PrismSoloRoute && !g_Config.m_PrismSoloPrediction && !g_Config.m_PrismSoloHooks && !g_Config.m_PrismSoloDebug)
		return;
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_PrismThemeAccent));
	Graphics()->TextureClear();
	Graphics()->LinesBegin();
	auto Line = [&](vec2 A, vec2 B, ColorRGBA Color) {
		Graphics()->SetColor(Color);
		const IGraphics::CLineItem Item(A, B);
		Graphics()->LinesDraw(&Item, 1);
	};
	auto Cross = [&](vec2 P, float Radius, ColorRGBA Color) {
		Line(P - vec2(Radius, 0), P + vec2(Radius, 0), Color);
		Line(P - vec2(0, Radius), P + vec2(0, Radius), Color);
	};
	if(g_Config.m_PrismSoloRoute)
	{
		const auto &Route = m_PrismSolo.Route();
		for(size_t i = 1; i < Route.size() && i < 160; ++i)
			Line(Route[i - 1], Route[i], Accent.WithAlpha(0.3f));
		Cross(m_PrismSolo.Target(), 9, Accent.WithAlpha(0.9f));
	}
	const auto &Plan = m_PrismSolo.Plan();
	if(g_Config.m_PrismSoloPrediction && Plan.Valid())
	{
		for(int i = 1; i <= Plan.m_Ticks; ++i)
			Line(Plan.m_aStates[i - 1].m_Pos, Plan.m_aStates[i].m_Pos, Accent.WithAlpha(0.85f));
		if(Plan.m_UsesHook)
		{
			Cross(Plan.m_HookPoint, 6, ColorRGBA(0.55f, 0.8f, 1, 0.9f));
			Line(m_PrismSolo.Observed().m_Pos, Plan.m_HookPoint, Accent.WithAlpha(0.25f));
		}
		if(Plan.m_HasLanding)
			Cross(Plan.m_Landing, 5, ColorRGBA(0.5f, 0.95f, 0.75f, 0.8f));
	}
	const auto &Search = m_PrismSolo.Search();
	if(g_Config.m_PrismSoloHooks)
		for(int i = 0; i < Search.AnchorCount(); ++i)
			Cross(Search.Anchors()[i], 4, ColorRGBA(0.6f, 0.75f, 1, 0.55f));
	if(g_Config.m_PrismSoloDebug)
	{
		for(int i = 0; i < Search.DebugCount(); ++i)
		{
			const auto &Trace = Search.DebugTraces()[i];
			const ColorRGBA Color = Trace.Safe() ? Accent.WithAlpha(0.18f) : ColorRGBA(1, 0.35f, 0.3f, 0.3f);
			for(int j = 1; j < Trace.m_Count; ++j)
				Line(Trace.m_aStates[j - 1].m_Pos, Trace.m_aStates[j].m_Pos, Color);
		}
		const auto &S = m_PrismSolo.Observed();
		Line(S.m_Pos, S.m_Pos + S.m_Vel * 4, ColorRGBA(1, 0.8f, 0.4f, 0.8f));
	}
	Graphics()->LinesEnd();
}
void CGameClient::PrismRenderPathfinderHud(float Width, float Height)
{
	if(!g_Config.m_PrismSoloEnabled || !g_Config.m_PrismSoloHud || Client()->State() != IClient::STATE_ONLINE)
		return;
	const float Scale = g_Config.m_PrismHudScale / 100.0f;
	const float Font = std::clamp(g_Config.m_PrismHudFontSize, 5, 12) * Scale;
	const float Row = Font + 4 * Scale, Pad = 7 * Scale;
	const float W = std::min(Width, 190 * Scale), H = (g_Config.m_PrismSoloDebug ? 9 : 7) * Row + Pad * 2;
	const float X = PrismQol::HudCoordinate(g_Config.m_PrismSoloHudX, Width, W);
	const float Y = PrismQol::HudCoordinate(g_Config.m_PrismSoloHudY, Height, H);
	const float Opacity = g_Config.m_PrismHudOpacity / 100.0f;
	const ColorRGBA Accent = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_PrismThemeAccent));
	Graphics()->DrawRect(X, Y, W, H, ColorRGBA(0.09f, 0.098f, 0.106f, Opacity), IGraphics::CORNER_ALL, 6 * Scale);
	auto Text = [&](int Index, const char *pText, bool Heading = false) {
		TextRender()->TextColor(Heading ? Accent : ColorRGBA(0.94f, 0.94f, 0.95f, Opacity));
		TextRender()->Text(X + Pad, Y + Pad + Index * Row, Font, pText, W - Pad * 2);
	};
	char aText[256];
	Text(0, g_Config.m_PrismSoloMode ? "PATHFINDER / AUTOPILOT" : "PATHFINDER / ASSIST", true);
	str_format(aText, sizeof(aText), "Target: %s   State: %s", g_Config.m_PrismSoloTarget ? "Manual" : "Finish", PrismPath::StatusName(m_PrismSolo.Status()));
	Text(1, aText);
	str_format(aText, sizeof(aText), "Stage: %s", m_PrismSolo.SeekingStart() ? "Reach race start" : m_PrismSolo.Map().RegionName(m_PrismSolo.Map().RegionAt(m_PrismSolo.Observed().m_Pos)));
	Text(2, aText);
	str_format(aText, sizeof(aText), "Progress: %.0f%%   Search: %.2f ms", m_PrismSolo.Progress() * 100, m_PrismSolo.Stats().m_LastUs / 1000);
	Text(3, aText);
	Text(4, m_PrismSolo.Reason());
	const auto &Plan = m_PrismSolo.Plan();
	if(Plan.Valid())
	{
		const auto &Input = Plan.m_aInputs[0];
		str_format(aText, sizeof(aText), "Next: %s%s%s", Input.m_Direction < 0 ? "Left" : Input.m_Direction > 0 ? "Right" :
															  "Hold",
			Input.m_Jump ? " / jump" : "", Input.m_Hook ? " / hook" : " / release");
		Text(5, aText);
	}
	else
		Text(5, "Next: awaiting safe trajectory");
	Text(6, "F12: emergency stop");
	if(g_Config.m_PrismSoloDebug)
	{
		const auto &S = m_PrismSolo.Observed();
		const auto &Stats = m_PrismSolo.Stats();
		str_format(aText, sizeof(aText), "v %.1f,%.1f  jumps %d/%d  hook %d", S.m_Vel.x, S.m_Vel.y, S.m_JumpedTotal, S.m_Jumps, S.m_HookState);
		Text(7, aText);
		str_format(aText, sizeof(aText), "Expanded %llu  pruned %llu  replans %llu", (unsigned long long)Stats.m_Expanded,
			(unsigned long long)Stats.m_Pruned, (unsigned long long)Stats.m_Replans);
		Text(8, aText);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
