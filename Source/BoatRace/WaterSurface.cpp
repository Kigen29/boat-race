#include "WaterSurface.h"

#include "BoatRaceGameState.h"
#include "BayTerrain.h"
#include "OceanWaves.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AWaterSurface::AWaterSurface()
{
	PrimaryActorTick.bCanEverTick = true;
	// Tick after the boats so the surface matches where they ended up.
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;

	NearWater = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("NearWater"));
	SetRootComponent(NearWater);
	NearWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NearWater->SetCastShadow(false);
	NearWater->bUseAsyncCooking = false;
	NearWater->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	FarWater = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FarWater"));
	FarWater->SetupAttachment(NearWater);
	FarWater->SetStaticMesh(PlaneFinder.Object);
	// The engine plane is 1 m across, so this is 12 km of open water.
	FarWater->SetRelativeScale3D(FVector(12000.f, 12000.f, 1.f));
	FarWater->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	FarWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FarWater->SetCastShadow(false);
}

void AWaterSurface::BeginPlay()
{
	Super::BeginPlay();

	// The Higgsfield-derived water material if import_assets.sh built it,
	// otherwise the engine default, which still takes a colour.
	UMaterialInterface* WaterMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Higgsfield/M_Water.M_Water"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (WaterMaterial != nullptr)
	{
		NearWater->SetMaterial(0, WaterMaterial);
		FarWater->SetMaterial(0, WaterMaterial);
	}
	// Dynamic instances either way, so the weather can tint the sea.
	NearMaterial = NearWater->CreateAndSetMaterialInstanceDynamic(0);
	FarMaterial = FarWater->CreateAndSetMaterialInstanceDynamic(0);
	SetWaterTint(FLinearColor(0.006f, 0.02f, 0.025f));
	// The horizon plane has no vertex colours (they read as white): no foam.
	if (FarMaterial != nullptr)
	{
		FarMaterial->SetScalarParameterValue(TEXT("FoamFromVertex"), 0.f);
	}

	GridCells = FMath::Clamp(GridCells, 8, 200);
	CellSize = FMath::Clamp(CellSize, 100.f, 5000.f);

	BuildGrid();
}

void AWaterSurface::SetWaterTint(const FLinearColor& Tint)
{
	// "Color" is the parameter the engine's basic material and the generated
	// M_Water both expose; on any other material this is simply ignored.
	if (NearMaterial != nullptr)
	{
		NearMaterial->SetVectorParameterValue(TEXT("Color"), Tint);
	}
	if (FarMaterial != nullptr)
	{
		// A shade darker out towards the horizon so the join is not a line.
		FarMaterial->SetVectorParameterValue(TEXT("Color"), Tint * 0.85f);
	}
}

void AWaterSurface::BuildGrid()
{
	const int32 VertsPerSide = GridCells + 1;
	const int32 VertexCount = VertsPerSide * VertsPerSide;

	Vertices.SetNum(VertexCount);
	Normals.SetNum(VertexCount);
	UVs.SetNum(VertexCount);
	Tangents.SetNum(VertexCount);
	// Red marks foam for M_Water: white water breaking along the shore.
	VertexColors.Init(FLinearColor::Black, VertexCount);

	Triangles.Reset(GridCells * GridCells * 6);
	for (int32 Y = 0; Y < GridCells; ++Y)
	{
		for (int32 X = 0; X < GridCells; ++X)
		{
			const int32 Corner = Y * VertsPerSide + X;
			// Counter-clockwise when seen from above, so the surface faces up.
			Triangles.Add(Corner);
			Triangles.Add(Corner + VertsPerSide);
			Triangles.Add(Corner + 1);

			Triangles.Add(Corner + 1);
			Triangles.Add(Corner + VertsPerSide);
			Triangles.Add(Corner + VertsPerSide + 1);
		}
	}

	GridCentre = GetViewCentreSnapped();
	RefreshGrid(0.f);

	NearWater->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, false);
	bGridBuilt = true;
}

FVector2D AWaterSurface::GetViewCentreSnapped() const
{
	FVector2D Centre = FVector2D::ZeroVector;

	const UWorld* World = GetWorld();
	if (const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Centre = FVector2D(ViewLocation.X, ViewLocation.Y);
	}

	// Snap to whole cells: the grid then slides a cell at a time instead of
	// dragging the wave pattern along with the camera.
	Centre.X = FMath::GridSnap(Centre.X, static_cast<double>(CellSize));
	Centre.Y = FMath::GridSnap(Centre.Y, static_cast<double>(CellSize));
	return Centre;
}

void AWaterSurface::RefreshGrid(float Seconds)
{
	const int32 VertsPerSide = GridCells + 1;
	const double Half = 0.5 * GridCells * CellSize;
	// The actor sits at the world origin, so mesh-local X/Y equal world X/Y
	// minus the grid centre.
	const double OriginX = GridCentre.X - Half;
	const double OriginY = GridCentre.Y - Half;

	for (int32 Y = 0; Y < VertsPerSide; ++Y)
	{
		for (int32 X = 0; X < VertsPerSide; ++X)
		{
			const int32 Index = Y * VertsPerSide + X;
			const double WorldX = OriginX + X * CellSize;
			const double WorldY = OriginY + Y * CellSize;

			FVector2D Slope;
			const float Height = OceanWaves::GetHeightAndSlope(FVector2D(WorldX, WorldY), Seconds, Slope);

			Vertices[Index] = FVector(WorldX - GetActorLocation().X, WorldY - GetActorLocation().Y, Height);
			Normals[Index] = FVector(-Slope.X, -Slope.Y, 1.0).GetSafeNormal();
			// Tangent along +X, tilted to follow the surface.
			Tangents[Index] = FProcMeshTangent(FVector(1.0, 0.0, Slope.X).GetSafeNormal(), false);
			UVs[Index] = FVector2D(WorldX / 1000.0, WorldY / 1000.0);
		}
	}

	// Shore foam only changes when the grid moves to a new cell.
	if (!GridCentre.Equals(FoamCentre))
	{
		FoamCentre = GridCentre;
		for (int32 Y = 0; Y < VertsPerSide; ++Y)
		{
			for (int32 X = 0; X < VertsPerSide; ++X)
			{
				const double WorldX = OriginX + X * CellSize;
				const double WorldY = OriginY + Y * CellSize;
				// Within about seven metres of the beach or the rocks the
				// water breaks white, fading out to clear water.
				const float Clearance = BayTerrain::GetClearanceMetres(WorldX / 100.0, WorldY / 100.0);
				const float Foam = FMath::Clamp(1.f - Clearance / 7.f, 0.f, 1.f);
				VertexColors[Y * VertsPerSide + X] = FLinearColor(Foam, 0.f, 0.f, 1.f);
			}
		}
	}
}

void AWaterSurface::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bGridBuilt)
	{
		return;
	}

	TimeSinceRefresh += DeltaSeconds;
	if (TimeSinceRefresh < UpdateInterval)
	{
		return;
	}
	TimeSinceRefresh = 0.f;

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	// Server time keeps the sea in step for everyone; without a game state yet,
	// local time is close enough for the menu.
	const float Seconds = RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);

	GridCentre = GetViewCentreSnapped();
	RefreshGrid(Seconds);
	NearWater->UpdateMeshSection_LinearColor(0, Vertices, Normals, UVs, VertexColors, Tangents);

	// Keep the horizon plane under the camera too, a little below the mean
	// surface so the grid edge is never silhouetted against it.
	FarWater->SetWorldLocation(FVector(GridCentre.X, GridCentre.Y, OceanWaves::MeanLevel - 45.f));
}
