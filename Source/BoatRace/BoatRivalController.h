#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BoatRivalController.generated.h"

class ABoatPawn;

/**
 * A computer opponent that fills an empty grid slot, so a race always has
 * four boats in it.
 *
 * It drives the same boat with the same physics as a player, by setting the
 * throttle and rudder it would get from the keys: it reads the racing line a
 * little way ahead, swerves round rocks, barges and whirlpools, backs out when
 * stuck, and uses its torpedoes and mines on whoever is close. Runs only on
 * the server; everyone else sees its boat like any other.
 */
UCLASS()
class ABoatRivalController : public AAIController
{
	GENERATED_BODY()

public:
	ABoatRivalController();

	virtual void Tick(float DeltaSeconds) override;

	/** 0.6 a gentle opponent .. 1 flat out everywhere. */
	float Skill = 0.9f;

	/** Preferred offset from the racing line, metres, so the four do not queue. */
	float LaneOffset = 0.f;

private:
	FVector2D PickTarget(const ABoatPawn* Boat, int32 NextGate, float Speed) const;
	void UseWeapons(ABoatPawn* Boat, float Now);

	float StuckTime = 0.f;
	float ReverseUntil = -1.f;
	float NextWeaponTime = 0.f;
	float LaneDriftPhase = 0.f;
};
