#include "controller.h"

#include <game/collision.h>

#include <algorithm>

namespace PrismPath
{
	void CController::AccumulateStats()
	{
		const SStats &S = m_Search.Stats();
#define PRISM_ACCUMULATE(Field) m_Stats.Field += S.Field - m_SearchStats.Field
		PRISM_ACCUMULATE(m_Expanded);
		PRISM_ACCUMULATE(m_Pruned);
		PRISM_ACCUMULATE(m_Simulations);
		PRISM_ACCUMULATE(m_Timeouts);
		PRISM_ACCUMULATE(m_Dead);
		PRISM_ACCUMULATE(m_Frozen);
		PRISM_ACCUMULATE(m_Unsupported);
#undef PRISM_ACCUMULATE
		m_SearchStats = S;
	}
	void CController::ClearPlan()
	{
		AccumulateStats();
		m_Search.Reset();
		m_SearchStats = {};
		m_Plan = {};
		m_PlanStartTick = -1;
		m_LastTick = -1;
		ReleaseInput(m_Output);
	}
	void CController::Reset(bool ClearMap)
	{
		Stop("client reset");
		m_Paused = false;
		if(ClearMap)
		{
			m_Map.Reset();
			m_Navigation.Reset();
			m_vRoute.clear();
		}
		m_Id = -1;
		m_StallTick = -1;
		m_BestCost = UNREACHABLE;
		m_StartCost = 0;
	}
	void CController::Stop(const char *pReason)
	{
		m_ReleasePending |= m_Owned;
		m_Owned = m_Enabled = false;
		m_Status = EStatus::IDLE;
		m_Reason = pReason;
		ClearPlan();
	}
	void CController::Pause(const char *pReason)
	{
		m_ReleasePending |= m_Owned;
		m_Owned = false;
		m_Paused = true;
		m_Status = EStatus::PAUSED;
		m_Reason = pReason;
		ClearPlan();
	}
	void CController::Resume()
	{
		m_Paused = false;
		m_Failures = 0;
		Replan("resumed");
	}
	void CController::Replan(const char *pReason)
	{
		ClearPlan();
		++m_Stats.m_Replans;
		m_Reason = pReason;
		m_Status = EStatus::REPLANNING;
	}
	void CController::SetGoals(const std::vector<vec2> &Goals)
	{
		if(Goals == m_vGoals)
			return;
		m_vGoals = Goals;
		m_Navigation.Reset();
		m_vRoute.clear();
		m_StartCost = 0;
		Replan("destination changed");
	}
	bool CController::Adopt(CGameWorld &World, const SPlan &Candidate, int64_t Deadline)
	{
		if(!Candidate.Valid())
			return false;
		// A search snapshot may be several ticks old. Align to the nearest physical
		// state, then re-simulate the committed prefix from the CURRENT world.
		int Offset = 0;
		float Best = distance(Candidate.m_aStates[0].m_Pos, m_Observed.m_Pos) + distance(Candidate.m_aStates[0].m_Vel, m_Observed.m_Vel) * 4;
		for(int i = 1; i < Candidate.m_Ticks - 4; ++i)
		{
			const float D = distance(Candidate.m_aStates[i].m_Pos, m_Observed.m_Pos) + distance(Candidate.m_aStates[i].m_Vel, m_Observed.m_Vel) * 4;
			if(D < Best)
			{
				Best = D;
				Offset = i;
			}
		}
		CSimulator::Clone(m_VerificationWorld, World);
		m_Simulator.Init(m_Id, World.GetCharacterById(m_Id)->Core()->m_Input.m_Fire);
		SPlan Verified;
		Verified.m_aStates[0] = m_Observed;
		for(int i = Offset; i < Candidate.m_Ticks && Verified.m_Ticks < 32; ++i)
		{
			if(Deadline && NowUs() >= Deadline)
				return false;
			EReject Reject;
			SState State;
			if(!m_Simulator.Step(m_VerificationWorld, Candidate.m_aInputs[i], State, Reject))
				return false;
			Verified.m_aInputs[Verified.m_Ticks] = Candidate.m_aInputs[i];
			Verified.m_aStates[++Verified.m_Ticks] = State;
		}
		if(!Verified.Valid() || m_Navigation.Cost(Verified.m_aStates[Verified.m_Ticks].m_Pos) >= m_Navigation.Cost(m_Observed.m_Pos) + 64)
			return false;
		Verified.m_UsesHook = Candidate.m_UsesHook;
		Verified.m_HookPoint = Candidate.m_HookPoint;
		Verified.m_HasLanding = Candidate.m_HasLanding;
		Verified.m_Landing = Candidate.m_Landing;
		Verified.m_Score = Candidate.m_Score;
		m_Plan = Verified;
		m_PlanStartTick = World.GameTick();
		m_Status = m_Autopilot ? EStatus::EXECUTING : EStatus::PLANNING;
		m_Reason = "verified physics trajectory";
		AccumulateStats();
		m_Search.Reset();
		m_SearchStats = {};
		++m_Stats.m_Plans;
		m_Failures = 0;
		return true;
	}
	bool CController::SafeHold(CGameWorld &World, int64_t Deadline)
	{
		m_Simulator.Init(m_Id, World.GetCharacterById(m_Id)->Core()->m_Input.m_Fire);
		float Best = -UNREACHABLE;
		CNetObj_PlayerInput Chosen{};
		const bool Attached = m_Observed.m_HookState == HOOK_GRABBED || m_Observed.m_HookState == HOOK_FLYING;
		for(int Direction : {0, -1, 1})
			for(bool Hook : {Attached, false})
			{
				if(Deadline && NowUs() >= Deadline)
					break;
				const vec2 Aim = m_Observed.m_HookState == HOOK_FLYING ? m_Observed.m_HookDir * 256 : m_Observed.m_HookPos - m_Observed.m_Pos;
				SAction Action{Direction, 12, false, Hook, m_Observed.m_HookPos, Aim};
				STrace Trace;
				if(!m_Simulator.Simulate(World, Action, Trace, Deadline))
					continue;
				const SState &End = Trace.m_aStates[Trace.m_Count - 1];
				// Recovery first keeps a stable state, then retains route/momentum.
				const float Cost = m_Navigation.Ready() ? m_Navigation.Cost(End.m_Pos) : distance(End.m_Pos, m_Observed.m_Pos);
				const float Score = -std::min(Cost, 100000.0f) - length(End.m_Vel) + (End.m_Grounded ? 20 : 0) - (Direction ? 3 : 0);
				if(Score > Best)
				{
					Best = Score;
					Chosen = Action.Input();
				}
			}
		if(Best == -UNREACHABLE)
			return false;
		m_Output = Chosen;
		return true;
	}
	float CController::Progress() const
	{
		if(m_Status == EStatus::FINISHED)
			return 1;
		if(m_Status == EStatus::ANALYZING)
			return m_Map.Progress();
		if(m_StartCost <= 1 || m_BestCost >= UNREACHABLE)
			return 0;
		return std::clamp(1 - m_BestCost / m_StartCost, 0.0f, 1.0f);
	}
	bool CController::Compose(CGameWorld *pWorld, int Id, CNetObj_PlayerInput &Input, bool Enabled, bool Allowed,
		bool ManualActivity, const SSettings &Settings)
	{
		auto Release = [&] {
			if(!m_ReleasePending)
				return false;
			ReleaseInput(Input);
			m_ReleasePending = false;
			return true;
		};
		if(!Enabled)
		{
			if(m_Enabled || m_Owned)
			{
				Stop();
				m_Paused = false;
			}
			return Release();
		}
		if(!m_Enabled)
		{
			m_Enabled = true;
			m_Paused = false;
			m_Status = EStatus::ANALYZING;
			m_Failures = 0;
			m_BestCost = UNREACHABLE;
			m_StartCost = 0;
		}
		if(Settings.m_Autopilot != m_Autopilot)
		{
			m_ReleasePending |= m_Owned;
			m_Owned = false;
			m_Autopilot = Settings.m_Autopilot;
			Replan("mode changed");
		}
		if(Settings.m_SafeRoutes != m_Settings.m_SafeRoutes)
		{
			m_Navigation.Reset();
			Replan("route preference changed");
		}
		else if(Settings.m_Horizon != m_Settings.m_Horizon || Settings.m_Momentum != m_Settings.m_Momentum)
			Replan("search settings changed");
		m_Settings = Settings;
		if(!Allowed || !pWorld || Id < 0 || !pWorld->GetCharacterById(Id))
		{
			if(m_Owned)
				Pause(!Allowed ? "menu / focus / spectator" : "death / disconnect");
			return Release();
		}
		if(m_Paused)
			return Release();
		if(m_Autopilot && ManualActivity && Settings.m_ManualOverride)
		{
			Pause("manual input override");
			// Return ownership to physical input on this very snapshot.
			m_ReleasePending = false;
			return false;
		}
		if(m_Id != Id)
		{
			m_Id = Id;
			Replan("controlled Tee changed");
		}
		if(const char *pReason = CSimulator::UnsupportedWorld(*pWorld, Id))
		{
			m_Reason = pReason;
			m_Status = EStatus::NO_ROUTE;
			m_ReleasePending |= m_Owned;
			m_Owned = false;
			ClearPlan();
			return Release();
		}
		m_Observed = SState::Read(*pWorld, Id);
		if(m_Observed.m_FreezeTime || m_Observed.m_DeepFrozen || m_Observed.m_LiveFrozen)
		{
			Pause("frozen: resume after recovery");
			return Release();
		}
		if(Release())
			return true;
		m_Owned = m_Autopilot;
		const int Tick = pWorld->GameTick();
		if(Tick != m_LastTick)
		{
			const int64_t Start = NowUs();
			const int64_t Deadline = Start + std::clamp(Settings.m_BudgetUs, 500, 8000);
			m_LastTick = Tick;
			if(!m_Map.Collision())
				m_Map.Begin(*pWorld->Collision());
			if(m_Map.Failed())
			{
				Pause("map analysis size limit");
				return Release();
			}
			if(!m_Map.Ready())
			{
				m_Status = EStatus::ANALYZING;
				m_Reason = "extracting geometry / regions / hook faces";
				m_Map.Step(2048, Deadline - 400);
				ReleaseInput(m_Output);
				if(m_Autopilot && !SafeHold(*pWorld, Deadline))
				{
					Pause("no safe continuation during map analysis");
					m_Status = EStatus::NO_ROUTE;
					return Release();
				}
			}
			else
			{
				if(!m_Navigation.Ready())
				{
					if(m_Status == EStatus::ANALYZING || m_Status == EStatus::REPLANNING)
					{
						std::vector<vec2> Goals = m_vGoals;
						if(Goals.empty())
							for(int Index : m_Map.Finishes())
								Goals.push_back(m_Map.Collision()->GetPos(Index));
						if(!m_Navigation.Begin(m_Map, Goals, Settings.m_SafeRoutes))
						{
							Pause("no reachable finish: set a manual destination");
							m_Status = EStatus::NO_ROUTE;
							return Release();
						}
						m_Status = EStatus::PLANNING;
						m_Reason = "global region routing";
					}
					m_Navigation.Step(2048, Deadline - 400);
					ReleaseInput(m_Output);
					if(m_Autopilot && !m_Navigation.Ready() && !SafeHold(*pWorld, Deadline))
					{
						Pause("no safe continuation during route analysis");
						m_Status = EStatus::NO_ROUTE;
						return Release();
					}
				}
				if(m_Navigation.Ready())
				{
					const float Cost = m_Navigation.Cost(m_Observed.m_Pos);
					if(Cost >= UNREACHABLE)
					{
						Pause("no connected region route");
						m_Status = EStatus::NO_ROUTE;
						return Release();
					}
					if(!m_StartCost)
						m_StartCost = Cost;
					if(Cost < m_BestCost - 16)
					{
						m_BestCost = Cost;
						m_StallTick = Tick;
					}
					if(m_Navigation.AtGoal(m_Observed.m_Pos))
					{
						Pause("destination reached");
						m_Status = EStatus::FINISHED;
						return Release();
					}
					int Offset = m_Plan.Valid() ? Tick - m_PlanStartTick : -1;
					if(Offset >= 0 && Offset < m_Plan.m_Ticks && Diverged(m_Plan.m_aStates[Offset], m_Observed, Settings.m_ReplanDistance))
					{
						Replan("physics divergence / missed hook");
						m_Status = EStatus::RECOVERING;
						Offset = -1;
					}
					if(Offset < 0 || Offset >= m_Plan.m_Ticks - 4 || (!m_Autopilot && Tick - m_PlanStartTick >= 5))
					{
						if(!m_Search.Active() && !m_Search.Complete())
						{
							m_SearchStats = {};
							if(!m_Search.Begin(*pWorld, Id, m_Map, m_Navigation, Settings))
							{
								Pause("invalid search start");
								m_Status = EStatus::NO_ROUTE;
								return Release();
							}
							m_vRoute = m_Navigation.Route(m_Observed.m_Pos);
							m_Reason = "searching physical states";
						}
						m_Search.Step(Deadline - 500, 256);
						AccumulateStats();
						if(m_Search.Complete() || (m_Search.Stats().m_Simulations >= 256 && m_Search.Result().Valid()))
						{
							const SPlan Candidate = m_Search.Result();
							if(Adopt(*pWorld, Candidate, Deadline))
								Offset = 0;
							else
							{
								ClearPlan();
								++m_Failures;
								++m_Stats.m_Replans;
								m_Reason = Candidate.Valid() ? "stale candidate rejected: replan" : "physics search exhausted";
								if(m_Failures >= 6)
								{
									Pause(m_Reason.c_str());
									m_Status = EStatus::NO_ROUTE;
									return Release();
								}
							}
						}
					}
					if(Offset >= 0 && Offset < m_Plan.m_Ticks)
					{
						m_Output = m_Plan.m_aInputs[Offset];
						m_Status = m_Autopilot ? EStatus::EXECUTING : EStatus::PLANNING;
					}
					else if(m_Autopilot)
					{
						if(!SafeHold(*pWorld, Deadline))
						{
							Pause("no safe recovery continuation");
							m_Status = EStatus::NO_ROUTE;
							return Release();
						}
						m_Status = EStatus::RECOVERING;
					}
					if(m_Autopilot && m_StallTick >= 0 && Tick - m_StallTick > SERVER_TICK_SPEED * 8)
					{
						Pause("no route progress for 8 seconds");
						m_Status = EStatus::NO_ROUTE;
						return Release();
					}
				}
			}
			m_Stats.m_LastUs = double(NowUs() - Start);
			m_Stats.m_WorstUs = std::max(m_Stats.m_WorstUs, m_Stats.m_LastUs);
			m_LastTick = Tick;
		}
		if(!m_Owned)
			return false;
		const int Fire = Input.m_Fire, Flags = Input.m_PlayerFlags;
		Input = m_Output;
		Input.m_PlayerFlags = Flags;
		Input.m_Fire = (Fire + (Fire & 1)) & INPUT_STATE_MASK;
		return true;
	}
}
