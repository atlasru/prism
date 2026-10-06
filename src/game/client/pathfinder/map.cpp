#include "map.h"

#include <game/collision.h>
#include <game/mapitems.h>

#include <algorithm>

namespace PrismPath
{
	void CMapAnalysis::Reset()
	{
		m_pCollision = nullptr;
		m_vCells.clear();
		m_vRegions.clear();
		m_vFinishes.clear();
		m_vStarts.clear();
		m_vTeleports.clear();
		m_vSurfaces.clear();
		m_vSurfaceBuckets.clear();
		m_Width = m_Height = m_Cursor = m_Phase = 0;
		m_Ready = m_Failed = false;
	}
	bool CMapAnalysis::Begin(CCollision &Collision)
	{
		Reset();
		m_Width = Collision.GetWidth();
		m_Height = Collision.GetHeight();
		if(m_Width <= 0 || m_Height <= 0 || int64_t(m_Width) * m_Height > 4 * 1024 * 1024)
		{
			m_Failed = true;
			return false;
		}
		m_pCollision = &Collision;
		m_vCells.reserve(m_Width * m_Height);
		m_vSurfaceBuckets.resize(((m_Width + 7) / 8) * ((m_Height + 7) / 8));
		return true;
	}
	void CMapAnalysis::AnalyzeCell(int Index)
	{
		const int X = Index % m_Width, Y = Index / m_Width;
		const vec2 P(X * 32 + 16, Y * 32 + 16);
		const auto &C = *m_pCollision;
		SCell Cell;
		const int Collision = C.GetCollisionAt(P.x, P.y);
		if(Collision == TILE_SOLID || Collision == TILE_NOHOOK)
			Cell.m_Flags |= SOLID;
		if(Collision == TILE_SOLID)
			Cell.m_Flags |= HOOKABLE;
		for(int Tile : {C.GetTileIndex(Index), C.GetFrontTileIndex(Index)})
		{
			if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
				Cell.m_Flags |= FREEZE;
			if(Tile == TILE_DFREEZE || Tile == TILE_LFREEZE)
				Cell.m_Flags |= DEEP_FREEZE;
			if(Tile == TILE_DEATH)
				Cell.m_Flags |= DEATH;
			if(Tile == TILE_STOP || Tile == TILE_STOPS || Tile == TILE_STOPA)
				Cell.m_Flags |= STOPPER;
			if(Tile == TILE_START)
				Cell.m_Flags |= START;
			if(Tile == TILE_FINISH)
				Cell.m_Flags |= FINISH;
			if(Tile == ENTITY_SPAWN + ENTITY_OFFSET)
				Cell.m_Flags |= SPAWN;
		}
		if(C.IsTeleport(Index) || C.IsEvilTeleport(Index) || C.IsCheckTeleport(Index) || C.IsCheckEvilTeleport(Index) ||
			C.IsTeleportHook(Index))
			Cell.m_Flags |= TELEPORT;
		if(C.TeleLayer())
		{
			Cell.m_TeleNumber = C.TeleLayer()[Index].m_Number;
			Cell.m_TeleType = C.TeleLayer()[Index].m_Type;
			if(Cell.m_TeleType == TILE_TELEOUT || Cell.m_TeleType == TILE_TELECHECKOUT)
				Cell.m_Flags |= TELE_EXIT;
		}
		if(C.IsSpeedup(Index))
			Cell.m_Flags |= SPEEDUP;
		if(C.IsTune(Index))
			Cell.m_Flags |= TUNE;
		if(C.GetSwitchType(Index))
			Cell.m_Flags |= SWITCH;
		if(Y + 1 < m_Height && C.CheckPoint(P + vec2(0, 32)))
			Cell.m_Flags |= FLOOR;
		if(Y > 0 && C.CheckPoint(P - vec2(0, 32)))
			Cell.m_Flags |= CEILING;
		if((Cell.m_Flags & (FLOOR | CEILING)) == (FLOOR | CEILING) ||
			(C.CheckPoint(P - vec2(32, 0)) && C.CheckPoint(P + vec2(32, 0))))
			Cell.m_Flags |= NARROW;
		for(vec2 Offset : {vec2(-32, 0), vec2(32, 0), vec2(0, -32), vec2(0, 32)})
		{
			const int I = C.GetPureMapIndex(P + Offset);
			for(int Tile : {C.GetTileIndex(I), C.GetFrontTileIndex(I)})
				if(Tile == TILE_FREEZE || Tile == TILE_DFREEZE || Tile == TILE_DEATH)
					Cell.m_Flags |= LOW_CLEARANCE;
		}
		Cell.m_Passable = !(Cell.m_Flags & (SOLID | FREEZE | DEATH | TELEPORT)) &&
				  !C.TestBox(P, CCharacterCore::PhysicalSizeVec2()) && CSimulator::CheckPosition(C, P) == EReject::NONE;
		if(Cell.m_Passable)
		{
			const unsigned Features = Cell.m_Flags & (FLOOR | CEILING | NARROW | LOW_CLEARANCE | SPEEDUP | STOPPER | SWITCH | TUNE);
			if(X > 0 && !m_vRegions.empty() && m_vRegions.back().m_Y == Y &&
				m_vRegions.back().m_Right == X - 1 && m_vRegions.back().m_Right - m_vRegions.back().m_Left < 7 &&
				m_vRegions.back().m_Flags == Features)
				m_vRegions.back().m_Right = X;
			else
				m_vRegions.push_back({X, X, Y, Features, {}});
			Cell.m_Region = static_cast<int>(m_vRegions.size()) - 1;
		}
		if(Cell.m_Flags & FINISH)
			m_vFinishes.push_back(Index);
		if(Cell.m_Flags & (START | SPAWN))
			m_vStarts.push_back(Index);
		if(Cell.m_Flags & (TELEPORT | TELE_EXIT))
			m_vTeleports.push_back(Index);
		m_vCells.push_back(Cell);
	}
	void CMapAnalysis::AddSurface(vec2 From, vec2 To)
	{
		// Faces are tile segments, queried through spatial buckets rather than a full-map scan.
		const int Id = static_cast<int>(m_vSurfaces.size());
		m_vSurfaces.push_back({From, To});
		const vec2 Mid = (From + To) * 0.5f;
		const int BX = std::clamp(int(Mid.x / 256), 0, (m_Width + 7) / 8 - 1);
		const int BY = std::clamp(int(Mid.y / 256), 0, (m_Height + 7) / 8 - 1);
		m_vSurfaceBuckets[BY * ((m_Width + 7) / 8) + BX].push_back(Id);
	}
	void CMapAnalysis::ConnectCell(int Index)
	{
		const int X = Index % m_Width, Y = Index / m_Width;
		const SCell &Cell = m_vCells[Index];
		if(Cell.m_Region >= 0)
			for(int Other : {X + 1 < m_Width ? Index + 1 : -1, Y + 1 < m_Height ? Index + m_Width : -1})
			{
				if(Other < 0 || m_vCells[Other].m_Region < 0 || m_vCells[Other].m_Region == Cell.m_Region)
					continue;
				const int A = Cell.m_Region, B = m_vCells[Other].m_Region;
				const vec2 Portal = (m_pCollision->GetPos(Index) + m_pCollision->GetPos(Other)) * 0.5f;
				auto Connect = [&](int From, int To) {
					auto &Edges = m_vRegions[From].m_vEdges;
					if(std::none_of(Edges.begin(), Edges.end(), [&](const SEdge &E) { return E.m_To == To; }))
						Edges.push_back({To, Portal});
				};
				Connect(A, B);
				Connect(B, A);
			}
		if(!(Cell.m_Flags & HOOKABLE))
			return;
		auto Empty = [&](int NX, int NY) {
			return NX >= 0 && NY >= 0 && NX < m_Width && NY < m_Height && !(m_vCells[NY * m_Width + NX].m_Flags & SOLID);
		};
		if(Empty(X, Y - 1))
			AddSurface(vec2(X * 32 + 2, Y * 32 + 1), vec2(X * 32 + 30, Y * 32 + 1));
		if(Empty(X, Y + 1))
			AddSurface(vec2(X * 32 + 2, Y * 32 + 31), vec2(X * 32 + 30, Y * 32 + 31));
		if(Empty(X - 1, Y))
			AddSurface(vec2(X * 32 + 1, Y * 32 + 2), vec2(X * 32 + 1, Y * 32 + 30));
		if(Empty(X + 1, Y))
			AddSurface(vec2(X * 32 + 31, Y * 32 + 2), vec2(X * 32 + 31, Y * 32 + 30));
	}
	bool CMapAnalysis::Step(int MaxWork, int64_t Deadline)
	{
		if(m_Ready || m_Failed || !m_pCollision)
			return m_Ready;
		for(int Work = 0; Work < MaxWork; ++Work)
		{
			if((Work & 31) == 0 && Deadline && NowUs() >= Deadline)
				break;
			if(m_Phase == 0)
				AnalyzeCell(m_Cursor);
			else
				ConnectCell(m_Cursor);
			if(++m_Cursor == m_Width * m_Height)
			{
				m_Cursor = 0;
				if(++m_Phase == 2)
				{
					m_Ready = true;
					break;
				}
			}
		}
		return m_Ready;
	}
	float CMapAnalysis::Progress() const { return m_Ready ? 1 : (m_Phase + m_Cursor / float(std::max(1, m_Width * m_Height))) / 2; }
	int CMapAnalysis::RegionAt(vec2 P) const
	{
		if(!m_Ready || P.x < 0 || P.y < 0 || P.x >= m_Width * 32 || P.y >= m_Height * 32)
			return -1;
		return m_vCells[int(P.y / 32) * m_Width + int(P.x / 32)].m_Region;
	}
	int CMapAnalysis::NearestRegion(vec2 P, float Radius) const
	{
		int Best = RegionAt(P);
		if(Best >= 0)
			return Best;
		float BestDistance = Radius;
		const int CX = int(P.x / 32), CY = int(P.y / 32), Tiles = int(Radius / 32) + 1;
		for(int Y = std::max(0, CY - Tiles); Y <= std::min(m_Height - 1, CY + Tiles); ++Y)
			for(int X = std::max(0, CX - Tiles); X <= std::min(m_Width - 1, CX + Tiles); ++X)
			{
				const int Id = m_vCells[Y * m_Width + X].m_Region;
				const float D = distance(P, vec2(X * 32 + 16, Y * 32 + 16));
				if(Id >= 0 && D < BestDistance)
				{
					Best = Id;
					BestDistance = D;
				}
			}
		return Best;
	}
	int CMapAnalysis::HookCandidates(vec2 P, vec2 Target, float Range, vec2 *pPoints, int Capacity) const
	{
		if(!m_Ready || Capacity <= 0)
			return 0;
		struct SCandidate
		{
			vec2 m_Point;
			float m_Cost;
		};
		std::array<SCandidate, 32> aBest;
		int Count = 0;
		const int BW = (m_Width + 7) / 8, BH = (m_Height + 7) / 8;
		for(int BY = std::max(0, int((P.y - Range) / 256)); BY <= std::min(BH - 1, int((P.y + Range) / 256)); ++BY)
			for(int BX = std::max(0, int((P.x - Range) / 256)); BX <= std::min(BW - 1, int((P.x + Range) / 256)); ++BX)
				for(int Id : m_vSurfaceBuckets[BY * BW + BX])
				{
					const auto &Face = m_vSurfaces[Id];
					vec2 Projection;
					closest_point_on_line(Face.m_From, Face.m_To, Target, Projection);
					for(vec2 AimPoint : {Face.m_From, Face.m_To, Projection})
					{
						const vec2 Aim = AimPoint - P;
						if(length(Aim) < 24 || length(Aim) > Range)
							continue;
						const float Cost = distance(AimPoint, Target) * 0.6f + length(Aim) * 0.12f + std::max(0.0f, AimPoint.y - P.y) * 0.6f;
						if(Count == int(aBest.size()) && Cost >= aBest[Count - 1].m_Cost)
							continue;
						bool Duplicate = false;
						for(int i = 0; i < Count; ++i)
							Duplicate |= distance(aBest[i].m_Point, AimPoint) < 16;
						if(Duplicate)
							continue;
						if(Count < int(aBest.size()))
							aBest[Count++] = {AimPoint, Cost};
						else
							aBest[Count - 1] = {AimPoint, Cost};
						std::sort(aBest.begin(), aBest.begin() + Count, [](const SCandidate &A, const SCandidate &B) { return A.m_Cost < B.m_Cost; });
					}
				}
		int Out = 0;
		// At most 32 authoritative rays per expansion, regardless of map size.
		for(int i = 0; i < Count && Out < std::min(Capacity, MAX_ANCHORS); ++i)
		{
			const vec2 Aim = aBest[i].m_Point - P;
			vec2 Hit, Before;
			int Tele = 0;
			if(m_pCollision->IntersectLineTeleHook(P, P + normalize(Aim) * Range, &Hit, &Before, &Tele) != TILE_SOLID || Tele)
				continue;
			bool Duplicate = false;
			for(int j = 0; j < Out; ++j)
				Duplicate |= distance(pPoints[j], Hit) < 28;
			if(!Duplicate)
				pPoints[Out++] = Hit;
		}
		return Out;
	}
	const char *CMapAnalysis::RegionName(int Id) const
	{
		if(Id < 0 || Id >= int(m_vRegions.size()))
			return "outside corridor";
		const unsigned F = m_vRegions[Id].m_Flags;
		if(F & NARROW)
			return F & LOW_CLEARANCE ? "freeze corridor" : "narrow corridor";
		if(F & LOW_CLEARANCE)
			return "low clearance";
		if(F & SPEEDUP)
			return "speedup";
		if(F & FLOOR)
			return "platform / ledge";
		if(F & CEILING)
			return "hook ceiling";
		return "gap / shaft";
	}
	void CNavigation::Reset()
	{
		m_pMap = nullptr;
		m_vCosts.clear();
		m_vNext.clear();
		m_vPortals.clear();
		m_vGoals.clear();
		m_Queue = {};
		m_Ready = false;
	}
	bool CNavigation::Begin(const CMapAnalysis &Map, const std::vector<vec2> &Goals, bool SafeRoutes, float GoalRadius)
	{
		Reset();
		m_GoalRadius = GoalRadius;
		if(!Map.Ready() || Goals.empty())
			return false;
		m_pMap = &Map;
		m_SafeRoutes = SafeRoutes;
		m_vCosts.assign(Map.Regions().size(), UNREACHABLE);
		m_vNext.assign(Map.Regions().size(), -1);
		m_vPortals.resize(Map.Regions().size());
		for(vec2 Goal : Goals)
		{
			const int R = Map.NearestRegion(Goal, 48);
			if(R < 0)
				continue;
			m_vGoals.push_back(Goal);
			m_vCosts[R] = 0;
			m_vPortals[R] = Goal;
			m_Queue.emplace(0, R);
		}
		return !m_vGoals.empty();
	}
	bool CNavigation::Step(int MaxWork, int64_t Deadline)
	{
		if(!m_pMap)
			return false;
		for(int Work = 0; Work < MaxWork && !m_Queue.empty(); ++Work)
		{
			if((Work & 31) == 0 && Deadline && NowUs() >= Deadline)
				break;
			const auto [Cost, Id] = m_Queue.top();
			m_Queue.pop();
			if(Cost > m_vCosts[Id])
				continue;
			const SRegion &Region = m_pMap->Regions()[Id];
			for(const auto &Edge : Region.m_vEdges)
			{
				const auto &Other = m_pMap->Regions()[Edge.m_To];
				const float Penalty = m_SafeRoutes && (Other.m_Flags & LOW_CLEARANCE) ? 4.0f : 0;
				const float NextCost = Cost + distance(Region.Center(), Edge.m_Portal) + distance(Edge.m_Portal, Other.Center()) + Penalty;
				if(NextCost >= m_vCosts[Edge.m_To])
					continue;
				m_vCosts[Edge.m_To] = NextCost;
				m_vNext[Edge.m_To] = Id;
				m_vPortals[Edge.m_To] = Edge.m_Portal;
				m_Queue.emplace(NextCost, Edge.m_To);
			}
		}
		return m_Ready = m_Queue.empty();
	}
	float CNavigation::Cost(vec2 P) const
	{
		if(!m_Ready || !m_pMap)
			return UNREACHABLE;
		const int Id = m_pMap->NearestRegion(P, 48);
		if(Id < 0 || m_vCosts[Id] >= UNREACHABLE)
			return UNREACHABLE;
		if(m_vNext[Id] < 0)
			return distance(P, m_vPortals[Id]);
		const int Next = m_vNext[Id];
		return distance(P, m_vPortals[Id]) + distance(m_vPortals[Id], m_pMap->Regions()[Next].Center()) + m_vCosts[Next];
	}
	std::vector<vec2> CNavigation::Route(vec2 P, int Limit) const
	{
		std::vector<vec2> Route;
		if(!m_Ready || !m_pMap)
			return Route;
		int Id = m_pMap->NearestRegion(P, 48);
		if(Id < 0 || m_vCosts[Id] >= UNREACHABLE)
			return Route;
		Route.push_back(P);
		for(int i = 0; Id >= 0 && i < Limit; ++i)
		{
			Route.push_back(m_vPortals[Id]);
			Id = m_vNext[Id];
		}
		return Route;
	}
	vec2 CNavigation::LocalTarget(vec2 P, float Lookahead) const
	{
		if(!m_Ready || !m_pMap)
			return P;
		int Id = m_pMap->NearestRegion(P, 48);
		if(Id < 0 || m_vCosts[Id] >= UNREACHABLE)
			return P;
		vec2 Previous = P;
		float Length = 0;
		for(int i = 0; Id >= 0 && i < 64; ++i)
		{
			const vec2 Point = m_vPortals[Id];
			Length += distance(Previous, Point);
			if(Length >= Lookahead || m_vNext[Id] < 0)
				return Point;
			Previous = Point;
			Id = m_vNext[Id];
		}
		return Previous;
	}
	bool CNavigation::AtGoal(vec2 P, float Radius) const
	{
		if(Radius < 0)
			Radius = m_GoalRadius;
		return std::any_of(m_vGoals.begin(), m_vGoals.end(), [&](vec2 Goal) { return distance(P, Goal) <= Radius; });
	}
	vec2 CNavigation::Goal(vec2 P) const
	{
		vec2 Best = P;
		const auto RoutePoints = Route(P);
		if(!RoutePoints.empty())
			Best = RoutePoints.back();
		return Best;
	}
}
