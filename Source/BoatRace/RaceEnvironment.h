#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RaceEnvironment.generated.h"

class AWaterSurface;
class UBoxComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UMaterialInstanceDynamic;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

UENUM()
enum class ERaceWeather : uint8
{
	/** Heavy overcast, grey-green sea, low light. */
	Storm,
	/** Broken cloud, blue-grey sea. */
	Overcast,
	/** Low sun, long reflections. */
	Clear
};

/**
 * A barrel floating on the course.
 *
 * Simulated by the server and replicated, so a boat or a blast scatters it and
 * everyone sees it go the same way.
 */
UCLASS()
class ADebrisBarrel : public AActor
{
	GENERATED_BODY()

public:
	ADebrisBarrel();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server only: shove the barrel away from an explosion. */
	void PushFromBlast(const FVector& Origin, float Radius, float Impulse);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UStaticMeshComponent> Body;
};

/**
 * Everything in the bay that is not a boat: sky, light, weather, water, the
 * gate buoys, the rock spires, the ramps, the whirlpools and the barges.
 *
 * Built from BoatTrack data, which is identical on every machine, so almost
 * none of it is replicated: each machine makes its own copy. Only the things
 * the rules care about (ammo crates and debris) are spawned by the server.
 */
UCLASS(Config = Game)
class ARaceEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ARaceEnvironment();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 0 storm, 1 overcast, 2 clear. */
	UPROPERTY(Config)
	int32 Weather = 0;

	/** Sea state: 0 glass, 1 the default lively sea, 2 a storm. The weather
	 *  setting nudges this (storm raises it, clear calms it). */
	UPROPERTY(Config)
	float WaveRoughness = 1.f;

	/** Seconds between a crate being taken and coming back. */
	UPROPERTY(Config)
	float CrateRespawnSeconds = 15.f;

	UPROPERTY(Config)
	int32 CrateTorpedoes = 3;

	UPROPERTY(Config)
	int32 CrateMines = 2;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UExponentialHeightFogComponent> Haze;

private:
	void ApplyWeather();
	void BuildGates();
	void BuildRocks();
	void BuildRamps();
	void BuildWhirlpools();
	void BuildBarges();
	void SpawnServerActors();
	void UpdateFloatingPieces(float Seconds);
	void HighlightNextGate(float Seconds);

	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
		const FVector& Scale, const FLinearColor& Color, bool bBlocking, UMaterialInstanceDynamic** OutMaterial = nullptr);

	/** A component that rides the waves at a fixed XY. */
	struct FFloatingPiece
	{
		TObjectPtr<UStaticMeshComponent> Component;
		FVector2D Anchor = FVector2D::ZeroVector;
		float Rise = 0.f;
		/** Lets tall poles lean with the swell instead of standing rigid. */
		bool bTilts = false;
	};

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GateMaterials;

	UPROPERTY()
	TObjectPtr<AWaterSurface> Water;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BargePieces;

	TArray<FFloatingPiece> FloatingPieces;
	TArray<FLinearColor> GateBaseColors;
	int32 MaterialsPerGate = 0;
	int32 HighlightedGate = -1;
};
