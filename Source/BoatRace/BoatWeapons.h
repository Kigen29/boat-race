#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatWeapons.generated.h"

class ABoatPawn;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

class USceneComponent;

namespace BoatCombat
{
	/** Cover blocks both assisted gunfire and missile acquisition. */
	bool HasClearShot(const ABoatPawn* Shooter, const ABoatPawn* Target);
}

/**
 * What the deck gun looks like when it fires: a muzzle flash, tracers down
 * the range, spouts where rounds meet the sea and sparks where they meet a
 * hull.
 *
 * Cosmetic only. Every machine draws its own from the boat's one replicated
 * "firing" bit; which rounds actually hit is the server's business alone, so
 * nothing here is sent. Plain C++ owned by the boat, like its wake: the boat
 * keeps the components alive.
 */
class FDeckGunEffects
{
public:
	/** Creates the pooled meshes on an actor. Call once, from BeginPlay. */
	void Init(AActor* Owner, USceneComponent* AttachTo);
	bool IsReady() const { return Flash != nullptr; }

	/** One round on its way from Muzzle to End, which is a hull (sparks) or the sea. */
	void Fire(const FVector& Muzzle, const FVector& End, bool bHull);

	/** Call every frame, firing or not, so rounds already in the air land. */
	void Update(float DeltaSeconds, const FVector& Muzzle, const FVector& Forward);

private:
	struct FTracer
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 0.f;
		bool bHull = false;
		bool bLive = false;
	};
	struct FStrike
	{
		FVector Location = FVector::ZeroVector;
		float Age = 1.f;
		bool bHull = false;
		bool bLive = false;
	};
	void Strike(const FVector& Location, bool bHull);

	TArray<FTracer> Tracers;
	TArray<FStrike> Strikes;
	TArray<TObjectPtr<UStaticMeshComponent>> TracerMeshes;
	TArray<TObjectPtr<UStaticMeshComponent>> StrikeMeshes;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StrikeMaterials;
	TObjectPtr<UStaticMeshComponent> Flash;
	TObjectPtr<UPointLightComponent> FlashLight;
	float FlashAge = 1.f;
	int32 NextTracer = 0;
	int32 NextStrike = 0;
};

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

	/**
	 * Server only: who fired it and at whom. Wanted is the boat the shooter's
	 * sights had locked; with none the missile flies straight, unless
	 * bPickNearest (computer drivers), when it takes the nearest boat ahead.
	 */
	void Launch(ABoatPawn* Shooter, ABoatPawn* Wanted, bool bPickNearest);

	/** Whether Shooter's missile sights can hold Other: afloat, in range and
	 *  ahead. Slack loosens it a little, for the server checking a lock the
	 *  owner's machine saw a moment ago. */
	static bool CanLock(const ABoatPawn* Shooter, const ABoatPawn* Other, float Slack = 1.f);

	/** The boat it is chasing, so that boat's HUD can warn of the lock. */
	ABoatPawn* GetTarget() const { return Target; }

	UPROPERTY(Config)
	float Speed = 6500.f;

	/** Degrees per second it can turn. */
	UPROPERTY(Config)
	float TurnRate = 85.f;

	/** Seconds the sights must stay on a boat before the lock is good. */
	UPROPERTY(Config)
	float LockSeconds = 1.f;

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
