#pragma once

#include "CoreMinimal.h"

/**
 * One sine component of the sea surface.
 *
 * Only vertical displacement is used. That is a deliberate choice: the water
 * mesh and the boat's buoyancy probes then read exactly the same number for
 * any point, so a boat never floats above or sinks below the surface you see.
 */
struct FOceanWave
{
	/** Crest height above the mean surface, cm. */
	float Amplitude = 0.f;
	/** Crest-to-crest distance, cm. */
	float Wavelength = 0.f;
	/** Travel direction in the XY plane. */
	FVector2D Direction = FVector2D(1.0, 0.0);
	/** Wave number, 2 pi / Wavelength. */
	float WaveNumber = 0.f;
	/** Angular frequency, sqrt(g * k): long swells travel faster than short chop. */
	float AngularSpeed = 0.f;
	/** Keeps the components from all peaking at the origin at once. */
	float Phase = 0.f;
};

/**
 * The sea surface, shared by everything that touches water: the water mesh,
 * boat buoyancy, torpedoes, mines and floating crates.
 *
 * Every function is pure maths on (position, time), so each machine works it
 * out independently and they all agree, with no network traffic. Pass the
 * server's clock as Seconds (ABoatRaceGameState::GetServerNow) so the sea is
 * at the same point in its cycle for all four players.
 */
namespace OceanWaves
{
	/** Height of the mean water surface. */
	constexpr float MeanLevel = 0.f;

	/** Gravity used for the wave speeds, cm/s^2. */
	constexpr float Gravity = 980.f;

	/**
	 * Sea state, set once at startup from WaveRoughness in DefaultGame.ini.
	 * 0 = flat calm, 1 = the default lively sea, 2 = storm.
	 */
	extern float Roughness;

	/** Sets Roughness, clamped to a sane range. */
	void SetRoughness(float NewRoughness);

	/** The six components of the sea. */
	const TArray<FOceanWave>& GetWaves();

	/** Surface height at a world XY. */
	float GetHeight(const FVector2D& WorldXY, float Seconds);

	/** Surface height, and the slope (dz/dx, dz/dy) at the same point. */
	float GetHeightAndSlope(const FVector2D& WorldXY, float Seconds, FVector2D& OutSlope);

	/** Unit normal of the surface, pointing up out of the water. */
	FVector GetNormal(const FVector2D& WorldXY, float Seconds);

	/** How fast the surface itself is rising at a point, cm/s. */
	float GetVerticalSpeed(const FVector2D& WorldXY, float Seconds);

	/** Crest height of the roughest possible sea, used for mesh bounds. */
	float GetMaxHeight();
}
