// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PATHFINDER_SIMULATOR_H
#define GAME_CLIENT_PATHFINDER_SIMULATOR_H
#include "types.h"

namespace PrismPath
{
	class CSimulator
	{
		CGameWorld m_Work;
		int m_Id = -1;
		int m_FireCounter = 0;

	public:
		static const char *UnsupportedWorld(CGameWorld &World, int Id);
		static void Clone(CGameWorld &To, CGameWorld &From);
		static EReject CheckPosition(const CCollision &Collision, vec2 Position);
		static EReject CheckSegment(const CCollision &Collision, vec2 From, vec2 To);
		void Init(int Id, int FireCounter = 0)
		{
			m_Id = Id;
			m_FireCounter = FireCounter;
		}
		// Work is detached. No parent links, events, door writes or live character mutations.
		bool Simulate(CGameWorld &From, const SAction &Action, STrace &Trace, int64_t DeadlineUs = 0);
		bool Step(CGameWorld &World, CNetObj_PlayerInput Input, SState &State, EReject &Reject);
		CGameWorld &World() { return m_Work; }
	};
	int64_t NowUs();
}
#endif
