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

/**
 * The finish replay: every boat's last minute recorded fifteen times a
 * second on this machine, then shown again at half speed from trackside
 * cameras while the results are up. Local only; nothing is sent.
 */
UCLASS()
class AFinishReplay : public AActor
{
	GENERATED_BODY()

public:
	AFinishReplay();

	/** Call every frame while racing. */
	void Record(float DeltaSeconds);
	/** Notes the moment the winner crossed the line. */
	void MarkWin();
	/** Starts the playback; false when nothing was recorded. */
	bool Play(class APlayerController* Viewer);
	/** Advances the playback; false once it has finished (view restored). */
	bool Advance(float DeltaSeconds);
	void Stop();
	bool IsPlaying() const { return bPlaying; }
	void Clear();

private:
	struct FTrack
	{
		TWeakObjectPtr<class ABoatPawn> Boat;
		TArray<FTransform> Frames;
		/** Recording clock at each frame (frames land on whole game frames,
		 *  so they are not evenly spaced). */
		TArray<float> Times;
		TObjectPtr<UStaticMeshComponent> Copy;
		FTransform Sample(float At) const;
	};
	TArray<FTrack> Tracks;

	UPROPERTY()
	TObjectPtr<class ACameraActor> Camera;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Copies;

	TWeakObjectPtr<class APlayerController> Viewer;
	float Clock = 0.f;
	float NextSample = 0.f;
	float WinAt = -1.f;
	float PlayFrom = 0.f;
	float PlayTo = 0.f;
	float PlayAt = 0.f;
	int32 Focus = 0;
	bool bPlaying = false;
	FVector CameraSpot = FVector::ZeroVector;
	float SpotChosenAt = -100.f;
};
