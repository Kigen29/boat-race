#pragma once

#include "CoreMinimal.h"
#include "BoatWake.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Pawn.h"
#include "BoatPawn.generated.h"

class ABoatRacePlayerState;
class UAudioComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USpotLightComponent;
class UCameraComponent;
class USceneComponent;
class USpringArmComponent;
class UStaticMeshComponent;

/** The boat state other machines need in order to draw and collide with it. */
USTRUCT()
struct FBoatNetState
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/** World velocity, cm/s, used to extrapolate between updates. */
	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	/** Rudder input -1..1, for the visible rudder and the wake. */
	UPROPERTY()
	float Steer = 0.f;

	/** Throttle input -1..1. */
	UPROPERTY()
	float Throttle = 0.f;

	/** Bumped every time the server repositions the boat. */
	UPROPERTY()
	uint8 TeleportCounter = 0;

	/** Boost firing, so everyone sees the extra spray and hears the engine. */
	UPROPERTY()
	bool bBoosting = false;
};

/**
 * A player's boat: a 1,200 kg rigid body floating on the OceanWaves surface.
 *
 * Nothing about how it moves is animated. Four buoyancy probes at the hull
 * corners push up in proportion to their depth, so pitching over a crest,
 * rolling in a turn, slamming into a trough and launching off a ramp all come
 * out of the physics. Steering and thrust are the only assisted parts: the
 * rudder targets a yaw rate through a control loop, which keeps handling
 * predictable without fighting the simulation.
 *
 * Networking: the player who owns the boat simulates it, so controls never
 * wait on the network, and reports the result to the server 30 times a second.
 * Everyone else sees a smoothed, extrapolated copy that is kinematic but still
 * solid, so ramming works both ways. The server decides laps, hits and damage.
 */
UCLASS(Config = Game)
class ABoatPawn : public APawn
{
	GENERATED_BODY()

public:
	ABoatPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Server-side control ---------------------------------------------
	/** Put the boat somewhere, upright and stopped. */
	void TeleportBoat(const FVector& Location, float Yaw);

	/** Position used for lap counting: reported for remote boats, actual for ours. */
	FVector GetRaceLocation() const;

	/** Hurt the boat. Returns true if this blow wrecked it. */
	bool ApplyDamage(float Amount, ABoatRacePlayerState* Attacker);

	/** Shove the boat; routed to the machine that is simulating it. */
	void ApplyExplosionImpulse(const FVector& Origin, float Strength);

	/** Repair and restart after a wreck. */
	void Revive(float InvulnerableSeconds);

	void GiveAmmo(int32 Torpedoes, int32 Mines);

	/** -PhotoTour only: swing the camera round to look at the boat. */
	void SetShowcaseCamera(bool bShowcase);

	/** For a computer driver: the throttle and rudder it would get from the keys. */
	void SetDriverInput(float Throttle, float Steer);

	/** A jolt of the camera, 0..1; only the local player's view moves. */
	void ShakeCamera(float Amount) { CameraShake = FMath::Max(CameraShake, Amount); }

	/** Server only: fire a torpedo, or drop a mine, as if the button were pressed. */
	void RequestFire(bool bMine);

