#ifndef GAME_CLIENT_PRISM_EFFECTS_RENDER_H
#define GAME_CLIENT_PRISM_EFFECTS_RENDER_H

#include <base/color.h>
#include <base/math.h>
#include <engine/graphics.h>
#include <algorithm>
#include <cmath>

namespace PrismEffects
{
// Shared immediate-mode native primitives for the world and inspector preview.
// QuadParticle and QuadSegment must be called inside an untextured QuadsBegin/End.
inline void QuadSegment(IGraphics *pGraphics, vec2 From, vec2 To, float Width, ColorRGBA Color)
{
	const vec2 Delta = To - From;
	const float Length = length(Delta);
	if(Length < 0.001f || Width <= 0 || Color.a <= 0)
		return;
	const vec2 Normal = vec2(-Delta.y, Delta.x) * (Width * 0.5f / Length);
	pGraphics->SetColor(Color);
	IGraphics::CFreeformItem Item(From - Normal, From + Normal, To - Normal, To + Normal);
	pGraphics->QuadsDrawFreeform(&Item, 1);
}

inline void QuadParticle(IGraphics *pGraphics, vec2 Center, int Mode, float Size, float Phase, ColorRGBA Color)
{
	pGraphics->SetColor(Color);
	if(Mode == 2)
	{
		// Four-point sparkle, made from two thin diamonds.
		const vec2 X = direction(Phase) * Size;
		const vec2 Y(-X.y, X.x);
		IGraphics::CFreeformItem aQuads[] = {
			{Center - X, Center - Y * 0.22f, Center + Y * 0.22f, Center + X},
			{Center - Y, Center - X * 0.22f, Center + X * 0.22f, Center + Y}};
		pGraphics->QuadsDrawFreeform(aQuads, 2);
		return;
	}
	if(Mode == 3)
	{
		// Original heart silhouette: bounded 16-triangle fan.
		auto Point = [&](float T) {
			const float S = std::sin(T);
			return Center + vec2(S * S * S, -(13 * std::cos(T) - 5 * std::cos(2 * T) - 2 * std::cos(3 * T) - std::cos(4 * T)) / 16.0f) * Size;
		};
		for(int i = 0; i < 16; ++i)
		{
			IGraphics::CFreeformItem Item(Center, Point(i * 2 * pi / 16), Center, Point((i + 1) * 2 * pi / 16));
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
		return;
	}
	const vec2 X = direction(Phase) * Size;
	const vec2 Y(-X.y * 0.45f, X.x * 0.45f);
	IGraphics::CFreeformItem Item(Center - X, Center - Y, Center + Y, Center + X);
	pGraphics->QuadsDrawFreeform(&Item, 1);
	QuadSegment(pGraphics, Center - X * 0.7f, Center + X * 0.7f, std::max(0.4f, Size * 0.08f), ColorRGBA(Color.r * 0.55f, Color.g * 0.55f, Color.b * 0.55f, Color.a));
}

inline void DrawParticle(IGraphics *pGraphics, vec2 Center, int Mode, float Size, float Phase, ColorRGBA Color, bool Glow)
{
	pGraphics->TextureClear();
	pGraphics->QuadsBegin();
	pGraphics->QuadsSetRotation(0);
	if(Glow)
		QuadParticle(pGraphics, Center, Mode, Size * 1.6f, Phase, Color.WithAlpha(Color.a * 0.12f));
	QuadParticle(pGraphics, Center, Mode, Size, Phase, Color);
	pGraphics->QuadsEnd();
	pGraphics->SetColor(1, 1, 1, 1);
}

inline void DrawHighlight(IGraphics *pGraphics, vec2 Center, float Scale, float Width, float Rounding, ColorRGBA Color, float FillAlpha, float OutlineAlpha)
{
	const float W = 48 * Scale, H = 56 * Scale;
	const float X = Center.x - W / 2, Y = Center.y - H / 2;
	const float Radius = std::clamp(Rounding, Width, std::min(W, H) * 0.5f);
	pGraphics->TextureClear();
	pGraphics->DrawRect(X, Y, W, H, Color.WithAlpha(Color.a * FillAlpha), IGraphics::CORNER_ALL, Radius);
	pGraphics->QuadsBegin();
	pGraphics->QuadsSetRotation(0);
	const ColorRGBA Outline = Color.WithAlpha(Color.a * OutlineAlpha);
	QuadSegment(pGraphics, vec2(X + Radius, Y + Width / 2), vec2(X + W - Radius, Y + Width / 2), Width, Outline);
	QuadSegment(pGraphics, vec2(X + Radius, Y + H - Width / 2), vec2(X + W - Radius, Y + H - Width / 2), Width, Outline);
	QuadSegment(pGraphics, vec2(X + Width / 2, Y + Radius), vec2(X + Width / 2, Y + H - Radius), Width, Outline);
	QuadSegment(pGraphics, vec2(X + W - Width / 2, Y + Radius), vec2(X + W - Width / 2, Y + H - Radius), Width, Outline);
	const vec2 aCenters[] = {vec2(X + W - Radius, Y + H - Radius), vec2(X + Radius, Y + H - Radius), vec2(X + Radius, Y + Radius), vec2(X + W - Radius, Y + Radius)};
	pGraphics->SetColor(Outline);
	for(int Corner = 0; Corner < 4; ++Corner)
		for(int Step = 0; Step < 6; ++Step)
		{
			const vec2 A = direction((Corner + Step / 6.0f) * pi / 2);
			const vec2 B = direction((Corner + (Step + 1) / 6.0f) * pi / 2);
			IGraphics::CFreeformItem Item(aCenters[Corner] + A * Radius, aCenters[Corner] + A * (Radius - Width), aCenters[Corner] + B * Radius, aCenters[Corner] + B * (Radius - Width));
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
	pGraphics->QuadsEnd();
	pGraphics->SetColor(1, 1, 1, 1);
}
}
#endif
