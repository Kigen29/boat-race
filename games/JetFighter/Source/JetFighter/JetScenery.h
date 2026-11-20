#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterSurface.h"
#include "JetScenery.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;

/** A fixed-budget island mesh, airstrip and seeded puff clouds. */
UCLASS(Config=Game)
class AJetScenery : public AActor
{
    GENERATED_BODY()
public:
    AJetScenery();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    UProceduralMeshComponent* GetTerrainMesh() const { return Terrain; }
    UPROPERTY(Config) int32 CloudGroups = 16;
    UPROPERTY(Config) int32 CloudSeed = 6307;
    UPROPERTY(Config) float CloudWindMetres = 8.f;
private:
    void BuildTerrain();
    void BuildAirstrip();
    void BuildClouds();
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> Terrain;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Runway;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Markings;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Beacons;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Clouds;
    TArray<FTransform> CloudStarts;
};

/** Altitude-aware grid spacing, without changing the boat's water renderer. */
UCLASS(Config=Game)
class AJetWaterSurface : public AWaterSurface
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    static float GridScaleForAltitude(float HeightCm, float BaseCellCm);
    static float WaveDetailWeight(float HeightMetres, float GridRadius, float FadeHeightMetres);
    UPROPERTY(Config) float WaveFadeHeightMetres = 600.f;
private:
    friend struct FJetFlightTestAccess;
    void RefreshForView(const FVector& Eye);
    float BaseCellSize = 700.f;
    float SinceRefresh = 0.f;
    TArray<FVector> AirVertices, AirNormals;
    TArray<FVector2D> AirUVs;
    TArray<FProcMeshTangent> AirTangents;
};
