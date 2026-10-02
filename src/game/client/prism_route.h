// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_ROUTE_H
#define GAME_CLIENT_PRISM_ROUTE_H

#include "prism_assist.h"

#include <base/time.h>

namespace PrismRoute
{
	constexpr int MAX_SAMPLES = 32;
	constexpr int MAX_SEARCH = 16;
	constexpr int MAX_SIMULATED_TICKS = 2400;
	enum class EMode
	{
		NONE,
		CONTINUE,
		REJOIN,
		EMERGENCY,
		BUDGET,
		UNKNOWN
	};
	inline const char *ModeName(EMode Mode)
	{
		static constexpr const char *s_apNames[] = {"NONE", "CONTINUE", "REJOIN", "EMERGENCY", "BUDGET", "UNKNOWN"};
		return s_apNames[static_cast<int>(Mode)];
	}
	struct SIntent
	{
		vec2 m_Pos = vec2(0, 0), m_Vel = vec2(0, 0), m_Aim = vec2(1, 0), m_RecentMotion = vec2(0, 0);
		int m_Direction = 0, m_RecentDirection = 0;
		bool m_Jump = false, m_Hook = false;
		bool m_DirectionOwned = false, m_HookOwned = false;
		vec2 Heading() const
		{
			const int Direction = PrismAssist::IntendedDirection(m_Direction, m_Vel.x, m_RecentDirection, m_Hook, m_Aim);
			vec2 Heading(static_cast<float>(Direction), 0);
			if(m_Hook && length(m_Aim) > 0.01f)
				Heading += normalize(m_Aim) * 0.6f;
			if(m_Jump)
				Heading.y -= 0.7f;
			else if(!Direction && length(m_Vel) > 1)
				Heading += normalize(m_Vel);
			else if(!Direction && length(m_RecentMotion) > 1)
				Heading += normalize(m_RecentMotion);
			return length(Heading) > 0.01f ? normalize(Heading) : vec2(0, 0);
		}
	};
	struct SRoute
	{
		std::array<vec2, MAX_SAMPLES> m_aPoints{};
		int m_Count = 0, m_Tick = -1;
		vec2 m_Waypoint = vec2(0, 0), m_Heading = vec2(0, 0);
		float m_Confidence = 0, m_Radius = 24;
		EMode m_Mode = EMode::NONE;
		bool Valid() const { return m_Count > 1 && m_Confidence > 0; }
		float Deviation(vec2 Pos) const
		{
			float Best = std::numeric_limits<float>::infinity();
			for(int i = 1; i < m_Count; ++i)
			{
				vec2 Closest;
				if(closest_point_on_line(m_aPoints[i - 1], m_aPoints[i], Pos, Closest))
					Best = std::min(Best, distance(Pos, Closest));
				Best = std::min(Best, distance(Pos, m_aPoints[i]));
			}
			return Best;
		}
	};
	struct SStats
	{
		int m_Candidates = 0, m_SimulatedTicks = 0;
		uint64_t m_Plans = 0, m_CacheHits = 0, m_Timeouts = 0;
		double m_LastUs = 0, m_TotalUs = 0, m_WorstUs = 0;
		double AverageUs() const { return m_Plans ? m_TotalUs / m_Plans : 0; }
	};
	// WHERE: a sampled, physically reached local corridor. HOW stays in CPredictor.
	// Clock and simulator are injectable for deterministic budgets and future TAS.
	class CIntentPlanner
	{
		SRoute m_Route;
		SStats m_Stats;
		int m_LastDirection = 0, m_LastTick = -1;
		bool m_LastJump = false, m_LastHook = false, m_Recovering = false;
		vec2 m_LastAim = vec2(1, 0), m_LastPos = vec2(0, 0), m_LastVel = vec2(0, 0);
		int m_LastHorizon = 0;

