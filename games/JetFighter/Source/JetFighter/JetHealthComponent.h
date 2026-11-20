#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JetHealthComponent.generated.h"

/** Local damage ownership. Multiplayer validation will live above this API. */
UCLASS()
class UJetHealthComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    float GetHull() const { return Hull; }
    float GetMaxHull() const { return MaxHull; }
    bool IsDestroyed() const { return Hull <= 0.f; }
    int32 GetTeam() const { return Team; }
    void ResetHull(float Maximum, int32 NewTeam);
    bool ApplyHit(float Damage, int32 SourceTeam, uint64 ShotId);
private:
    float MaxHull = 120.f;
    float Hull = 120.f;
    int32 Team = 1;
    // Shots are monotonically numbered by the encounter's single gun owner.
    uint64 LastShotId = 0;
};
