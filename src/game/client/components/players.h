/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_PLAYERS_H
#define GAME_CLIENT_COMPONENTS_PLAYERS_H
#include <generated/protocol.h>

#include <game/client/component.h>
#include <game/client/render.h>
#include <game/client/prism_effects.h>

class CPlayers : public CComponent
{
	friend class CGhost;

	void RenderHand6(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha);
	void RenderHand7(const CTeeRenderInfo *pInfo, vec2 HandPos, float HandAngle, float Alpha);

	void RenderHand(const CTeeRenderInfo *pInfo, vec2 CenterPos, vec2 Dir, float AngleOffset, vec2 PostRotOffset, float Alpha);
	void RenderPlayer(
		const CScreenRect &ScreenRect,
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientId,
		float Intra = 0.f);
	void RenderHook(
		const CScreenRect &ScreenRect,
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		const CTeeRenderInfo *pRenderInfo,
		int ClientId,
		float Intra = 0.f);
	void RenderHookCollLine(
		const CScreenRect &ScreenRect,
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		int ClientId);
	bool IsPlayerInfoAvailable(int ClientId) const;

	struct CPrismOwner
	{
		PrismEffects::CTrail m_Trail;
		vec2 m_LastPosition = vec2(0, 0);
		vec2 m_LastHookPosition = vec2(0, 0);
		bool m_PositionValid = false;
		bool m_HookValid = false;
		bool m_TeeSeen = false;
		bool m_HookSeen = false;
		float m_SpawnCredit = 0;
	};
	std::array<CPrismOwner, MAX_CLIENTS> m_aPrismOwners{};
	PrismEffects::CParticlePool m_PrismParticles;
	double m_PrismTime = 0;
	float m_PrismDt = 0;
	int m_PrismLastTick = -1;
	unsigned m_PrismRandom = 0x13579bdf;
	void RenderPrismHook(int ClientId, vec2 Position, vec2 HookPosition, float Alpha, bool Local);
	void RenderPrismPlayer(int ClientId, vec2 Position, float Alpha, bool Local);
	float PrismRandom();

	int m_WeaponEmoteQuadContainerIndex;
	int m_aWeaponSpriteMuzzleQuadContainerIndex[NUM_WEAPONS];

	void CreateNinjaTeeRenderInfo();
	void CreateSpectatorTeeRenderInfo();

	std::shared_ptr<CManagedTeeRenderInfo> m_pNinjaTeeRenderInfo;
	std::shared_ptr<CManagedTeeRenderInfo> m_pSpectatorTeeRenderInfo;

public:
	float GetPlayerTargetAngle(
		const CNetObj_Character *pPrevChar,
		const CNetObj_Character *pPlayerChar,
		int ClientId,
		float Intra = 0.0f);

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnReset() override;
	void OnStateChange(int NewState, int OldState) override;

	const std::shared_ptr<CManagedTeeRenderInfo> &NinjaTeeRenderInfo() const { return m_pNinjaTeeRenderInfo; }
	const std::shared_ptr<CManagedTeeRenderInfo> &SpectatorTeeRenderInfo() const { return m_pSpectatorTeeRenderInfo; }
};

#endif