	public:
		const SRoute &Route() const { return m_Route; }
		const SStats &Stats() const { return m_Stats; }
		void Reset() { *this = {}; }
		void Recovery(bool Active)
		{
			const bool WasRecovering = m_Recovering;
			m_Recovering = Active;
			if(m_Route.Valid() && (Active || WasRecovering))
				m_Route.m_Mode = Active ? EMode::EMERGENCY : EMode::REJOIN;
		}
		template<class TSimulator, class TClock>
		const SRoute &Plan(TSimulator &Simulator, const CNetObj_PlayerInput &Input, const SIntent &Intent,
			int Tick, int Horizon, int BudgetUs, const vec2 *pAnchors, int Anchors, TClock Clock)
		{
			Horizon = std::clamp(Horizon, static_cast<int>(SERVER_TICK_SPEED), PrismAssist::MAX_ROUTE_TICKS);
			const vec2 Heading = Intent.Heading();
			const bool SameIntent = m_LastDirection == Intent.m_Direction && m_LastJump == Intent.m_Jump &&
						m_LastHook == Intent.m_Hook && distance(m_LastAim, Intent.m_Aim) < 40 &&
						m_LastHorizon == Horizon;
			const bool StableState = SameIntent && distance(m_LastVel, Intent.m_Vel) < 8;
			// Cached corridor is advice only. Freeze Avoid re-simulates NOW every input.
			if(m_Route.Valid() && StableState && Tick >= m_LastTick && Tick - m_LastTick < 5 &&
				distance(Intent.m_Pos, m_LastPos) < 64 && m_Route.Deviation(Intent.m_Pos) < 48)
			{
				++m_Stats.m_CacheHits;
				return m_Route;
			}
			const int64_t Start = Clock();
			m_Stats.m_Candidates = m_Stats.m_SimulatedTicks = 0;
			bool Timeout = false;
			PrismAssist::STrajectory Best;
			float BestScore = -std::numeric_limits<float>::infinity();
			auto Evaluate = [&](const PrismAssist::SAction *pActions, int Phases) {
				if(m_Stats.m_Candidates >= MAX_SEARCH || m_Stats.m_SimulatedTicks + Horizon > MAX_SIMULATED_TICKS ||
					Clock() - Start >= BudgetUs)
				{
					Timeout = true;
					return;
				}
				PrismAssist::STrajectory Path;
				++m_Stats.m_Candidates;
				if(!Simulator.SimulateSequence(Input, pActions, Phases, Horizon, Path))
					return;
				m_Stats.m_SimulatedTicks += Path.m_Count;
				if(Clock() - Start >= BudgetUs)
				{
					Timeout = true;
					return;
				}
				if(!Path.Safe() || (Path.m_Hook && !Path.m_HookAttached))
					return;
				const vec2 End = Path.m_aStates[Path.m_Count - 1].m_Pos;
				const float Progress = dot(End - Intent.m_Pos, Heading);
				const float Cost = (pActions[0].m_Direction != Input.m_Direction ? 12 : 0) +
						   (pActions[0].m_Jump != bool(Input.m_Jump) ? 6 : 0) + (pActions[0].m_Hook ? 8 : 0);
				float Deviation = 0;
				if(m_Route.Valid() && SameIntent)
					for(int i = 0; i < Path.m_Count; i += 8)
						Deviation += std::min(64.0f, m_Route.Deviation(Path.m_aStates[i].m_Pos)) * 0.12f;
				const float Score = Progress - Cost - Deviation;
				if(Score > BestScore + 4)
				{
					Best = Path;
					BestScore = Score;
				}
			};
			PrismAssist::SAction Manual{Input.m_Direction, Input.m_Jump != 0, Horizon};
			Evaluate(&Manual, 1);
			// Safe intended continuation commits immediately; no clearance optimization.
			if(!Best.Safe())
			{
				for(int Delay : {0, 8, 16, 32})
				{
					if(Timeout || Input.m_Jump)
						break;
					PrismAssist::SAction aActions[2] = {Manual, {Input.m_Direction, true, Horizon}};
					aActions[0].m_Ticks = std::max(1, Delay);
					Evaluate(Delay ? aActions : &aActions[1], Delay ? 2 : 1);
				}
				for(int Anchor = 0; Anchor < std::min(Anchors, 4) && !Intent.m_HookOwned && !Intent.m_Hook && !Timeout; ++Anchor)
				{
					PrismAssist::SAction aActions[2] = {{Input.m_Direction, bool(Input.m_Jump), 16, true, pAnchors[Anchor] - Intent.m_Pos}, Manual};
					Evaluate(aActions, 2);
				}
				for(int Direction : {0, -Input.m_Direction})
					for(int Duration : {4, 8, 16})
					{
						if(Intent.m_DirectionOwned || Timeout)
							break;
						PrismAssist::SAction aActions[2] = {{Direction, bool(Input.m_Jump), Duration}, Manual};
						Evaluate(aActions, 2);
					}
			}
			const double Elapsed = static_cast<double>(Clock() - Start);
			++m_Stats.m_Plans;
			m_Stats.m_LastUs = Elapsed;
			m_Stats.m_TotalUs += Elapsed;
			m_Stats.m_WorstUs = std::max(m_Stats.m_WorstUs, Elapsed);
			m_LastDirection = Intent.m_Direction;
			m_LastJump = Intent.m_Jump;
			m_LastHook = Intent.m_Hook;
			m_LastAim = Intent.m_Aim;
			m_LastTick = Tick;
			m_LastPos = Intent.m_Pos;
			m_LastVel = Intent.m_Vel;
			m_LastHorizon = Horizon;
			if(Timeout)
			{
				++m_Stats.m_Timeouts;
				m_Route.m_Confidence = 0;
				m_Route.m_Mode = EMode::BUDGET;
				return m_Route;
			}
			if(Best.Safe())
			{
				SRoute Next;
				Next.m_Tick = Tick;
				Next.m_Heading = Heading;
				Next.m_Confidence = length(Heading) > 0 ? 0.85f : 0.4f;
				Next.m_Mode = m_Recovering ? EMode::REJOIN : EMode::CONTINUE;
				const int Stride = std::max(1, (Best.m_Count + MAX_SAMPLES - 2) / (MAX_SAMPLES - 1));
				for(int i = 0; i < Best.m_Count - 1 && Next.m_Count < MAX_SAMPLES - 1; i += Stride)
					Next.m_aPoints[Next.m_Count++] = Best.m_aStates[i].m_Pos;
				Next.m_Waypoint = Best.m_aStates[Best.m_Count - 1].m_Pos;
				Next.m_aPoints[Next.m_Count++] = Next.m_Waypoint;
				// Keep previous destination during recovery. Never commit to the escape.
				if(m_Recovering && SameIntent && m_Route.Valid() && Tick - m_Route.m_Tick < Horizon)
					m_Route.m_Mode = EMode::REJOIN;
				else
					m_Route = Next;
			}
			else if(!(m_Route.Valid() && SameIntent && Tick - m_Route.m_Tick < Horizon))
			{
				m_Route = {};
				m_Route.m_Mode = EMode::UNKNOWN;
			}
			return m_Route;
		}
		const SRoute &Plan(PrismAssist::CPredictor &Predictor, const CNetObj_PlayerInput &Input, const SIntent &Intent,
			int Tick, int Horizon, int BudgetUs, const vec2 *pAnchors, int Anchors)
		{
			Predictor.SetDeadline(time_get_nanoseconds().count() / 1000 + BudgetUs);
			const auto &Result = Plan(Predictor, Input, Intent, Tick, Horizon, BudgetUs, pAnchors, Anchors,
				[] { return time_get_nanoseconds().count() / 1000; });
			Predictor.SetDeadline(0);
			return Result;
		}
	};

