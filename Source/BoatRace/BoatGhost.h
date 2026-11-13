#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatGhost.generated.h"

class UStaticMeshComponent;

/** One recorded lap: where the boat was, and when it reached each gate. */
struct FGhostLap
{
	float LapTime = 0.f;
	TArray<float> Times;
	TArray<FVector> Locations;
	TArray<FRotator> Rotations;
	/** Seconds into the lap at which each gate (by index) was passed; -1 if not. */
	TArray<float> Splits;

	bool IsValid() const { return LapTime > 0.f && Times.Num() > 1; }
	bool Sample(float Seconds, FVector& OutLocation, FRotator& OutRotation) const;

	/** Saved to Saved/BoatRace/Ghost.dat on this machine only. */
	bool Save() const;
	bool Load();
};

/**
 * Time trial's ghost: a see-through copy of the boat replaying the best lap
 * this machine has driven. Local only, never replicated, no collision.
 */
UCLASS()
class AGhostBoat : public AActor
{
	GENERATED_BODY()

public:
	AGhostBoat();

	void Show(const FGhostLap* Lap, float SecondsIntoLap);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ghost")
	TObjectPtr<UStaticMeshComponent> Hull;

	virtual void BeginPlay() override;
};
