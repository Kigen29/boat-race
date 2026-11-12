#pragma once

#include "CoreMinimal.h"

/** One checkpoint: an invisible line across the course between two buoys. */
struct FBoatGate
{
	FVector2D Center = FVector2D::ZeroVector;
	/** Buoy on the inside of the circuit. */
	FVector2D Inner = FVector2D::ZeroVector;
	/** Buoy on the outside of the circuit. */
	FVector2D Outer = FVector2D::ZeroVector;
	/** Unit direction boats must be travelling to score the gate. */
	FVector2D Forward = FVector2D::ZeroVector;
};

/** A solid round obstacle: rock pillar or debris barrel. */
struct FBoatCircle
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.f;
};

/** A floating wedge that throws a boat into the air. */
struct FBoatRamp
{
	FVector2D Center = FVector2D::ZeroVector;
	/** Heading the ramp climbs towards, degrees. */
	float Yaw = 0.f;
};

/** A rotating current that drags boats in and around. */
struct FBoatWhirlpool
{
	FVector2D Center = FVector2D::ZeroVector;
	float Radius = 0.f;
};

/** A barge shuttling back and forth across the course. */
struct FBoatBarge
{
	FVector2D From = FVector2D::ZeroVector;
	FVector2D To = FVector2D::ZeroVector;
	/** Seconds for one there-and-back trip. */
	float Period = 1.f;
};

/**
 * The circuit and everything fixed in the bay, as plain data.
 *
 * The server counts laps from it, every client builds the same scenery from it,
 * and the boats read the obstacles from it, so no map asset is needed and the
 * four machines never disagree about where anything is.
 *
 * All distances are Unreal units (1 uu = 1 cm). The layout is generated and
 * checked by Scripts/course_layout.py into BoatTrackData.h.
 */
namespace BoatTrack
{
	constexpr int32 MaxPlayers = 4;

	/** Height of the mean water surface. */
	constexpr float WaterZ = 0.f;

	/** How far the hull's centre floats above the mean surface at rest. */
	constexpr float BoatRideHeight = 45.f;

	/** Boats are held inside this rectangle. */
	constexpr float BoundsX = 72000.f;
	constexpr float BoundsY = 64000.f;

	/** Checkpoints in racing order. Gate 0 is the start / finish line. */
	const TArray<FBoatGate>& GetGates();
	int32 NumGates();

	/** Half the distance between a gate's two buoys. */
	float GetGateHalfWidth();

	/** Total length of one lap, cm. */
	float GetLapLength();

	/** Distance along the racing line from the start line to this gate, cm. */
	float GetDistanceToGate(int32 GateIndex);

	const TArray<FBoatCircle>& GetRocks();
	const TArray<FBoatRamp>& GetRamps();
	const TArray<FBoatWhirlpool>& GetWhirlpools();
	const TArray<FVector2D>& GetCratePoints();
	const TArray<FVector2D>& GetDebrisPoints();
	const TArray<FBoatBarge>& GetBarges();

	/** Where a barge is at a given time. Same answer on every machine. */
	FVector2D GetBargeLocation(const FBoatBarge& Barge, float Seconds);

	/** Starting grid for player slot 0..3: two rows of two behind the line. */
	FTransform GetGridTransform(int32 Slot);

	/** Where a boat restarts after a reset or a wreck, just past GateIndex. */
	FTransform GetGateResetTransform(int32 GateIndex);

	/** True if moving From -> To passes through the gate in the racing direction. */
	bool CrossesGate(const FBoatGate& Gate, const FVector2D& From, const FVector2D& To);

	/** Identification colour for player slot 0..3. */
	FLinearColor GetPlayerColor(int32 Slot);
}