	inline PrismAssist::SCorrection SelectCorrection(const PrismAssist::STrajectory *pPaths, int Count,
		int ManualDirection, bool ManualJump, bool MacroDirectionOwned, int Travel, const SRoute &Route, int Previous)
	{
		auto Result = PrismAssist::SelectCorrection(pPaths, Count, ManualDirection, ManualJump, 2, MacroDirectionOwned, Travel);
		if(!Result.m_Apply || !Route.Valid())
			return Result;
		float Best = -std::numeric_limits<float>::infinity();
		bool BestRoute = false;
		for(int i = 1; i < std::min(Count, PrismAssist::MAX_CANDIDATES); ++i)
		{
			const auto &Path = pPaths[i];
			if(!Path.Safe() || (Path.m_Hook && !Path.m_HookAttached) || (ManualJump && !Path.m_Jump) ||
				(MacroDirectionOwned && Path.m_Direction != pPaths[0].m_Direction))
				continue;
			const vec2 End = Path.m_aStates[Path.m_Count - 1].m_Pos;
			const float Deviation = Route.Deviation(End);
			const bool OnRoute = Deviation <= Route.m_Radius && dot(End - Path.m_aStates[0].m_Pos, Route.m_Heading) >= 0;
			const float Progress = distance(pPaths[0].m_aStates[0].m_Pos, Route.m_Waypoint) - distance(End, Route.m_Waypoint);
			const int Cost = (Path.m_Direction != pPaths[0].m_Direction ? 4 : 0) + (Path.m_Jump != pPaths[0].m_Jump ? 1 : 0) + (Path.m_Hook ? 2 : 0);
			const float Score = Progress - Deviation * 0.6f - Cost * 4 + (i == Previous ? 8 : 0);
			if(i == 1 || (OnRoute && !BestRoute) || (OnRoute == BestRoute && Score > Best + 2))
			{
				Best = Score;
				BestRoute = OnRoute;
				Result = {true, Path.m_Direction, Path.m_Jump, i, Path.m_Hook, Path.m_HookPoint, OnRoute};
			}
		}
		return Result;
	}
}
#endif
