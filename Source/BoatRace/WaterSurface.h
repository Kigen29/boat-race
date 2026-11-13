#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "WaterSurface.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class UStaticMeshComponent;

/**
 * The visible sea.
 *
 * A grid of triangles around the local camera, displaced every frame by the
 * same OceanWaves maths the boats float on, with a flat plane filling the
 * horizon beyond it. The grid follows the camera in whole cells so the pattern
 * does not appear to slide along with you.
 *
 * Purely cosmetic and identical on every machine, so it is never replicated;
 * each machine spawns its own.
 */
UCLASS()
class AWaterSurface : public AActor
{
	GENERATED_BODY()

public:
	AWaterSurface();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Cells across the moving grid. 60 x 60 cells is 7,200 triangles. */
	UPROPERTY(Config)
	int32 GridCells = 60;

	/** Size of one cell, cm. 700 cm x 60 cells covers 420 m. */
	UPROPERTY(Config)
	float CellSize = 700.f;

	/** Seconds between rebuilds. 0 rebuilds every frame; raise it on weak hardware. */
	UPROPERTY(Config)
	float UpdateInterval = 0.f;

	/** Called by ARaceEnvironment once the weather is decided. */
	void SetWaterTint(const FLinearColor& Tint);

	/** The colour the water reflects towards the horizon. */
	void SetSkyTint(const FLinearColor& Sky);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Water")
	TObjectPtr<UProceduralMeshComponent> NearWater;

	/** Flat water from the edge of the grid out to the horizon. */
	UPROPERTY(VisibleAnywhere, Category = "Water")
	TObjectPtr<UStaticMeshComponent> FarWater;

private:
	void BuildGrid();
	void RefreshGrid(float Seconds);
	FVector2D GetViewCentreSnapped() const;

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> Triangles;
	TArray<FProcMeshTangent> Tangents;
	TArray<FLinearColor> VertexColors;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> NearMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FarMaterial;

	/** Grid centre the shore foam was last worked out for. */
	FVector2D FoamCentre = FVector2D(1.e12, 1.e12);

	FVector2D GridCentre = FVector2D::ZeroVector;
	float TimeSinceRefresh = 0.f;
	bool bGridBuilt = false;
};
