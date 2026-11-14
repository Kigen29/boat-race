#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatCoins.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Gold coins laid along the course in short rows, a different line on each
 * leg, so chasing them takes you off the ideal line now and then.
 *
 * Personal, like the coins in most racing games: every machine lays its own
 * and only its own boat collects them, so nothing is sent and nobody steals
 * yours. A collected coin comes back 25 seconds later. Spent in the garage.
 */
UCLASS()
class ACourseCoins : public AActor
{
	GENERATED_BODY()

public:
	ACourseCoins();

	virtual void Tick(float DeltaSeconds) override;

	/** Coins picked up since the green light. */
	int32 GetCollected() const { return Collected; }

	/** A fresh race: every coin back, count to zero. */
	void ResetRace();

	/** Power-up: coins within 35 m come to you for a while. */
	void StartMagnet(float Seconds);
	bool IsMagnetOn() const;

	static ACourseCoins* Find(const UWorld* World);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Coins")
	TObjectPtr<UInstancedStaticMeshComponent> Coins;

private:
	void Build();

	TArray<FVector2D> Spots;
	/** Real time each coin returns; 0 while it is there. */
	TArray<float> BackAt;
	TArray<FTransform> Transforms;
	int32 Collected = 0;
	float MagnetUntil = 0.f;
	int32 BuiltFor = -1;
	float Scale = 1.f;
	float LastMoved = -1.f;
};
