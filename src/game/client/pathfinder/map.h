// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PATHFINDER_MAP_H
#define GAME_CLIENT_PATHFINDER_MAP_H
#include "simulator.h"

#include <queue>
#include <vector>

namespace PrismPath
{
	enum EGeometry : unsigned
	{
		SOLID = 1,
		HOOKABLE = 2,
		FREEZE = 4,
		DEATH = 8,
		TELEPORT = 16,
		SPEEDUP = 32,
		STOPPER = 64,
		SWITCH = 128,
		FLOOR = 256,
		CEILING = 512,
		NARROW = 1024,
		LOW_CLEARANCE = 2048,
		START = 4096,
		FINISH = 8192,
		SPAWN = 16384,
		DEEP_FREEZE = 32768,
		TUNE = 65536,
		TELE_EXIT = 131072
	};
	struct SCell
	{
		unsigned m_Flags = 0;
		unsigned char m_TeleNumber = 0, m_TeleType = 0;
		int m_Region = -1;
		bool m_Passable = false;
	};
	struct SEdge
	{
		int m_To;
		vec2 m_Portal;
	};
	struct SRegion
	{
		int m_Left = 0, m_Right = 0, m_Y = 0;
		unsigned m_Flags = 0;
		std::vector<SEdge> m_vEdges;
		vec2 Center() const { return vec2((m_Left + m_Right + 1) * 16.0f, m_Y * 32.0f + 16); }
	};
	struct SSurface
	{
		vec2 m_From, m_To;
	};

	// Static analysis is incremental, cached for one map lifetime, and never runs disabled.
	class CMapAnalysis
	{
		CCollision *m_pCollision = nullptr;
		std::vector<SCell> m_vCells;
		std::vector<SRegion> m_vRegions;
		std::vector<int> m_vFinishes, m_vStarts, m_vTeleports;
		std::vector<SSurface> m_vSurfaces;
		std::vector<std::vector<int>> m_vSurfaceBuckets;
		int m_Width = 0, m_Height = 0, m_Cursor = 0, m_Phase = 0;
		bool m_Ready = false, m_Failed = false;
		void AnalyzeCell(int Index);
		void ConnectCell(int Index);
		void AddSurface(vec2 From, vec2 To);

	public:
		void Reset();
		bool Begin(CCollision &Collision);
		bool Step(int MaxWork = 1024, int64_t DeadlineUs = 0);
		bool Ready() const { return m_Ready; }
		bool Failed() const { return m_Failed; }
		float Progress() const;
		int Width() const { return m_Width; }
		int Height() const { return m_Height; }
		CCollision *Collision() const { return m_pCollision; }
		const std::vector<SCell> &Cells() const { return m_vCells; }
		const std::vector<SRegion> &Regions() const { return m_vRegions; }
		const std::vector<int> &Starts() const { return m_vStarts; }
		const std::vector<int> &Finishes() const { return m_vFinishes; }
		const std::vector<int> &Teleports() const { return m_vTeleports; }
		const std::vector<SSurface> &Surfaces() const { return m_vSurfaces; }
		int RegionAt(vec2 Position) const;
		int NearestRegion(vec2 Position, float Radius = 96) const;
		int HookCandidates(vec2 Position, vec2 Target, float Range, vec2 *pPoints, int Capacity) const;
		const char *RegionName(int Id) const;
	};

	// Geometric region connectivity is a promising corridor, not proof of reachability.
	// Only CPhysicsSearch can certify a transition with real inputs.
	class CNavigation
	{
		const CMapAnalysis *m_pMap = nullptr;
		std::vector<float> m_vCosts;
		std::vector<int> m_vNext;
		std::vector<vec2> m_vPortals;
		std::vector<vec2> m_vGoals;
		using SQueueEntry = std::pair<float, int>;
		std::priority_queue<SQueueEntry, std::vector<SQueueEntry>, std::greater<SQueueEntry>> m_Queue;
		bool m_Ready = false, m_SafeRoutes = true;
		float m_GoalRadius = 20;

	public:
		void Reset();
		bool Begin(const CMapAnalysis &Map, const std::vector<vec2> &Goals, bool SafeRoutes, float GoalRadius = 20);
		bool Step(int MaxWork = 512, int64_t DeadlineUs = 0);
		bool Ready() const { return m_Ready; }
		float Cost(vec2 Position) const;
		vec2 LocalTarget(vec2 Position, float Lookahead = 256) const;
		bool AtGoal(vec2 Position, float Radius = -1) const;
		vec2 Goal(vec2 Position) const;
		std::vector<vec2> Route(vec2 Position, int Limit = 256) const;
	};
}
#endif
