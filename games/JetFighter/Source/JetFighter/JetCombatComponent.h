#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "JetCombatComponent.generated.h"
class AJetTrainingTarget;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/** Bounded fixed-step local gun simulation; visuals never decide damage. */
UCLASS()
class UJetCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartTraining();
    void Advance(float Dt, bool bTrigger, bool bCanFire, const FVector& InheritedVelocityMetres);
    AJetTrainingTarget* GetTarget() const { return Target; }
    float GetHeat() const { return Heat; }
    bool IsOverheated() const { return bOverheated; }
    float GetHitSeconds() const { return HitSeconds; }
    int32 GetShotsFired() const { return ShotsFired; }
    int32 GetHits() const { return Hits; }
    int32 GetLiveRounds() const { return Rounds.Num(); }
    static constexpr float RangeMetres = 900.f;
private:
    friend struct FJetCombatTestAccess;
    struct FRound
    {
        FVector Position, Velocity;
        float Travelled = 0.f;
        uint64 Id = 0;
    };
    void Step(float Dt, bool bTrigger, bool bCanFire, const FVector& InheritedVelocityMetres);
    bool TraceSurface(const FVector& Start, const FVector& End, FVector& Contact) const;
    void RenderRounds();
    UPROPERTY() TObjectPtr<AJetTrainingTarget> Target;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Tracers;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlash;
    TArray<FRound> Rounds;
    double Accumulator = 0.;
    float Cooldown = 0.f;
    float Heat = 0.f;
    float HitSeconds = 0.f;
    float FlashSeconds = 0.f;
    bool bOverheated = false;
    int32 ShotsFired = 0;
    int32 Hits = 0;
    uint64 NextShotId = 1;
};
