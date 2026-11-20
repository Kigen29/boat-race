#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "JetTrainingTarget.generated.h"
class UJetHealthComponent;
class USphereComponent;
class UStaticMeshComponent;

/** Stationary, unarmed training drone: a real hit volume, hull and destruction. */
UCLASS()
class AJetTrainingTarget : public AActor
{
    GENERATED_BODY()
public:
    AJetTrainingTarget();
    virtual void BeginPlay() override;
    virtual void Tick(float Dt) override;
    bool ReceiveShot(float Damage, int32 SourceTeam, uint64 ShotId, const FVector& Impact);
    UJetHealthComponent* GetHealth() const { return Health; }
private:
    UPROPERTY() TObjectPtr<USphereComponent> HitVolume;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Model;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Blast;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Debris;
    UPROPERTY() TObjectPtr<UJetHealthComponent> Health;
    float HitFlashTime = 0.f;
    float DestructionTime = 0.f;
};
