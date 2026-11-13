#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatProjectile.generated.h"

class ABoatPawn;
class ABoatRacePlayerState;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Damage and knockback in a radius, applied by the server.
 *
 * Shared by torpedoes, mines and anything else that goes off. The server is
 * the only machine that decides who was hit and for how much; the shove has to
 * be sent on to whichever machine is simulating each boat.
 */
namespace BoatBlast
{
	/** Spawns the effect, hurts boats in range and shoves them. Server only. */
	void Detonate(UWorld* World, const FVector& Origin, float Radius, float Damage,
		float Impulse, ABoatPawn* FiredBy);
}

/**
 * The visible part of an explosion: a fireball of several swelling flames, a
 * column of white water, chunks of debris thrown out on arcs, rolling black
 * smoke, a shock ring across the water and a flash of light, with the boom.
 *
 * Spawned by the server and replicated, then animated independently on each
 * machine, so no per-frame network traffic.
 */
UCLASS()
class ABoatExplosion : public AActor
{
	GENERATED_BODY()

public:
	ABoatExplosion();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Sets the size of the flash; call before the actor starts ticking. */
	void SetBlastRadius(float Radius);

protected:
	UPROPERTY(Replicated)
	float BlastRadius = 900.f;

	UPROPERTY(VisibleAnywhere, Category = "Explosion")
	TObjectPtr<UStaticMeshComponent> Flash;

	UPROPERTY(VisibleAnywhere, Category = "Explosion")
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere, Category = "Explosion")
	TObjectPtr<UPointLightComponent> Glare;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	/** One animated piece: a flame, a jet of water, a chunk or a smoke puff. */
	struct FBit
	{
		int32 Kind = 0;
		FVector Start = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Delay = 0.f;
		float Life = 1.f;
		float Size = 1.f;
	};
	TArray<FBit> Bits;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BitMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BitMaterials;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	float Age = 0.f;
	float Lifetime = 4.2f;
};

/**
 * A torpedo skimming the surface.
 *
 * Every machine works out where it is from the launch time, place and heading,
 * so it flies smoothly everywhere without its position being replicated. Only
 * the server tests for a hit.
 */
UCLASS()
class ABoatTorpedo : public AActor
{
	GENERATED_BODY()

public:
	ABoatTorpedo();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: record who fired and in which direction. */
	void InitProjectile(ABoatPawn* Shooter);

	UPROPERTY(Config)
	float Speed = 5500.f;

	UPROPERTY(Config)
	float RangeSeconds = 6.f;

	UPROPERTY(Config)
	float Damage = 60.f;

	UPROPERTY(Config)
	float BlastRadius = 900.f;

	UPROPERTY(Config)
	float BlastImpulse = 1100000.f;

protected:
	UPROPERTY(Replicated)
	FVector LaunchLocation = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FVector LaunchDirection = FVector::ForwardVector;

	/** Server time of launch, so every machine agrees how far along it is. */
	UPROPERTY(Replicated)
	float LaunchTime = 0.f;

	UPROPERTY()
	TObjectPtr<ABoatPawn> FiredBy;

	UPROPERTY(VisibleAnywhere, Category = "Torpedo")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Torpedo")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Torpedo")
	TObjectPtr<UStaticMeshComponent> Wake;

private:
	void Explode(const FVector& At);
	bool bExploded = false;
	FVector PreviousLocation = FVector::ZeroVector;
};

/**
 * A mine dropped in the water behind you.
 *
 * Floats where it was left, arms after a moment so it cannot catch the boat
 * that dropped it, then goes off when anything comes close.
 */
UCLASS()
class ABoatMine : public AActor
{
	GENERATED_BODY()

public:
	ABoatMine();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitProjectile(ABoatPawn* Shooter);

	/** Server only: a hazard laid on the course, armed at once, drifting slowly
	 *  round where it was laid, lasting until something sets it off. */
	void InitSeaMine(const FVector2D& InHome, float Phase);

	UPROPERTY(Config)
	float ArmSeconds = 1.2f;

	UPROPERTY(Config)
	float TriggerRadius = 350.f;

	UPROPERTY(Config)
	float LifetimeSeconds = 45.f;

	UPROPERTY(Config)
	float Damage = 75.f;

	UPROPERTY(Config)
	float BlastRadius = 700.f;

	UPROPERTY(Config)
	float BlastImpulse = 1300000.f;

protected:
	UPROPERTY(Replicated)
	FVector2D Anchor = FVector2D::ZeroVector;

	UPROPERTY(Replicated)
	float DropTime = 0.f;

	UPROPERTY()
	TObjectPtr<ABoatPawn> FiredBy;

	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Mine")
	TObjectPtr<UStaticMeshComponent> Light;

private:
	bool bExploded = false;
	bool bArmSounded = false;
	bool bSeaMine = false;
	FVector2D Home = FVector2D::ZeroVector;
	float DriftPhase = 0.f;
};

/**
 * A floating crate of ammunition.
 *
 * One of these sits at each point on the course. The server hands out the
 * ammunition and hides the crate; it comes back after a delay, so the fast
 * line and the armed line are not always the same line.
 */
UCLASS()
class AWeaponCrate : public AActor
{
	GENERATED_BODY()

public:
	AWeaponCrate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server only: place the crate and say what it holds. */
	void SetUpCrate(const FVector2D& Position, int32 InTorpedoes, int32 InMines, float InRespawnSeconds);

	UPROPERTY(Config)
	float PickupRadius = 420.f;

	/** Chance a crate holds a shield instead of ammunition. */
	UPROPERTY(Config)
	float ShieldChance = 0.2f;

	UPROPERTY(Config)
	float ShieldSeconds = 8.f;

protected:
	UPROPERTY(Replicated)
	bool bAvailable = true;

	UPROPERTY(Replicated)
	FVector2D Anchor = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Crate")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Crate")
	TObjectPtr<UStaticMeshComponent> Marker;

	virtual void BeginPlay() override;

private:
	bool bWasAvailable = true;
	int32 Torpedoes = 3;
	int32 Mines = 2;
	float RespawnSeconds = 15.f;
	float RespawnAtServerTime = 0.f;
};
