#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

/**
 * The wake a boat leaves on the water: a band of churned white water behind
 * the transom that widens and fades, and the two arms of a Kelvin wake
 * opening out at about 19.5 degrees either side of the track.
 *
 * Laid down by distance travelled rather than by time (a sample every metre
 * and a half), which is how shipped games keep a wake even at any speed, and
 * drawn as one ribbon mesh that follows the waves. The foam pattern itself is
 * world-space noise in M_Foam, so it stays where it was left on the water.
 *
 * Plain C++ owned by an actor: the actor keeps the mesh component alive.
 */
class FBoatWakeTrail
{
public:
	/** Creates the ribbon on an actor. Call once, from BeginPlay. */
	void Init(AActor* Owner);

	/**
	 * Call every frame. Stern is the point the wake starts from; Speed in cm/s.
	 * Brightness matches the foam to the light (unlit material).
	 */
	void Update(const FVector& Stern, const FVector& Forward, float Speed, bool bInWater, float WaveTime,
		float Now, float Brightness, float Scale = 1.f);

	/** Clears the trail, e.g. after the boat is teleported. */
	void Reset() { Samples.Reset(); }

private:
	struct FSample
	{
		FVector2D Position;
		FVector2D Right;
		float Speed = 0.f;
		float Time = 0.f;
		float Strength = 0.f;
	};

	TArray<FSample> Samples;
	TObjectPtr<UProceduralMeshComponent> Mesh;
	TObjectPtr<UMaterialInstanceDynamic> Material;
	float LastBrightness = -1.f;
	/** The ribbon is rebuilt thirty times a second, not every frame. */
	float LastBuild = -1.f;
};
