// Prism additions, distributed under the zlib license in license.txt.
#ifndef GAME_CLIENT_PRISM_QOL_H
#define GAME_CLIENT_PRISM_QOL_H

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace PrismQol
{
static constexpr int NUM_MACROS = 4;
static constexpr int MAX_STEPS = 8;
static constexpr int MAX_DELAY_MS = 5000;
enum EAction
{
	WAIT,
	LEFT_ON, LEFT_OFF,
	RIGHT_ON, RIGHT_OFF,
	JUMP_ON, JUMP_OFF,
	HOOK_ON, HOOK_OFF,
	FIRE_PULSE,
	RELEASE_ALL,
	NUM_ACTIONS
};
enum EMode { ONCE, HOLD, TOGGLE };
enum EOwned { OWN_LEFT = 1, OWN_RIGHT = 2, OWN_JUMP = 4, OWN_HOOK = 8 };
static constexpr const char *ACTION_NAMES[NUM_ACTIONS] = {
	"Wait", "Left down", "Left up", "Right down", "Right up", "Jump down", "Jump up",
	"Hook down", "Hook up", "Fire pulse", "Release owned inputs"};

struct SStep
{
	int m_Action = WAIT;
	int m_DelayMs = 100;
};
struct SSequence
{
	std::array<SStep, MAX_STEPS> m_aSteps{};
	int m_Count = 0;
};

// This is a fixed numeric format, NOT a command interpreter. No commands,
// arbitrary text, external programs, recursion or script evaluation are accepted.
inline bool Parse(const char *pText, SSequence &Out)
{
	Out = {};
	if(!pText)
		return false;
	const char *p = pText;
	while(*p)
	{
		if(Out.m_Count == MAX_STEPS || *p < '0' || *p > '9')
			return false;
		char *pEnd;
		const long Action = std::strtol(p, &pEnd, 10);
		if(pEnd == p || *pEnd != ':' || Action < 0 || Action >= NUM_ACTIONS)
			return false;
		p = pEnd + 1;
		if(*p < '0' || *p > '9')
			return false;
		const long Delay = std::strtol(p, &pEnd, 10);
		if(pEnd == p || Delay < 0 || Delay > MAX_DELAY_MS || (*pEnd && *pEnd != ';'))
			return false;
		Out.m_aSteps[Out.m_Count++] = {(int)Action, (int)Delay};
		p = pEnd;
		if(*p == ';')
		{
			++p;
			if(!*p)
				return false;
		}
	}
	return true;
}

inline bool Encode(const SSequence &Sequence, char *pOut, size_t Capacity)
{
	if(!pOut || !Capacity || Sequence.m_Count < 0 || Sequence.m_Count > MAX_STEPS)
		return false;
	pOut[0] = '\0';
	size_t Written = 0;
	for(int i = 0; i < Sequence.m_Count; ++i)
	{
		const auto &Step = Sequence.m_aSteps[i];
		if(Step.m_Action < 0 || Step.m_Action >= NUM_ACTIONS || Step.m_DelayMs < 0 || Step.m_DelayMs > MAX_DELAY_MS)
			return false;
		const int Count = std::snprintf(pOut + Written, Capacity - Written, "%s%d:%d", i ? ";" : "", Step.m_Action, Step.m_DelayMs);
		if(Count < 0 || (size_t)Count >= Capacity - Written)
		{
			pOut[0] = '\0';
			return false;
		}
		Written += Count;
	}
	return true;
}

class CMacroEngine
{
	struct SSlot
	{
		char m_aDefinition[512] = {};
		SSequence m_Sequence{};
		int m_Bind = 0;
		int m_Mode = ONCE;
		bool m_Enabled = false;
		bool m_Running = false;
		bool m_KeyDown = false;
		int m_Step = 0;
		int m_Owned = 0;
		int64_t m_NextMs = 0;
	};
	std::array<SSlot, NUM_MACROS> m_aSlots{};
	bool m_FirePulse = false;

	static void Stop(SSlot &Slot)
	{
		Slot.m_Running = false;
		Slot.m_Owned = 0;
		Slot.m_Step = 0;
		Slot.m_NextMs = 0;
	}
	static void Execute(SSlot &Slot, int Action, bool &FirePulse)
	{
		switch(Action)
		{
		case LEFT_ON: Slot.m_Owned |= OWN_LEFT; break;
		case LEFT_OFF: Slot.m_Owned &= ~OWN_LEFT; break;
		case RIGHT_ON: Slot.m_Owned |= OWN_RIGHT; break;
		case RIGHT_OFF: Slot.m_Owned &= ~OWN_RIGHT; break;
		case JUMP_ON: Slot.m_Owned |= OWN_JUMP; break;
		case JUMP_OFF: Slot.m_Owned &= ~OWN_JUMP; break;
		case HOOK_ON: Slot.m_Owned |= OWN_HOOK; break;
		case HOOK_OFF: Slot.m_Owned &= ~OWN_HOOK; break;
		case FIRE_PULSE: FirePulse = true; break;
		case RELEASE_ALL: Slot.m_Owned = 0; break;
		default: break;
		}
	}
public:
	void Configure(int Index, const char *pDefinition, int Bind, int Mode, bool Enabled)
	{
		if(Index < 0 || Index >= NUM_MACROS)
			return;
		auto &Slot = m_aSlots[Index];
		if(!pDefinition)
			pDefinition = "";
		if(std::strcmp(Slot.m_aDefinition, pDefinition) != 0)
		{
			SSequence Parsed;
			if(!Parse(pDefinition, Parsed))
				Parsed = {};
			std::snprintf(Slot.m_aDefinition, sizeof(Slot.m_aDefinition), "%s", pDefinition);
			Slot.m_Sequence = Parsed;
			Stop(Slot);
		}
		if(Slot.m_Bind != Bind || Slot.m_Mode != Mode || Slot.m_Enabled != Enabled)
		{
			Stop(Slot);
			Slot.m_KeyDown = false;
		}
		Slot.m_Bind = Bind;
		Slot.m_Mode = std::clamp(Mode, (int)ONCE, (int)TOGGLE);
		Slot.m_Enabled = Enabled;
	}
	void KeyEvent(int Key, bool Pressed, bool Repeat, int64_t NowMs)
	{
		for(auto &Slot : m_aSlots)
		{
			if(!Slot.m_Enabled || Slot.m_Bind <= 0 || Slot.m_Bind != Key || Slot.m_Sequence.m_Count == 0)
				continue;
			if(!Pressed)
			{
				Slot.m_KeyDown = false;
				if(Slot.m_Mode == HOLD)
					Stop(Slot);
				continue;
			}
			if(Repeat || Slot.m_KeyDown)
				continue;
			Slot.m_KeyDown = true;
			if(Slot.m_Mode == TOGGLE && Slot.m_Running)
				Stop(Slot);
			else
			{
				Stop(Slot);
				Slot.m_Running = true;
				Slot.m_NextMs = NowMs;
			}
		}
	}
	void Tick(int64_t NowMs, bool Allowed)
	{
		if(!Allowed)
		{
			Cancel();
			return;
		}
		for(auto &Slot : m_aSlots)
		{
			// At most MAX_STEPS updates per frame even with zero-delay steps.
			for(int Budget = 0; Slot.m_Running && NowMs >= Slot.m_NextMs && Budget < MAX_STEPS; ++Budget)
			{
				if(Slot.m_Step >= Slot.m_Sequence.m_Count)
				{
					if(Slot.m_Mode == ONCE || (Slot.m_Mode == HOLD && !Slot.m_KeyDown))
					{
						Stop(Slot);
						break;
					}
					Slot.m_Step = 0;
				}
				const auto &Step = Slot.m_Sequence.m_aSteps[Slot.m_Step++];
				Execute(Slot, Step.m_Action, m_FirePulse);
				Slot.m_NextMs = NowMs + std::max(Step.m_DelayMs, 1);
			}
		}
	}
	void Cancel()
	{
		for(auto &Slot : m_aSlots)
		{
			Stop(Slot);
			Slot.m_KeyDown = false;
		}
		m_FirePulse = false;
	}
	int Owned() const
	{
		int Mask = 0;
		for(const auto &Slot : m_aSlots)
			Mask |= Slot.m_Owned;
		return Mask;
	}
	bool TakeFirePulse()
	{
		const bool Pulse = m_FirePulse;
		m_FirePulse = false;
		return Pulse;
	}
	int ActiveCount() const
	{
		int Count = 0;
		for(const auto &Slot : m_aSlots)
			Count += Slot.m_Running;
		return Count;
	}
	bool Running(int Index) const { return Index >= 0 && Index < NUM_MACROS && m_aSlots[Index].m_Running; }
};

// Normalized coordinates use [0,10000], independent of current pixel resolution.
inline float HudCoordinate(int Normalized, float Extent, float ElementExtent)
{
	if(!std::isfinite(Extent) || !std::isfinite(ElementExtent) || Extent <= 0.0f)
		return 0.0f;
	return std::clamp(std::clamp(Normalized, 0, 10000) / 10000.0f * Extent, 0.0f, std::max(0.0f, Extent - std::max(0.0f, ElementExtent)));
}
inline int HudNormalize(float Position, float Extent)
{
	if(!std::isfinite(Position) || !std::isfinite(Extent) || Extent <= 0.0f)
		return 0;
	return (int)(std::clamp(Position / Extent, 0.0f, 1.0f) * 10000.0f + 0.5f);
}

static constexpr int NUM_HUD_MODULES = 7;
enum EHudModule { HUD_HOTKEYS, HUD_IDENTITY, HUD_PERFORMANCE, HUD_DUMMY, HUD_STAFF, HUD_INPUT, HUD_EFFECTS };
enum EHudPreset { HUD_MINIMAL, HUD_STREAMING, HUD_COMPETITIVE, HUD_CUSTOM };
struct SHudPreset
{
	std::array<int, NUM_HUD_MODULES> m_aEnabled{};
	std::array<int, NUM_HUD_MODULES> m_aX{{300, 300, 8200, 300, 8200, 300, 7600}};
	std::array<int, NUM_HUD_MODULES> m_aY{{900, 250, 250, 3200, 1800, 7500, 1800}};
};
inline SHudPreset HudPreset(int Preset)
{
	SHudPreset Result;
	if(Preset == HUD_MINIMAL)
		Result.m_aEnabled = {{0, 1, 0, 0, 0, 0, 0}};
	else if(Preset == HUD_STREAMING)
		Result.m_aEnabled = {{1, 1, 0, 1, 0, 1, 1}};
	else if(Preset == HUD_COMPETITIVE)
		Result.m_aEnabled = {{1, 1, 1, 1, 0, 1, 0}};
	else
		Result.m_aEnabled = {{1, 1, 0, 1, 0, 0, 0}};
	return Result;
}
// Snap leading/trailing edges and the center. Caller may also align to other
// widgets with the same threshold. All values are in the caller's canvas units.
inline float HudSnap(float Position, float Extent, float ElementExtent, float Threshold = 4.0f)
{
	if(!std::isfinite(Position) || !std::isfinite(Extent) || !std::isfinite(ElementExtent))
		return 0.0f;
	const float End = std::max(0.0f, Extent - std::max(0.0f, ElementExtent));
	Position = std::clamp(Position, 0.0f, End);
	const float aTargets[] = {0.0f, End / 2.0f, End};
	for(float Target : aTargets)
		if(std::abs(Position - Target) <= std::max(0.0f, Threshold))
			return Target;
	return Position;
}
// Shared bounds for the in-game HUD and editor. Dimensions are DDNet HUD units
// (height 300), not pixels. Uniform downscaling to a smaller canvas is external.
inline void HudModuleExtent(int Module, int GlobalScale, int ModuleScale, int Padding, int FontSize, int KeySize, float &Width, float &Height)
{
	const float Scale = std::clamp(GlobalScale, 65, 150) * std::clamp(ModuleScale, 65, 150) / 10000.0f;
	const float Pad = (float)std::clamp(Padding, 0, 16);
	const float Font = (float)std::clamp(FontSize, 5, 12);
	const float Row = Font + 4.0f;
	const float Key = (float)std::clamp(KeySize, 12, 32);
	const int aRows[NUM_HUD_MODULES] = {6, 1, 2, 2, 2, 0, 7};
	Module = std::clamp(Module, 0, NUM_HUD_MODULES - 1);
	Width = (Module == HUD_INPUT ? Key * 3.0f + 4.0f : Font * (Module == HUD_IDENTITY ? 19.0f : 24.0f)) + Pad * 2.0f;
	Height = Module == HUD_INPUT ? Key * 2.0f + 2.0f + Row + Pad * 2.0f : Row * aRows[Module] + Pad * 2.0f;
	Width *= Scale;
	Height *= Scale;
}
}
#endif
