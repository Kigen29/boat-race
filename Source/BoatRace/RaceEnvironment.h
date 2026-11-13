#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoatWake.h"
#include "RaceWeather.h"
#include "RaceEnvironment.generated.h"

class ABayTerrain;
class AWaterSurface;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

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
	/** The physics body: a plain cylinder, hidden when the barrel model is there. */
	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Debris")
	TObjectPtr<UStaticMeshComponent> Look;
};

/**
 * Everything in the bay that is not a boat: sky, light, weather, water, land,
 * the course and its obstacles.
 *
 * Built from BoatTrack data, which is identical on every machine, so almost
 * none of it is replicated: each machine makes its own copy and animates it
 * from the shared clock. Only the things the rules care about (ammo crates and
 * debris) are spawned by the server. The weather is one replicated byte on
 * the game state; this actor turns it into light, fog, cloud, rain or snow.
 */
UCLASS(Config = Game)
class ARaceEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ARaceEnvironment();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Sea state: 0 glass, 1 the default lively sea, 2 a storm. Each weather
	 *  multiplies it (a storm by about 1.5, sun by 0.6). */
	UPROPERTY(Config)
	float WaveRoughness = 1.f;

	/** Seconds between a crate being taken and coming back. */
	UPROPERTY(Config)
	float CrateRespawnSeconds = 15.f;

	UPROPERTY(Config)
	int32 CrateTorpedoes = 3;

	UPROPERTY(Config)
	int32 CrateMines = 2;

	/** Raindrops or snowflakes around the camera. Lower on weak graphics. */
	UPROPERTY(Config)
	int32 PrecipitationCount = 1400;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UDirectionalLightComponent> Sun;

	/** Moonlight: lights the scene at night without turning the sky blue. */
	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UDirectionalLightComponent> Moon;

	/** Exposure per weather, so night stays dark and fog stays bright. */
	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UPostProcessComponent> Grade;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UVolumetricCloudComponent> Clouds;

	UPROPERTY(VisibleAnywhere, Category = "Environment")
	TObjectPtr<UExponentialHeightFogComponent> Haze;

private:
	void ApplyWeather(ERaceWeather NewWeather);
	void BuildGates();
	void BuildLaneRopes();
	void BuildRocks();
	void BuildRamps();
	void BuildWhirlpools();
	void BuildBarges();
	void BuildLogs();
	void BuildStartGantry();
	void BuildSky();
	void BuildIceFloes();
	void BuildTraffic();
	void UpdateTraffic(float Seconds, float DeltaSeconds);
	void ApplyGrade(const FWeatherLook& Look);
	void SpawnServerActors();
	void UpdateFloatingPieces(float Seconds);
	void UpdateRopes(float Seconds);
	void UpdateWhirlpools(float Seconds);
	void UpdatePrecipitation(float Seconds, float DeltaSeconds);
	void UpdateRipples(float Seconds);
	void UpdateSky(float Seconds, float DeltaSeconds);
	void UpdateStartLights();
	void HighlightNextGate(float Seconds);
	void RepaintGates();

	UStaticMeshComponent* AddPiece(UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation,
		const FVector& Scale, const FLinearColor& Color, bool bBlocking, UMaterialInstanceDynamic** OutMaterial = nullptr);

	/** A Blender model, sized from its Blender height, painted. Null if not imported. */
	UStaticMeshComponent* AddModel(const TCHAR* Name, float ModelHeightMetres, const FVector& Location,
		const FRotator& Rotation, float Size, const FLinearColor& Paint = FLinearColor(0.f, 0.f, 0.f, 0.f));

	void Float(UStaticMeshComponent* Component, const FVector2D& Anchor, float Rise, float Yaw, bool bTilts);

	/** A component that rides the waves at a fixed XY. */
	struct FFloatingPiece
	{
		TObjectPtr<UStaticMeshComponent> Component;
		FVector2D Anchor = FVector2D::ZeroVector;
		float Rise = 0.f;
		float Yaw = 0.f;
		/** Lets tall things lean with the swell instead of standing rigid. */
		bool bTilts = false;
	};

	struct FRopeFloat
	{
		FVector2D Anchor;
		float Yaw = 0.f;
	};

	struct FSwirl
	{
		int32 Pool = 0;
		float Radius = 0.f;
		float Angle = 0.f;
		float Speed = 0.f;
	};

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	/** Every Paint slot on every gate pylon, with the gate it belongs to. */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GateMaterials;
	TArray<int32> GateOfMaterial;
	TArray<FLinearColor> GateBaseColors;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> GatePylons;
	TArray<FLinearColor> PylonColors;

	UPROPERTY()
	TObjectPtr<AWaterSurface> Water;

	UPROPERTY()
	TObjectPtr<ABayTerrain> Terrain;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BargePieces;
	TArray<int32> BargeOfPiece;
	TArray<float> BargePieceRise;

	UPROPERTY()
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> RopeInstances;
	TArray<FRopeFloat> RopeFloats[2];
	float RopeRefresh = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> SwirlPieces;
	TArray<FSwirl> Swirls;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> StartLamps;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Precipitation;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PrecipitationMaterial;
	TArray<FVector> PrecipitationSeeds;
	TArray<FTransform> PrecipitationTransforms;
	EPrecipitation ShownPrecipitation = EPrecipitation::None;
	/** How far the wind has carried the rain and the clouds so far. Kept as a
	 *  running sum: multiplying today's wind by the whole elapsed time would
	 *  throw everything sideways every time the wind shifted. */
	FVector PrecipitationDrift = FVector::ZeroVector;
	FVector CloudDrift = FVector::ZeroVector;
	FVector2D SmoothedWind = FVector2D::ZeroVector;

	/** Rings spreading where raindrops hit the water. */
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Ripples;
	TArray<FTransform> RippleTransforms;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CloudDeck;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudDeckMaterial;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Cumulus;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CumulusMaterial;
	TArray<FVector> CumulusSeeds;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BoltPieces;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> IceFloes;

	/** A boat going about its business off the course: a stadium-shaped
	 *  lap of open water, driven by the shared clock so every player sees it
	 *  in the same place. */
	struct FTrafficBoat
	{
		FVector2D Centre;
		FVector2D Along;
		float Straight = 0.f;   // cm, half the straight
		float Radius = 0.f;     // cm, the turn at each end
		float Speed = 0.f;      // cm/s
		float Phase = 0.f;      // cm along the lap at time zero
		float ModelYaw = 0.f;
		TObjectPtr<UStaticMeshComponent> Look;
		TObjectPtr<UStaticMeshComponent> Solid;
		TObjectPtr<class UAudioComponent> Engine;
		float Length = 0.f;
		FBoatWakeTrail Wake;
	};
	TArray<FTrafficBoat> Traffic;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> TrafficRefs;

	TArray<FFloatingPiece> FloatingPieces;
	int32 HighlightedGate = -1;
	int32 AppliedWeather = -1;
	float BaseSunIntensity = 8.f;
	float BaseSkyIntensity = 1.f;
	float LastLightningAge = -1.f;
	bool bNight = false;
};
