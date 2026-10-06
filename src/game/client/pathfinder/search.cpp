#include "search.h"

#include <algorithm>

namespace PrismPath
{
	float ScoreState(const SState &S, float InitialCost, float RemainingCost, vec2 Target, int Elapsed, bool Momentum)
	{
		if(RemainingCost >= UNREACHABLE || S.m_FreezeTime || S.m_DeepFrozen || S.m_LiveFrozen)
			return -UNREACHABLE;
		const vec2 Delta = Target - S.m_Pos;
		const float UsefulVelocity = length(Delta) > 1 ? dot(S.m_Vel, normalize(Delta)) : 0;
		return InitialCost - RemainingCost - Elapsed * 0.65f + (Momentum ? UsefulVelocity * 2.0f : 0) +
		       (S.m_Grounded ? 3.0f : 0) + (S.m_Jumps - S.m_JumpedTotal) * 0.5f;
	}
	void CPhysicsSearch::Reset()
	{
		m_vNodes.clear();
		m_Queue = {};
		m_Visited.clear();
		m_vActions.clear();
		m_pMap = nullptr;
		m_pNavigation = nullptr;
		m_PendingNode = m_BestNode = -1;
		m_ActionCursor = 0;
		m_Active = m_Complete = false;
		m_Stats = {};
		m_AnchorCount = m_DebugCount = 0;
	}
	bool CPhysicsSearch::Begin(CGameWorld &World, int Id, const CMapAnalysis &Map, const CNavigation &Navigation, const SSettings &Settings)
	{
		Reset();
		if(CSimulator::UnsupportedWorld(World, Id) || !Map.Ready() || !Navigation.Ready())
			return false;
		m_pMap = &Map;
		m_pNavigation = &Navigation;
		m_Settings = Settings;
		m_Id = Id;
		m_Settings.m_Horizon = std::clamp(Settings.m_Horizon, 24, MAX_HORIZON);
		const SState State = SState::Read(World, Id);
		if(CSimulator::CheckPosition(*World.Collision(), State.m_Pos) != EReject::NONE || State.m_FreezeTime || State.m_DeepFrozen || State.m_LiveFrozen)
			return false;
		m_InitialCost = Navigation.Cost(State.m_Pos);
		if(m_InitialCost >= UNREACHABLE)
			return false;
		m_LocalTarget = Navigation.LocalTarget(State.m_Pos);
		m_Simulator.Init(Id, World.GetCharacterById(Id)->Core()->m_Input.m_Fire);
		m_vNodes.reserve(MAX_NODES);
		m_Visited.reserve(MAX_NODES * 2);
		m_vActions.reserve(192);
		SNode Root;
		Root.m_pWorld = std::make_unique<CGameWorld>();
		CSimulator::Clone(*Root.m_pWorld, World);
		Root.m_Trace.m_aStates[0] = State;
		Root.m_Trace.m_Count = 1;
		Root.m_Score = ScoreState(State, m_InitialCost, m_InitialCost, m_LocalTarget, 0, Settings.m_Momentum);
		m_vNodes.push_back(std::move(Root));
		m_Queue.emplace(m_vNodes[0].m_Score, 0);
		m_Active = true;
		return true;
	}
	void CPhysicsSearch::GenerateActions(const SNode &Node)
	{
		m_vActions.clear();
		const auto &State = Node.m_Trace.m_aStates[Node.m_Trace.m_Count - 1];
		const int Preferred = m_LocalTarget.x > State.m_Pos.x + 4 ? 1 : m_LocalTarget.x < State.m_Pos.x - 4 ? -1 :
														      0;
		const std::array<int, 3> Directions = Preferred ? std::array<int, 3>{Preferred, 0, -Preferred} : std::array<int, 3>{0, 1, -1};
		auto Add = [&](int Dir, bool Jump, bool Hook, vec2 Anchor, int Ticks, vec2 Aim) {
			if(Node.m_Elapsed + Ticks > m_Settings.m_Horizon)
				return;
			m_vActions.push_back({Dir, Ticks, Jump, Hook, Anchor, Aim});
		};
		// Release edges and short durations search precise jump/hook timing.
		for(int Ticks : {8, 4, 12, 2})
			for(int Dir : Directions)
				for(bool Jump : {false, true})
				{
					if(Jump && (State.m_Jumped & 1))
						continue;
					Add(Dir, Jump, false, vec2(0, 0), Ticks, vec2(Preferred ? Preferred * 100 : 100, 0));
				}
		if(State.m_HookState == HOOK_GRABBED || State.m_HookState == HOOK_FLYING)
		{
			const vec2 Aim = State.m_HookState == HOOK_FLYING ? State.m_HookDir * 256 : State.m_HookPos - State.m_Pos;
			for(int Ticks : {8, 4, 12, 2})
				for(int Dir : Directions)
					Add(Dir, false, true, State.m_HookPos, Ticks, Aim);
		}
		else
		{
			m_AnchorCount = m_pMap->HookCandidates(State.m_Pos, m_LocalTarget,
				static_cast<float>(Node.m_pWorld->GetCharacterById(m_Id)->Core()->m_Tuning.m_HookLength), m_aAnchors.data(), MAX_ANCHORS);
			for(int A = 0; A < m_AnchorCount; ++A)
				for(int Dir : Directions)
					for(bool Jump : {false, true})
						for(int Ticks : {8, 12})
						{
							if(Jump && (State.m_Jumped & 1))
								continue;
							Add(Dir, Jump, true, m_aAnchors[A], Ticks, m_aAnchors[A] - State.m_Pos);
						}
		}
	}
	void CPhysicsSearch::CountReject(EReject R)
	{
		++m_Stats.m_Pruned;
		if(R == EReject::DEATH)
			++m_Stats.m_Dead;
		else if(R == EReject::FREEZE)
			++m_Stats.m_Frozen;
		else if(R != EReject::TIMEOUT && R != EReject::NONE)
			++m_Stats.m_Unsupported;
	}
	bool CPhysicsSearch::Step(int64_t Deadline, int MaxWork)
	{
		if(!m_Active)
			return m_Complete;
		const int64_t Start = NowUs();
		for(int Work = 0; Work < MaxWork; ++Work)
		{
			if(Deadline && NowUs() >= Deadline)
			{
				++m_Stats.m_Timeouts;
				break;
			}
			if(m_PendingNode < 0)
			{
				if(m_Queue.empty() || m_vNodes.size() >= MAX_NODES)
				{
					m_Active = false;
					m_Complete = true;
					break;
				}
				m_PendingNode = m_Queue.top().second;
				m_Queue.pop();
				GenerateActions(m_vNodes[m_PendingNode]);
				m_ActionCursor = 0;
				++m_Stats.m_Expanded;
			}
			if(m_ActionCursor == int(m_vActions.size()))
			{
				m_PendingNode = -1;
				continue;
			}
			const SAction Action = m_vActions[m_ActionCursor];
			SNode &Parent = m_vNodes[m_PendingNode];
			STrace Trace;
			++m_Stats.m_Simulations;
			if(!m_Simulator.Simulate(*Parent.m_pWorld, Action, Trace, Deadline))
			{
				if(Trace.m_Reject == EReject::TIMEOUT)
				{
					++m_Stats.m_Timeouts;
					break;
				}
				++m_ActionCursor;
				CountReject(Trace.m_Reject);
				if(m_DebugCount < int(m_aDebugTraces.size()))
					m_aDebugTraces[m_DebugCount++] = Trace;
				continue;
			}
			++m_ActionCursor;
			const SState &State = Trace.m_aStates[Trace.m_Count - 1];
			const int Elapsed = Parent.m_Elapsed + Action.m_Ticks;
			const float Remaining = m_pNavigation->Cost(State.m_Pos);
			const float Score = ScoreState(State, m_InitialCost, Remaining, m_pNavigation->LocalTarget(State.m_Pos), Elapsed, m_Settings.m_Momentum);
			const SStateKey Key = SStateKey::From(State, Action.m_Jump, Action.m_Hook);
			const auto Previous = m_Visited.find(Key);
			if(Score <= -UNREACHABLE || (Previous != m_Visited.end() && Previous->second >= Score - 0.01f))
			{
				CountReject(EReject::NONE);
				continue;
			}
			m_Visited[Key] = Score;
			SNode Node;
			Node.m_Parent = m_PendingNode;
			Node.m_Action = Action;
			Node.m_Elapsed = Elapsed;
			Node.m_Trace = Trace;
			Node.m_Score = Score;
			Node.m_pWorld = std::make_unique<CGameWorld>();
			CSimulator::Clone(*Node.m_pWorld, m_Simulator.World());
			const int Id = static_cast<int>(m_vNodes.size());
			m_vNodes.push_back(std::move(Node));
			if(Elapsed < m_Settings.m_Horizon)
				m_Queue.emplace(Score, Id);
			if((m_BestNode < 0 || Score > m_vNodes[m_BestNode].m_Score) &&
				(m_InitialCost - Remaining > 12 || m_pNavigation->AtGoal(State.m_Pos)))
				m_BestNode = Id;
			if(m_pNavigation->AtGoal(State.m_Pos) || (m_BestNode >= 0 && m_Stats.m_Simulations >= uint64_t(m_MinSimulations) &&
									 (m_vNodes[m_BestNode].m_Elapsed >= m_Settings.m_Horizon - 12 || m_InitialCost - Remaining > 384)))
			{
				if(m_pNavigation->AtGoal(State.m_Pos))
					m_BestNode = Id;
				m_Active = false;
				m_Complete = true;
				break;
			}
		}
		m_Stats.m_LastUs = static_cast<double>(NowUs() - Start);
		m_Stats.m_WorstUs = std::max(m_Stats.m_WorstUs, m_Stats.m_LastUs);
		return m_Complete;
	}
	SPlan CPhysicsSearch::Result() const
	{
		SPlan Plan;
		if(m_BestNode < 0)
			return Plan;
		std::array<int, MAX_HORIZON> aChain;
		int Count = 0;
		for(int Id = m_BestNode; Id > 0 && Count < MAX_HORIZON; Id = m_vNodes[Id].m_Parent)
			aChain[Count++] = Id;
		Plan.m_aStates[0] = m_vNodes[0].m_Trace.m_aStates[0];
		for(int i = Count - 1; i >= 0; --i)
		{
			const SNode &N = m_vNodes[aChain[i]];
			for(int Tick = 1; Tick < N.m_Trace.m_Count && Plan.m_Ticks < MAX_HORIZON; ++Tick)
			{
				Plan.m_aInputs[Plan.m_Ticks] = N.m_Action.Input();
				Plan.m_aStates[++Plan.m_Ticks] = N.m_Trace.m_aStates[Tick];
				if(N.m_Trace.m_aStates[Tick].m_Grounded)
				{
					Plan.m_HasLanding = true;
					Plan.m_Landing = N.m_Trace.m_aStates[Tick].m_Pos;
				}
			}
			if(N.m_Action.m_Hook)
			{
				Plan.m_UsesHook = true;
				Plan.m_HookPoint = N.m_Action.m_Anchor;
			}
		}
		Plan.m_Score = m_vNodes[m_BestNode].m_Score;
		return Plan;
	}
}
