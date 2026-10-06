// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PATHFINDER_CONTROLLER_H
#define GAME_CLIENT_PATHFINDER_CONTROLLER_H
#include "search.h"

#include <string>

namespace PrismPath
{
	class CController
	{
		CMapAnalysis m_Map;
		CNavigation m_Navigation;
		CPhysicsSearch m_Search;
		CSimulator m_Simulator;
		CGameWorld m_VerificationWorld;
		SPlan m_Plan;
		SStats m_Stats, m_SearchStats;
		SState m_Observed;
		SSettings m_Settings;
		CNetObj_PlayerInput m_Output{};
		EStatus m_Status = EStatus::IDLE;
		std::string m_Reason = "disabled";
		std::vector<vec2> m_vGoals, m_vRoute;
		int m_LastTick = -1, m_PlanStartTick = -1, m_Id = -1, m_StallTick = -1, m_Failures = 0;
		float m_StartCost = 0, m_BestCost = UNREACHABLE;
		bool m_Enabled = false, m_Paused = false, m_Owned = false, m_ReleasePending = false;
		bool m_Autopilot = false;
		bool m_StartVisited = false, m_SeekingStart = false;
		const bool m_UseWallClock;
		bool Adopt(CGameWorld &World, const SPlan &Plan, int64_t DeadlineUs);
		bool SafeHold(CGameWorld &World, int64_t DeadlineUs);
		void AccumulateStats();
		void ClearPlan();

	public:
		// Tests may use deterministic work limits; production always uses wall-clock budgets.
		explicit CController(bool UseWallClock = true) :
			m_UseWallClock(UseWallClock) {}
		void Reset(bool ClearMap = true);
		void ResetRaceStart();
		bool SeekingStart() const { return m_SeekingStart; }
		void Stop(const char *pReason = "disabled");
		void Pause(const char *pReason = "paused");
		void Resume();
		void Replan(const char *pReason = "requested");
		void SetGoals(const std::vector<vec2> &Goals);
		// Returns exclusive input ownership, including one release snapshot after stopping.
		bool Compose(CGameWorld *pWorld, int Id, CNetObj_PlayerInput &Input, bool Enabled, bool Allowed,
			bool ManualActivity, const SSettings &Settings);
		EStatus Status() const { return m_Status; }
		const char *Reason() const { return m_Reason.c_str(); }
		bool Owned() const { return m_Owned; }
		bool Paused() const { return m_Paused; }
		bool Enabled() const { return m_Enabled; }
		bool PendingRelease() const { return m_ReleasePending; }
		const SState &Observed() const { return m_Observed; }
		const SPlan &Plan() const { return m_Plan; }
		const SStats &Stats() const { return m_Stats; }
		const CPhysicsSearch &Search() const { return m_Search; }
		const CMapAnalysis &Map() const { return m_Map; }
		const CNavigation &Navigation() const { return m_Navigation; }
		const std::vector<vec2> &Route() const { return m_vRoute; }
		vec2 Target() const { return m_Navigation.LocalTarget(m_Observed.m_Pos); }
		float Progress() const;
	};
}
#endif
