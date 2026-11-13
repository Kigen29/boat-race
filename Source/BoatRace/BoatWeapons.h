#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatWeapons.generated.h"

class ABoatPawn;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A homing missile. It locks on to the nearest boat ahead when fired, skims
 * the waves and turns after it; flares make it lose the lock and chase the
 * decoy instead. Because it steers, its path cannot be worked out on every
 * machine from the launch alone like a torpedo's: the server flies it and
 * its movement is replicated.
 */
UCLASS(Config = Game)
class ABoatMissile : public AActor
{
	GENERATED_BODY()

public:
	ABoatMissile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: who fired it, and lock on to a target ahead. */
	void Launch(ABoatPawn* Shooter);

	/** The boat it is chasing, so that boat's HUD can warn of the lock. */
	ABoatPawn* GetTarget() const { return Target; }

	UPROPERTY(Config)
	float Speed = 6500.f;

	/** Degrees per second it can turn. */
	UPROPERTY(Config)
	float TurnRate = 70.f;

	UPROPERTY(Config)
	float LockRange = 30000.f;

	UPROPERTY(Config)
	float Damage = 45.f;

	UPROPERTY(Config)
	float BlastRadius = 800.f;

protected:
	UPROPERTY(Replicated)
	TObjectPtr<ABoatPawn> Target;

	UPROPERTY()
	TObjectPtr<ABoatPawn> FiredBy;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UStaticMeshComponent> Exhaust;

	UPROPERTY(VisibleAnywhere, Category = "Missile")
	TObjectPtr<UPointLightComponent> Glow;

	/** Smoke trail: a ring of soft puffs left behind, recycled. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Trail;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> TrailMaterials;

private:
	void Explode();
	FVector Decoy = FVector::ZeroVector;
	bool bDecoyed = false;
	float Age = 0.f;
	float TrailClock = 0.f;
	int32 NextTrail = 0;
	TArray<float> TrailAge;
	bool bExploded = false;
};

/**
 * Flares: a burst of burning magnesium balls arcing out behind a boat. Any
 * missile locked on to that boat breaks off and chases them instead.
 * Spawned by the server, replicated, animated on each machine from its spawn.
 */
UCLASS()
class AFlareBurst : public AActor
{
	GENERATED_BODY()

public:
	AFlareBurst();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Balls;

	UPROPERTY(VisibleAnywhere, Category = "Flare")
	TObjectPtr<UPointLightComponent> Light;

private:
	TArray<FVector> Velocities;
	float Age = 0.f;
};

/**
 * A grenade lobbed by the co-driver: a short arc ahead and to the side,
 * going off when it hits the water. Server simulated, movement replicated.
 */
UCLASS(Config = Game)
class ABoatGrenade : public AActor
{
	GENERATED_BODY()

public:
	ABoatGrenade();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void Throw(ABoatPawn* Thrower);

	UPROPERTY(Config)
	float Damage = 30.f;

	UPROPERTY(Config)
	float BlastRadius = 650.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Grenade")
	TObjectPtr<UStaticMeshComponent> Fuse;

	UPROPERTY()
	TObjectPtr<ABoatPawn> FiredBy;

private:
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	bool bExploded = false;
};
