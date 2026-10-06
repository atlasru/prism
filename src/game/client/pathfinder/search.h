// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PATHFINDER_SEARCH_H
#define GAME_CLIENT_PATHFINDER_SEARCH_H
#include "map.h"

#include <memory>
#include <unordered_map>

namespace PrismPath
{
	float ScoreState(const SState &State, float InitialCost, float RemainingCost, vec2 Target, int Elapsed, bool Momentum);

	class CPhysicsSearch
	{
		struct SNode
		{
			std::unique_ptr<CGameWorld> m_pWorld;
			STrace m_Trace;
			SAction m_Action;
			int m_Parent = -1, m_Elapsed = 0;
			float m_Score = 0;
		};
		CSimulator m_Simulator;
		std::vector<SNode> m_vNodes;
		std::priority_queue<std::pair<float, int>> m_Queue;
		std::unordered_map<SStateKey, float, SStateHash> m_Visited;
		std::vector<SAction> m_vActions;
		const CMapAnalysis *m_pMap = nullptr;
		const CNavigation *m_pNavigation = nullptr;
		SSettings m_Settings;
		int m_Id = -1, m_PendingNode = -1, m_ActionCursor = 0, m_BestNode = -1;
		int m_MinSimulations = 192;
		float m_InitialCost = UNREACHABLE;
		vec2 m_LocalTarget{};
		bool m_Active = false, m_Complete = false;
		SStats m_Stats;
		std::array<vec2, MAX_ANCHORS> m_aAnchors{};
		int m_AnchorCount = 0;
		std::array<STrace, 8> m_aDebugTraces{};
		int m_DebugCount = 0;
		void GenerateActions(const SNode &Node);
		void CountReject(EReject Reject);

	public:
		void Reset();
		bool Begin(CGameWorld &World, int Id, const CMapAnalysis &Map, const CNavigation &Navigation, const SSettings &Settings);
		// Wall-clock bounded, resumable between input ticks. MaxWork gives deterministic tests.
		bool Step(int64_t DeadlineUs = 0, int MaxWork = 256);
		bool Active() const { return m_Active; }
		bool Complete() const { return m_Complete; }
		SPlan Result() const;
		const SStats &Stats() const { return m_Stats; }
		vec2 LocalTarget() const { return m_LocalTarget; }
		const std::array<vec2, MAX_ANCHORS> &Anchors() const { return m_aAnchors; }
		int AnchorCount() const { return m_AnchorCount; }
		const std::array<STrace, 8> &DebugTraces() const { return m_aDebugTraces; }
		int DebugCount() const { return m_DebugCount; }
	};
}
#endif
