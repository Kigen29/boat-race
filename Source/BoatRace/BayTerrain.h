#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BayTerrain.generated.h"

class UInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The land around the bay, as one function of position.
 *
 * Built from the shore table in BoatTrackData.h (Scripts/course_layout.py
 * checks that the whole course, every obstacle and the barge runs are water).
 * The visible hills, their collision, where trees and houses go and the force
 * that pushes a beached boat back out all read the same numbers, so they
 * cannot disagree, and every machine builds an identical coast with nothing
 * sent over the network.
 */
namespace BayTerrain
{
	/** Metres the point is inside the water (positive) or up the bank (negative). */
	float GetClearanceMetres(double XMetres, double YMetres);

	/** Ground height in metres; below 0 is the lake bed. */
	float GetHeightMetres(double XMetres, double YMetres);

	/** Ground height at a world position, in Unreal units. */
	float GetHeight(const FVector2D& WorldUU);

	/** Unit direction towards open water from a world position. */
	FVector2D GetSeaward(const FVector2D& WorldUU);
}

/** Builds the coast, its collision, and everything that grows or stands on it. */
UCLASS()
class ABayTerrain : public AActor
{
	GENERATED_BODY()

public:
	ABayTerrain();

	virtual void BeginPlay() override;

	/** Recolours the land and the plants. 0 summer .. 1 deep snow. */
	void SetSnow(float Amount);

	/** Lamps and windows on or off. */
	void SetNight(bool bNight);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Terrain")
	TObjectPtr<UProceduralMeshComponent> Land;

private:
	struct FGrid
	{
		int32 Cells = 0;
		double Origin[2] = { 0.0, 0.0 };
		double CellMetres = 0.0;
		/** Cells fully inside this square (in metres) are left out. */
		double HoleHalf = -1.0;
	};

	void BuildSection(int32 Section, const FGrid& Grid, bool bCollision);
	void RecolourSection(int32 Section);
	void ScatterPlants();
	void PlaceBuildings();

	UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, float HeightMetres, bool bShadows);
	UStaticMeshComponent* PlaceModel(const TCHAR* Name, float HeightMetres, const FVector& LocationUU, float Yaw,
		float SizeMultiplier = 1.f, bool bCollision = false);
	void RepaintAll();

	/** Per section: the vertices' XY in metres, kept for recolouring. */
	TArray<TArray<FVector2D>> SectionPoints;
	TArray<TArray<FVector>> SectionVertices;
	TArray<TArray<int32>> SectionTriangles;
	TArray<TArray<FVector>> SectionNormals;

	UPROPERTY()
	TArray<TObjectPtr<UMeshComponent>> PaintedParts;

	float Snow = 0.f;
	bool bNight = false;
};
