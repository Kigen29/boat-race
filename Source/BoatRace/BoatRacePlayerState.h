#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BoatRacePlayerState.generated.h"

/** One player's race and combat record. Written by the server, read by every HUD. */
UCLASS()
class ABoatRacePlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: clear everything for a new race. */
	void ResetRace();

	/** Grid slot and colour, 0..3. -1 until the server assigns one. */
	UPROPERTY(Replicated)
	int32 SlotIndex = -1;

	/** -1 until the start line is crossed for the first time. */
	UPROPERTY(Replicated)
	int32 LapsCompleted = -1;

	/** The gate this player has to pass next. */
	UPROPERTY(Replicated)
	int32 NextGate = 0;

	/** Gates passed this race: the main sort key for race position. */
	UPROPERTY(Replicated)
	int32 GatesPassed = 0;

	UPROPERTY(Replicated)
	bool bFinished = false;

	/** Knocked out in an elimination race (also counts as finished). */
	UPROPERTY(Replicated)
	bool bEliminated = false;

	/** Race time at the finish, seconds. */
	UPROPERTY(Replicated)
	float FinishTime = 0.f;

	/** 1 = winner. 0 until finished. */
	UPROPERTY(Replicated)
	int32 FinishPosition = 0;

	/** Fastest completed lap, seconds. 0 until one lap is done. */
	UPROPERTY(Replicated)
	float BestLapTime = 0.f;

	/** Opponents wrecked by this player's weapons. */
	UPROPERTY(Replicated)
	int32 Hits = 0;

	/** Times this player has been wrecked. */
	UPROPERTY(Replicated)
	int32 Wrecks = 0;

	/** Server only: race time when the current lap began. */
	float LapStartTime = 0.f;
};
