#ifndef GAME_CLIENT_PRISM_EFFECTS_H
#define GAME_CLIENT_PRISM_EFFECTS_H

#include <base/vmath.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace PrismEffects
{
// Render-only storage. No references to prediction, physics, packets or RNG used by gameplay.
constexpr int MAX_PARTICLES = 512;
constexpr int MAX_TRAIL_SAMPLES = 64;
constexpr float TELEPORT_DISTANCE = 256.0f;

struct CParticle
{
	int m_Owner = -1;
	vec2 m_Position = vec2(0, 0);
	vec2 m_Velocity = vec2(0, 0);
	float m_Age = 0;
	float m_Lifetime = 1;
	float m_Phase = 0;
};

class CParticlePool
{
	std::array<CParticle, MAX_PARTICLES> m_aParticles{};
	int m_Count = 0;

public:
	void Reset()
	{
		for(auto &Particle : m_aParticles)
			Particle.m_Owner = -1;
		m_Count = 0;
	}
	void ClearOwner(int Owner)
	{
		for(auto &Particle : m_aParticles)
			if(Particle.m_Owner == Owner)
			{
				Particle.m_Owner = -1;
				--m_Count;
			}
	}
	void Advance(float Dt, int Limit)
	{
		Limit = std::clamp(Limit, 0, MAX_PARTICLES);
		Dt = std::clamp(Dt, 0.0f, 0.1f);
		for(auto &Particle : m_aParticles)
		{
			if(Particle.m_Owner < 0)
				continue;
			Particle.m_Age += Dt;
			if(Particle.m_Age >= Particle.m_Lifetime || m_Count > Limit)
			{
				Particle.m_Owner = -1;
				--m_Count;
			}
			else
				Particle.m_Position += Particle.m_Velocity * Dt;
		}
	}
	bool Spawn(const CParticle &Particle, int Limit)
	{
		if(Particle.m_Owner < 0 || !std::isfinite(Particle.m_Lifetime) || Particle.m_Lifetime <= 0 || m_Count >= std::clamp(Limit, 0, MAX_PARTICLES))
			return false;
		for(auto &Slot : m_aParticles)
			if(Slot.m_Owner < 0)
			{
				Slot = Particle;
				++m_Count;
				return true;
			}
		return false;
	}
	int Count() const { return m_Count; }
	const auto &Particles() const { return m_aParticles; }
};

struct CTrailSample
{
	vec2 m_Position = vec2(0, 0);
	double m_Time = 0;
};

class CTrail
{
	std::array<CTrailSample, MAX_TRAIL_SAMPLES> m_aSamples{};
	int m_Next = 0;
	int m_Count = 0;

public:
	void Reset() { m_Next = m_Count = 0; }
	int Count() const { return m_Count; }
	const CTrailSample &Sample(int OldestIndex) const { return m_aSamples[(m_Next - m_Count + OldestIndex + MAX_TRAIL_SAMPLES) % MAX_TRAIL_SAMPLES]; }
	// Returns true for a discontinuity; caller can clear other cosmetic state for this owner.
	bool Add(vec2 Position, double Time, float Interval, float Lifetime)
	{
		bool Discontinuity = false;
		if(m_Count)
		{
			const auto &Last = Sample(m_Count - 1);
			Discontinuity = Time < Last.m_Time || Time - Last.m_Time > 0.5 || distance(Position, Last.m_Position) > TELEPORT_DISTANCE;
			if(Discontinuity)
				Reset();
		}
		while(m_Count && Time - Sample(0).m_Time > std::clamp(Lifetime, 0.05f, 5.0f))
			--m_Count;
		if(m_Count && Time - Sample(m_Count - 1).m_Time < std::clamp(Interval, 0.008f, 0.25f))
			return Discontinuity;
		m_aSamples[m_Next] = {Position, Time};
		m_Next = (m_Next + 1) % MAX_TRAIL_SAMPLES;
		m_Count = std::min(m_Count + 1, MAX_TRAIL_SAMPLES);
		return Discontinuity;
	}
};

inline int QualityParticleLimit(int Quality) { return Quality <= 0 ? 96 : Quality == 1 ? 256 : MAX_PARTICLES; }
inline float Fade(float Age, float Lifetime, bool Enabled) { return Enabled ? std::clamp(1.0f - Age / std::max(Lifetime, 0.001f), 0.0f, 1.0f) : 1.0f; }
}
#endif
