#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatNetCombatTest.generated.h"

class ABoatPawn;
class ABoatMissile;

/** Spawned only by -NetCombatTest: two real network players, controlled shots. */
UCLASS(NotPlaceable, Transient)
class ABoatNetCombatTest : public AActor
{
	GENERATED_BODY()
public:
	ABoatNetCombatTest();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void Freeze(ABoatPawn* Boat);
	void SetPhase(uint8 Next);
	void TickServer(float DeltaSeconds);
	void TickClient(float DeltaSeconds);
	void TrackHostMissile();

	UFUNCTION(Server, Reliable)
	void ServerConfirm(uint8 SeenPhase, float SeenHealth);

	UPROPERTY(Replicated)
	TObjectPtr<ABoatPawn> Shooter;
	UPROPERTY(Replicated)
	TObjectPtr<ABoatPawn> Victim;
	UPROPERTY(Replicated)
	TObjectPtr<ABoatMissile> Tracked;
	UPROPERTY(Replicated)
	uint8 Phase = 0;

	uint8 LocalPhase = 0;
	bool bActed = false;
	bool bClientSawLock = false;
	float Age = 0.f;
	float ReportClock = 0.f;
};