	// ---- Read by the HUD and the game mode -------------------------------
	float GetForwardSpeed() const;
	float GetSpeedKmh() const;
	bool IsWrecked() const { return bWrecked; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	int32 GetTorpedoAmmo() const { return TorpedoAmmo; }
	int32 GetMineAmmo() const { return MineAmmo; }
	bool IsInvulnerable() const;
	/** How far out of the water the hull is, 0 afloat to 1 airborne. */
	float GetAirborneFraction() const { return 1.f - FMath::Clamp(LastSubmersion * BuoyancyMultiplier, 0.f, 1.f); }
	float GetTimeSinceHit() const;
	float GetThrottleInput() const { return InputThrottle; }

	/** 0..1 of the boost tank, and whether it is firing now. */
	float GetBoost() const { return Boost; }
	bool IsBoosting() const { return bBoostActive; }

	/** For a computer driver: fire the boost if there is enough in the tank. */
	void SetDriverBoost(bool bWant) { bWantBoost = bWant; }

	/** Server time at which the server will revive this boat. */
	float ReviveAtServerTime = 0.f;

	// ---- Lap counting state (server only, not replicated) ----------------
	FVector RaceLastLocation = FVector::ZeroVector;
	bool bRaceHasLastLocation = false;

	// ---- Hull and mass ---------------------------------------------------
	UPROPERTY(Config)
	float HullMassKg = 1200.f;

	/** Hull box: length, width, height in cm. */
	UPROPERTY(Config)
	float HullLength = 700.f;

	UPROPERTY(Config)
	float HullWidth = 240.f;

	UPROPERTY(Config)
	float HullHeight = 130.f;

	/** How deep the hull has to sink for full buoyancy, cm. */
	UPROPERTY(Config)
	float Draft = 60.f;

	// ---- Floating --------------------------------------------------------
	/** Upward push at full submersion, as a multiple of the boat's weight. */
	UPROPERTY(Config)
	float BuoyancyMultiplier = 2.6f;

	/** Heave damping, 1 = critically damped. Below 1 the boat rides waves more. */
	UPROPERTY(Config)
	float HeaveDampingRatio = 0.65f;

	/** How strongly the boat slides down the face of a wave. */
	UPROPERTY(Config)
	float WaveSlopeFactor = 1.f;

	// ---- Engine and handling --------------------------------------------
	/** Full-throttle acceleration, cm/s^2. */
	UPROPERTY(Config)
	float ThrustAcceleration = 1300.f;

	/** Reverse power as a fraction of forward. */
	UPROPERTY(Config)
	float ReverseFraction = 0.4f;

	/** Flat-out speed, cm/s. 2,900 is about 104 km/h. */
	UPROPERTY(Config)
	float ReferenceTopSpeed = 2900.f;

	/** How much harder the hull resists moving sideways than forwards. */
	UPROPERTY(Config)
	float LateralDragRatio = 7.f;

	/** Weight the hull carries on its own bow wave at full speed, as a
	 *  fraction. This lifts the nose and rides the boat higher with speed. */
	UPROPERTY(Config)
	float PlaningLiftFraction = 0.35f;

	/** Yaw rate at full rudder, degrees per second. */
	UPROPERTY(Config)
	float TurnRate = 76.f;

	/** How hard the rudder works to reach that yaw rate. */
	UPROPERTY(Config)
	float TurnResponse = 4.f;

	/** How far the boat leans into a turn at full speed, degrees. */
	UPROPERTY(Config)
	float MaxHeelDegrees = 14.f;

	/** How quickly it takes up that lean. */
	UPROPERTY(Config)
	float HeelResponse = 6.f;

	/** Roll past this many degrees brings in the righting assist. */
	UPROPERTY(Config)
	float SelfRightAngle = 70.f;

	/** How far the propeller hangs below the keel, cm. Too shallow and it
	 *  ventilates at rest and the boat will not pull away. */
	UPROPERTY(Config)
	float PropellerDepth = 40.f;

	// ---- Boost -----------------------------------------------------------
	/** Thrust while boosting, as a multiple of normal. Top speed rises by its square root. */
	UPROPERTY(Config)
	float BoostThrust = 1.7f;

	/** Seconds a full tank lasts. */
	UPROPERTY(Config)
	float BoostSeconds = 2.6f;

	/** Tank refilled per second of racing, and per second in the air off a ramp. */
	UPROPERTY(Config)
	float BoostTrickle = 0.035f;

	UPROPERTY(Config)
	float BoostPerAirSecond = 0.45f;

	// ---- Combat ----------------------------------------------------------
	UPROPERTY(Config)
	float MaxHealth = 100.f;

	UPROPERTY(Config)
	int32 StartTorpedoes = 1;

	UPROPERTY(Config)
	int32 StartMines = 1;

	UPROPERTY(Config)
	int32 MaxTorpedoes = 5;

	UPROPERTY(Config)
	int32 MaxMines = 3;

	UPROPERTY(Config)
	float FireCooldownSeconds = 0.9f;

	/** Which boat model to use: RaceBoat (the Blender boat, painted in the
	 *  player's colour) or Higgsfield (the generated SM_Boat). */
	UPROPERTY(Config)
	FString BoatModel = TEXT("RaceBoat");

	// ---- Higgsfield model tuning ----------------------------------------
	UPROPERTY(Config)
	float CustomMeshYawOffset = 0.f;

	UPROPERTY(Config)
	float CustomMeshZOffset = 0.f;

	UPROPERTY(Config)
	float CustomMeshScaleMultiplier = 1.f;

protected:
	// ---- Networking ------------------------------------------------------
	UFUNCTION(Server, Unreliable)
	void ServerSendMove(FBoatNetState Move);

	UFUNCTION(Client, Reliable)
	void ClientTeleport(FVector Location, float Yaw, uint8 TeleportSeq);

	UFUNCTION(Client, Reliable)
	void ClientImpulse(FVector Impulse);

	UFUNCTION(Client, Reliable)
	void ClientHitFeedback();

	UFUNCTION(Server, Reliable)
	void ServerFireTorpedo();

	UFUNCTION(Server, Reliable)
	void ServerDropMine();

	/** The owner hit something hard; the server decides what it costs. */
	UFUNCTION(Server, Unreliable)
	void ServerReportImpact(FVector_NetQuantize Location, float Speed, ABoatPawn* OtherBoat);

	/** Lets everyone else hear a crash the owner felt. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(FVector_NetQuantize Location, float Speed);

	UFUNCTION()
	void OnHullHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_NetState();

	UPROPERTY(ReplicatedUsing = OnRep_NetState)
	FBoatNetState NetState;

	UPROPERTY(Replicated)
	float Health = 100.f;

	UPROPERTY(Replicated)
	bool bWrecked = false;

	UPROPERTY(Replicated)
	float InvulnerableUntil = 0.f;

	UPROPERTY(Replicated)
	int32 TorpedoAmmo = 0;

	UPROPERTY(Replicated)
	int32 MineAmmo = 0;

	// ---- Components ------------------------------------------------------
	/** The simulated body. Everything else hangs off it. */
	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UBoxComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> HullMesh;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> BowMesh;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> CabinMesh;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> EngineMesh;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> FlagPoleMesh;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> FlagMesh;

	/** Holds /Game/Higgsfield/SM_Boat when that asset exists. */
	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> CustomMesh;

	/** Smoke plume shown while wrecked. */
	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UStaticMeshComponent> DamageMesh;

	/** Flames licking out of the engine when the hull is badly hurt. */
	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UPointLightComponent> FireLight;

	/** Running light at night. */
	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<USpotLightComponent> Headlight;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Boat")
	TObjectPtr<UCameraComponent> Camera;

private:
	void UpdatePhysicsMode();
	void ReadInput(float DeltaSeconds);
	void SimulatePhysics(float DeltaSeconds, float WaveTime);
	void ApplyBuoyancy(float WaveTime);
	void ApplyHydrodynamics();
	void ApplyEngineAndRudder(float DeltaSeconds);
	void ApplyWhirlpools();
	void KeepInsideBay();
	void SendMoveToServer(float DeltaSeconds);
	void FollowReportedState(float DeltaSeconds);
	void UpdateCosmetics(float DeltaSeconds);
	void TryApplyPlayerColor();
	void TryLoadCustomMesh();
	void BuildEffects();
	void UpdateEffects(float DeltaSeconds);
	void UpdateAudio(float DeltaSeconds);
	void EmitPuff(const FVector& Location, const FVector& Velocity, float Life, float StartSize, float EndSize,
		const FLinearColor& Color, float Opacity, float Gravity, float Flat = 0.8f, bool bOnWater = false);
	void ApplyWindAndShore();
	void HandleImpact(const FVector& Location, float Speed, ABoatPawn* OtherBoat);
	float GetWaveTime() const;
	bool CanDrive() const;

	/** Local cm offsets of the four buoyancy probes, set from the hull size. */
	FVector Probes[4];
	float BuoyancyPerProbe = 0.f;
	float DampingPerProbe = 0.f;
	float ForwardDragCoefficient = 0.f;
	float PlaningLiftCoefficient = 0.f;
	float YawInertia = 0.f;

	// Owner-side input.
	float InputThrottle = 0.f;
	float InputSteer = 0.f;

	// Owner-side networking.
	float SendAccumulator = 0.f;
	uint8 LocalTeleportSeq = 0;

	// Proxy-side smoothing.
	bool bHasNetState = false;
	float TimeSinceNetUpdate = 0.f;
	uint8 LastSeenTeleportCounter = 0;

	// Effects: a pool of soft puffs for spray, wake and smoke.
	struct FPuff
	{
		float Age = 0.f;
		float Life = 0.f;
		FVector Velocity = FVector::ZeroVector;
		float StartSize = 1.f;
		float EndSize = 1.f;
		FLinearColor Color = FLinearColor::White;
		float Opacity = 0.f;
		float Gravity = 0.f;
		/** Height as a fraction of width: 1 a ball of spray, 0.1 foam lying on the water. */
		float Flat = 0.8f;
		/** Foam rides the surface instead of flying. */
		bool bOnWater = false;
	};
	TArray<FPuff> Puffs;
	int32 NextPuff = 0;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> PuffMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PuffMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Flames;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> RoosterTail;

	/** Sheets of spray thrown off each side of the bow at speed. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SpraySheets;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SprayMaterials;

	FBoatWakeTrail Wake;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RoosterMaterial;

	UPROPERTY()
	TObjectPtr<UAudioComponent> EngineAudio;

	UPROPERTY()
	TObjectPtr<UAudioComponent> WaterAudio;

	float SprayAccumulator = 0.f;
	float Boost = 0.3f;
	bool bBoostActive = false;
	bool bWantBoost = false;
	bool bLookBack = false;
	void Rumble(float Intensity, float Seconds);
	float SmokeAccumulator = 0.f;
	float EngineRevs = 0.f;
	float AirTime = 0.f;
	float CameraShake = 0.f;
	float LastImpactTime = -10.f;
	FVector LastVelocity = FVector::ZeroVector;

	// Cosmetics and bookkeeping.
	bool bPhysicsActive = false;
	bool bColorApplied = false;
	bool bUsingCustomMesh = false;
	float LastSubmersion = 1.f;
	float LastFireTime = -100.f;
	float LastHitWallClock = -100.f;
};
