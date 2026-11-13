#include "BayTerrain.h"

#include "BoatArt.h"
#include "BoatRace.h"
#include "BoatTrack.h"
#include "BoatTrackData.h"

#include "Async/ParallelFor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const int32 BayLinePoints = UE_ARRAY_COUNT(BoatTrackData::RacingLine);
	/** Middle of the circuit, metres: the land is laid out around it. */
	const double BayCentreX = 30.0;
	const double BayCentreY = -75.0;
	/** The detailed, solid part of the land; beyond it, distant hills. */
	const double BayInnerHalf = 840.0;
	const double BayInnerCell = 10.0;
	const double BayOuterHalf = 2580.0;
	const double BayOuterCell = 60.0;

	double Hash2(int64 X, int64 Y, uint32 Seed)
	{
		uint64 N = static_cast<uint64>(X) * 0x9E3779B97F4A7C15ull ^ static_cast<uint64>(Y) * 0xC2B2AE3D27D4EB4Full ^ Seed * 0x165667B19E3779F9ull;
		N ^= N >> 29;
		N *= 0xBF58476D1CE4E5B9ull;
		N ^= N >> 32;
		return static_cast<double>(N & 0xFFFFFF) / static_cast<double>(0xFFFFFF);
	}

	/** Smooth value noise, -1..1. Deterministic, so every machine agrees. */
	double Noise(double X, double Y, uint32 Seed)
	{
		const double FX = FMath::Floor(X), FY = FMath::Floor(Y);
		const int64 IX = static_cast<int64>(FX), IY = static_cast<int64>(FY);
		double TX = X - FX, TY = Y - FY;
		TX = TX * TX * (3.0 - 2.0 * TX);
		TY = TY * TY * (3.0 - 2.0 * TY);
		const double A = Hash2(IX, IY, Seed), B = Hash2(IX + 1, IY, Seed);
		const double C = Hash2(IX, IY + 1, Seed), D = Hash2(IX + 1, IY + 1, Seed);
		return (FMath::Lerp(FMath::Lerp(A, B, TX), FMath::Lerp(C, D, TX), TY)) * 2.0 - 1.0;
	}

	double SmoothStep(double Edge0, double Edge1, double X)
	{
		const double T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	/** Clearance and the cliffiness of whichever stretch of shore is nearest. */
	double Clearance(double X, double Y, double& OutCliff)
	{
		double Best = -1.e18;
		OutCliff = 0.0;
		for (int32 Index = 0; Index < BayLinePoints; ++Index)
		{
			const int32 Next = (Index + 1) % BayLinePoints;
			const double AX = BoatTrackData::RacingLine[Index][0], AY = BoatTrackData::RacingLine[Index][1];
			const double BX = BoatTrackData::RacingLine[Next][0], BY = BoatTrackData::RacingLine[Next][1];
			const double DX = BX - AX, DY = BY - AY;
			const double T = FMath::Clamp(((X - AX) * DX + (Y - AY) * DY) / (DX * DX + DY * DY), 0.0, 1.0);
			const double QX = AX + DX * T, QY = AY + DY * T;
			const double Distance = FMath::Sqrt((X - QX) * (X - QX) + (Y - QY) * (Y - QY));
			// Counter-clockwise circuit: the left of the direction of travel is the inside.
			const bool bInside = (DX * (Y - AY) - DY * (X - AX)) > 0.0;
			const int32 Side = bInside ? 0 : 1;
			const double Reach = BoatTrackData::Shore[Index][Side] * (1.0 - T) + BoatTrackData::Shore[Next][Side] * T;
			const double Value = Reach - Distance;
			if (Value > Best)
			{
				Best = Value;
				OutCliff = BoatTrackData::Shore[Index][2 + Side] * (1.0 - T) + BoatTrackData::Shore[Next][2 + Side] * T;
			}
		}

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Barges); ++Index)
		{
			const double* Barge = BoatTrackData::Barges[Index];
			double DX = Barge[2] - Barge[0], DY = Barge[3] - Barge[1];
			const double Length = FMath::Sqrt(DX * DX + DY * DY);
			DX /= Length;
			DY /= Length;
			const double AX = Barge[0] - DX * BoatTrackData::BargeOverrunMetres, AY = Barge[1] - DY * BoatTrackData::BargeOverrunMetres;
			const double Span = Length + 2.0 * BoatTrackData::BargeOverrunMetres;
			const double T = FMath::Clamp((X - AX) * DX + (Y - AY) * DY, 0.0, Span);
			const double Distance = FMath::Sqrt(FMath::Square(X - (AX + DX * T)) + FMath::Square(Y - (AY + DY * T)));
			const double Value = BoatTrackData::BargeChannelMetres - Distance;
			if (Value > Best)
			{
				Best = Value;
				OutCliff = 0.7; // dredged channels have steep banks
			}
		}
		return Best;
	}

	double Height(double X, double Y)
	{
		double Cliff = 0.0;
		const double C = Clearance(X, Y, Cliff);
		if (C >= 0.0)
		{
			// The bed drops away quickly from the shore, so the waves do not
			// uncover a wide band of patchy shallows.
			return -FMath::Min(0.8 + C * 0.7, 16.0);
		}

		const double Up = -C;
		const double N1 = Noise(X / 180.0, Y / 180.0, 11);
		const double N2 = Noise(X / 55.0, Y / 55.0, 23);
		const double N3 = Noise(X / 420.0, Y / 420.0, 37);

		const double Beach = 3.0 * SmoothStep(0.0, 30.0, Up);
		const double CliffTop = (18.0 + 16.0 * (0.5 + 0.5 * N2)) * SmoothStep(0.0, 9.0, Up);
		const double Shore = FMath::Lerp(Beach, CliffTop, Cliff);
		const double Hills = (14.0 + 40.0 * (0.5 + 0.5 * N1)) * SmoothStep(20.0, 260.0, Up)
			+ 9.0 * N2 * SmoothStep(10.0, 120.0, Up);

		// A ring of mountains closes the bay in, so the horizon is hills.
		const double R = FMath::Sqrt(FMath::Square((X - BayCentreX) / 1.15) + FMath::Square(Y - BayCentreY));
		const double Mountains = 240.0 * SmoothStep(950.0, 1900.0, R) * (0.55 + 0.45 * (0.5 + 0.5 * N3))
			+ 60.0 * SmoothStep(1500.0, 2300.0, R) * N1;

		return Shore + Hills + Mountains - 0.8;
	}

	FLinearColor SRGB(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	/** Ground colour: sand, grass, straw, rock and snow, by height and steepness. */
	FLinearColor GroundColour(double X, double Y, double H, double Slope, float Snow)
	{
		if (H < -0.2)
		{
			const double Depth = FMath::Clamp(-H / 8.0, 0.0, 1.0);
			return FMath::Lerp(SRGB(214, 196, 150), SRGB(96, 118, 104), static_cast<float>(Depth));
		}

		const float Patch = static_cast<float>(0.5 + 0.5 * Noise(X / 40.0, Y / 40.0, 51));
		const float Dry = static_cast<float>(SmoothStep(0.35, 0.8, 0.5 + 0.5 * Noise(X / 110.0, Y / 110.0, 53)));
		FLinearColor Grass = FMath::Lerp(SRGB(92, 168, 56), SRGB(128, 190, 70), Patch);
		Grass = FMath::Lerp(Grass, SRGB(214, 190, 112), Dry * 0.65f); // straw, like the video's long grass
		const FLinearColor Sand = FMath::Lerp(SRGB(238, 218, 160), SRGB(226, 200, 140), Patch);
		const FLinearColor Rock = FMath::Lerp(SRGB(122, 114, 104), SRGB(150, 140, 126), Patch);
		const FLinearColor White = SRGB(236, 240, 248);

		FLinearColor Colour = Grass;
		if (H < 2.4 && Slope < 0.45)
		{
			Colour = FMath::Lerp(Sand, Grass, static_cast<float>(SmoothStep(1.4, 2.4, H)));
		}
		const float Steep = static_cast<float>(SmoothStep(0.55, 0.95, Slope));
		Colour = FMath::Lerp(Colour, Rock, Steep);
		if (H > 120.0)
		{
			Colour = FMath::Lerp(Colour, Rock, static_cast<float>(SmoothStep(120.0, 160.0, H)));
		}
		if (H > 175.0)
		{
			Colour = FMath::Lerp(Colour, White, static_cast<float>(SmoothStep(175.0, 205.0, H)) * (1.f - Steep * 0.5f));
		}
		if (Snow > 0.f && H > 0.6)
		{
			Colour = FMath::Lerp(Colour, White, Snow * (1.f - Steep * 0.7f));
		}
		return Colour;
	}
}

float BayTerrain::GetClearanceMetres(double XMetres, double YMetres)
{
	double Cliff = 0.0;
	return static_cast<float>(Clearance(XMetres, YMetres, Cliff));
}

float BayTerrain::GetHeightMetres(double XMetres, double YMetres)
{
	return static_cast<float>(Height(XMetres, YMetres));
}

float BayTerrain::GetHeight(const FVector2D& WorldUU)
{
	return static_cast<float>(Height(WorldUU.X / 100.0, WorldUU.Y / 100.0) * 100.0);
}

FVector2D BayTerrain::GetSeaward(const FVector2D& WorldUU)
{
	const double X = WorldUU.X / 100.0, Y = WorldUU.Y / 100.0;
	double Cliff = 0.0;
	const double Step = 2.0;
	const FVector2D Gradient(
		Clearance(X + Step, Y, Cliff) - Clearance(X - Step, Y, Cliff),
		Clearance(X, Y + Step, Cliff) - Clearance(X, Y - Step, Cliff));
	return Gradient.GetSafeNormal();
}

// ===========================================================================
// The actor
// ===========================================================================
ABayTerrain::ABayTerrain()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // every machine builds the same land itself

	Land = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Land"));
	SetRootComponent(Land);
	Land->bUseAsyncCooking = false;
	Land->bUseComplexAsSimpleCollision = true;
	Land->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Land->SetCollisionObjectType(ECC_WorldStatic);
	Land->SetCollisionResponseToAllChannels(ECR_Block);
	Land->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Land->SetCastShadow(true);
}

void ABayTerrain::BeginPlay()
{
	Super::BeginPlay();

	const double StartSeconds = FPlatformTime::Seconds();

	// The racing area in detail, solid; the distant hills coarse and visual only.
	FGrid Inner;
	Inner.Cells = static_cast<int32>(2.0 * BayInnerHalf / BayInnerCell);
	Inner.Origin[0] = BayCentreX - BayInnerHalf;
	Inner.Origin[1] = BayCentreY - BayInnerHalf;
	Inner.CellMetres = BayInnerCell;
	BuildSection(0, Inner, true);

	FGrid Outer;
	Outer.Cells = static_cast<int32>(2.0 * BayOuterHalf / BayOuterCell);
	Outer.Origin[0] = BayCentreX - BayOuterHalf;
	Outer.Origin[1] = BayCentreY - BayOuterHalf;
	Outer.CellMetres = BayOuterCell;
	Outer.HoleHalf = BayInnerHalf;
	BuildSection(1, Outer, false);

	UMaterialInterface* LandMaterial = BoatArt::Material(TEXT("M_VertexColor"));
	if (LandMaterial == nullptr)
	{
		LandMaterial = LoadObject<UMaterialInterface>(nullptr,
			TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (LandMaterial != nullptr)
	{
		Land->SetMaterial(0, LandMaterial);
		Land->SetMaterial(1, LandMaterial);
	}

	ScatterPlants();
	PlaceBuildings();

	UE_LOG(LogBoatRace, Log, TEXT("Coast built in %.2f s with %d painted parts."),
		FPlatformTime::Seconds() - StartSeconds, PaintedParts.Num());
}

void ABayTerrain::BuildSection(int32 Section, const FGrid& Grid, bool bCollision)
{
	const int32 Side = Grid.Cells + 1;
	TArray<FVector> Vertices;
	TArray<FVector2D> Points;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.SetNumUninitialized(Side * Side);
	Points.SetNumUninitialized(Side * Side);
	Normals.SetNumUninitialized(Side * Side);
	UVs.SetNumUninitialized(Side * Side);

	const double Step = Grid.CellMetres;
	ParallelFor(Side, [&](int32 Row)
	{
		for (int32 Column = 0; Column < Side; ++Column)
		{
			const int32 Index = Row * Side + Column;
			const double X = Grid.Origin[0] + Column * Step;
			const double Y = Grid.Origin[1] + Row * Step;
			const double H = Height(X, Y);
			const double E = Height(X + Step * 0.5, Y), W = Height(X - Step * 0.5, Y);
			const double N = Height(X, Y + Step * 0.5), S = Height(X, Y - Step * 0.5);
			Points[Index] = FVector2D(X, Y);
			Vertices[Index] = FVector(X * 100.0, Y * 100.0, H * 100.0);
			Normals[Index] = FVector(-(E - W) / Step, -(N - S) / Step, 1.0).GetSafeNormal();
			UVs[Index] = FVector2D(X / 20.0, Y / 20.0);
		}
	});

	TArray<int32> Triangles;
	Triangles.Reserve(Grid.Cells * Grid.Cells * 6);
	for (int32 Row = 0; Row < Grid.Cells; ++Row)
	{
		for (int32 Column = 0; Column < Grid.Cells; ++Column)
		{
			if (Grid.HoleHalf > 0.0)
			{
				const double X0 = Grid.Origin[0] + Column * Step - BayCentreX, X1 = X0 + Step;
				const double Y0 = Grid.Origin[1] + Row * Step - BayCentreY, Y1 = Y0 + Step;
				if (X0 >= -Grid.HoleHalf && X1 <= Grid.HoleHalf && Y0 >= -Grid.HoleHalf && Y1 <= Grid.HoleHalf)
				{
					continue; // the detailed section covers this
				}
			}
			const int32 Corner = Row * Side + Column;
			// Wound so the faces point up.
			Triangles.Append({ Corner, Corner + Side, Corner + 1, Corner + 1, Corner + Side, Corner + Side + 1 });
		}
	}

	SectionPoints.SetNum(FMath::Max(SectionPoints.Num(), Section + 1));
	SectionVertices.SetNum(FMath::Max(SectionVertices.Num(), Section + 1));
	SectionTriangles.SetNum(FMath::Max(SectionTriangles.Num(), Section + 1));
	SectionNormals.SetNum(FMath::Max(SectionNormals.Num(), Section + 1));
	SectionPoints[Section] = MoveTemp(Points);
	SectionVertices[Section] = MoveTemp(Vertices);
	SectionTriangles[Section] = MoveTemp(Triangles);
	SectionNormals[Section] = MoveTemp(Normals);

	TArray<FLinearColor> Colours;
	Colours.SetNumUninitialized(SectionPoints[Section].Num());
	for (int32 Index = 0; Index < Colours.Num(); ++Index)
	{
		const FVector2D& P = SectionPoints[Section][Index];
		const double Slope = FMath::Sqrt(1.0 / FMath::Max(FMath::Square(SectionNormals[Section][Index].Z), 1.e-4) - 1.0);
		Colours[Index] = GroundColour(P.X, P.Y, SectionVertices[Section][Index].Z / 100.0, Slope, Snow);
	}

	Land->CreateMeshSection_LinearColor(Section, SectionVertices[Section], SectionTriangles[Section],
		SectionNormals[Section], UVs, Colours, TArray<FProcMeshTangent>(), bCollision);
}

void ABayTerrain::RecolourSection(int32 Section)
{
	if (!SectionPoints.IsValidIndex(Section))
	{
		return;
	}
	TArray<FLinearColor> Colours;
	Colours.SetNumUninitialized(SectionPoints[Section].Num());
	for (int32 Index = 0; Index < Colours.Num(); ++Index)
	{
		const FVector2D& P = SectionPoints[Section][Index];
		const double Slope = FMath::Sqrt(1.0 / FMath::Max(FMath::Square(SectionNormals[Section][Index].Z), 1.e-4) - 1.0);
		Colours[Index] = GroundColour(P.X, P.Y, SectionVertices[Section][Index].Z / 100.0, Slope, Snow);
	}
	Land->UpdateMeshSection_LinearColor(Section, SectionVertices[Section], SectionNormals[Section],
		TArray<FVector2D>(), Colours, TArray<FProcMeshTangent>());
}

UInstancedStaticMeshComponent* ABayTerrain::MakeInstances(UStaticMesh* Mesh, float HeightMetres, bool bShadows)
{
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
	Instances->SetStaticMesh(Mesh);
	Instances->SetupAttachment(Land);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(bShadows);
	Instances->SetCanEverAffectNavigation(false);
	// Trees beyond about a kilometre are lost in the haze anyway.
	Instances->SetCullDistances(0, 140000);
	Instances->RegisterComponent();
	PaintedParts.Add(Instances);
	return Instances;
}

void ABayTerrain::ScatterPlants()
{
	struct FKind
	{
		const TCHAR* Name;
		float Height;
		UInstancedStaticMeshComponent* Instances = nullptr;
		float Scale = 1.f;
	};
	FKind Kinds[] =
	{
		{ TEXT("TreeRound"), 9.1f }, { TEXT("TreeTall"), 10.8f }, { TEXT("Pine"), 8.1f },
		{ TEXT("Palm"), 8.3f }, { TEXT("Bush"), 1.7f }, { TEXT("RockA"), 1.6f },
		{ TEXT("RockB"), 2.4f }, { TEXT("RockC"), 1.3f },
	};
	for (FKind& Kind : Kinds)
	{
		UStaticMesh* Mesh = BoatArt::Model(Kind.Name);
		Kind.Instances = MakeInstances(Mesh, Kind.Height, Kind.Height > 3.f);
		Kind.Scale = BoatArt::UnitScale(Mesh, Kind.Height);
	}
	enum { Round, Tall, Pine, Palm, Bush, RockA, RockB, RockC };

	// One candidate every 14 m inside the hills, jittered so it never looks planted.
	const double Spacing = 14.0;
	const double Reach = 1500.0;
	int32 Placed = 0;
	for (double GY = BayCentreY - Reach; GY <= BayCentreY + Reach; GY += Spacing)
	{
		for (double GX = BayCentreX - Reach * 1.15; GX <= BayCentreX + Reach * 1.15; GX += Spacing)
		{
			const int64 CX = static_cast<int64>(GX), CY = static_cast<int64>(GY);
			const double X = GX + (Hash2(CX, CY, 101) - 0.5) * Spacing;
			const double Y = GY + (Hash2(CX, CY, 103) - 0.5) * Spacing;
			const double Roll = Hash2(CX, CY, 107);
			double Cliff = 0.0;
			const double Up = -Clearance(X, Y, Cliff);
			if (Up < 4.0)
			{
				continue; // water, or right at the water's edge
			}
			const double H = Height(X, Y);
			const double Slope = FMath::Abs(Height(X + 3, Y) - Height(X - 3, Y)) / 6.0 + FMath::Abs(Height(X, Y + 3) - Height(X, Y - 3)) / 6.0;
			const double Forest = 0.5 + 0.5 * Noise(X / 150.0, Y / 150.0, 61);
			// Thin out with distance: the far hills only need a texture of trees.
			const double R = FMath::Sqrt(FMath::Square(X - BayCentreX) + FMath::Square(Y - BayCentreY));
			const double Density = FMath::Lerp(1.0, 0.35, SmoothStep(700.0, 1500.0, R));

			int32 Kind = -1;
			if (Slope > 0.7)
			{
				Kind = (Roll < 0.18 * Density) ? RockA + (CX + CY) % 3 : -1;
			}
			else if (H < 6.0 && Cliff < 0.4 && Up < 60.0)
			{
				Kind = (Roll < 0.16) ? (Roll < 0.1 ? Palm : Bush) : -1;
			}
			else if (H < 160.0 && Roll < (0.12 + 0.55 * Forest) * Density)
			{
				const double Pick = Hash2(CX, CY, 109);
				if (H > 70.0 || (H > 30.0 && Pick < 0.5))
				{
					Kind = Pine;
				}
				else
				{
					Kind = Pick < 0.42 ? Round : (Pick < 0.75 ? Tall : (Pick < 0.9 ? Bush : RockA + (CX % 3 + 3) % 3));
				}
			}
			if (Kind < 0 || Kinds[Kind].Instances == nullptr)
			{
				continue;
			}

			const float Size = Kinds[Kind].Scale * (0.75f + 0.6f * static_cast<float>(Hash2(CX, CY, 113)));
			const float Yaw = static_cast<float>(Hash2(CX, CY, 127) * 360.0);
			// Sunk a little so nothing floats on a slope.
			const FTransform Transform(FRotator(0.f, Yaw, 0.f), FVector(X * 100.0, Y * 100.0, (H - 0.4) * 100.0), FVector(Size));
			Kinds[Kind].Instances->AddInstance(Transform, true);
			++Placed;
		}
	}
	RepaintAll();
	UE_LOG(LogBoatRace, Log, TEXT("Scattered %d trees, bushes and rocks."), Placed);
}

UStaticMeshComponent* ABayTerrain::PlaceModel(const TCHAR* Name, float HeightMetres, const FVector& LocationUU, float Yaw,
	float SizeMultiplier, bool bCollision)
{
	UStaticMesh* Mesh = BoatArt::Model(Name);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
	Part->SetStaticMesh(Mesh);
	Part->SetupAttachment(Land);
	Part->SetWorldLocationAndRotation(LocationUU, FRotator(0.f, Yaw, 0.f));
	Part->SetWorldScale3D(FVector(BoatArt::UnitScale(Mesh, HeightMetres) * SizeMultiplier));
	if (bCollision)
	{
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetCollisionResponseToAllChannels(ECR_Block);
		Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Part->SetCanEverAffectNavigation(false);
	Part->RegisterComponent();
	PaintedParts.Add(Part);
	return Part;
}

void ABayTerrain::PlaceBuildings()
{
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	auto YawOf = [](const FVector2D& Direction)
	{
		return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X)));
	};
	auto Ground = [](const FVector2D& PointMetres)
	{
		return FVector(PointMetres.X * 100.0, PointMetres.Y * 100.0, Height(PointMetres.X, PointMetres.Y) * 100.0);
	};
	/** Walks out from a gate along its outward line until the land is at least MinHeight. */
	auto FindBank = [](const FBoatGate& Gate, double Direction, double MinHeight, double StartMetres)
	{
		const FVector2D Centre = Gate.Center / 100.0;
		const FVector2D Out = FVector2D(Gate.Forward.Y, -Gate.Forward.X) * Direction; // +1 outside, -1 inside
		for (double Distance = StartMetres; Distance < StartMetres + 260.0; Distance += 2.0)
		{
			const FVector2D P = Centre + Out * Distance;
			if (Height(P.X, P.Y) >= MinHeight)
			{
				return P;
			}
		}
		return Centre + Out * (StartMetres + 260.0);
	};

	// --- Start / finish: grandstand, pier, tents and flags on the outside bank.
	const FBoatGate& Start = Gates[0];
	const FVector2D Outward(Start.Forward.Y, -Start.Forward.X);
	const FVector2D StandSpot = FindBank(Start, 1.0, 1.2, 60.0) + Outward * 14.0;
	if (UStaticMeshComponent* Stand = PlaceModel(TEXT("Grandstand"), 12.3f, Ground(StandSpot), YawOf(-Outward)))
	{
		AddCrowd(Stand);
	}
	for (int32 Index = -3; Index <= 3; ++Index)
	{
		const FVector2D Spot = StandSpot + Outward * 18.0 + Start.Forward * (Index * 9.0);
		PlaceModel(Index % 2 ? TEXT("Flag") : TEXT("Tent"), Index % 2 ? 9.f : 6.1f, Ground(Spot), YawOf(-Outward) + 90.f);
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector2D Spot = StandSpot - Outward * (16.0 + Index * 30.0) + Start.Forward * 40.0;
		PlaceModel(TEXT("Pier"), 5.6f, FVector(Spot.X * 100.0, Spot.Y * 100.0, -40.0), YawOf(Outward));
		if (Index < 2)
		{
			const FVector2D Moor = Spot + Start.Forward * 8.0;
			PlaceModel(TEXT("Yacht"), 6.6f, FVector(Moor.X * 100.0, Moor.Y * 100.0, 20.0), YawOf(Outward), 1.f, true);
		}
	}

	// --- Cottages along the gentler banks, facing the water like the video's.
	int32 Houses = 0;
	for (int32 Index = 0; Index < Gates.Num() && Houses < 7; ++Index)
	{
		for (double Direction : { 1.0, -1.0 })
		{
			const int32 Side = Direction > 0 ? 1 : 0;
			if (BoatTrackData::Shore[Index][2 + Side] > 0.35 || (Index + Side) % 3 != 0)
			{
				continue;
			}
			const FVector2D Spot = FindBank(Gates[Index], Direction, 2.0, 50.0)
				+ FVector2D(Gates[Index].Forward.Y, -Gates[Index].Forward.X) * Direction * 22.0;
			const double H = Height(Spot.X, Spot.Y);
			if (H < 1.0 || H > 30.0 || (Index == 0 && Direction > 0))
			{
				continue;
			}
			const FVector2D Facing = FVector2D(-Gates[Index].Forward.Y, Gates[Index].Forward.X) * Direction;
			PlaceModel(TEXT("Cottage"), 7.5f, Ground(Spot) - FVector(0, 0, 50), YawOf(Facing) + 90.f);
			++Houses;
		}
	}

	// --- A lighthouse on the cliffs of the far corner.
	const FVector2D LightSpot = FindBank(Gates[4], 1.0, 10.0, 70.0) + FVector2D(Gates[4].Forward.Y, -Gates[4].Forward.X) * 6.0;
	PlaceModel(TEXT("Lighthouse"), 24.f, Ground(LightSpot) - FVector(0, 0, 150), 0.f, 1.2f);

	// --- The bridge over the narrows, square across the course.
	const FVector2D NarrowA(BoatTrackData::RacingLine[8][0], BoatTrackData::RacingLine[8][1]);
	const FVector2D NarrowB(BoatTrackData::RacingLine[9][0], BoatTrackData::RacingLine[9][1]);
	const FVector2D Mid = (NarrowA + NarrowB) * 0.5;
	PlaceModel(TEXT("Bridge"), 64.9f, FVector(Mid.X * 100.0, Mid.Y * 100.0, -600.0), YawOf(NarrowB - NarrowA), 1.f);

	// --- A giant inflatable duck in the lagoon inside the start, as in the video.
	const FVector2D DuckSpot = Start.Center / 100.0 - Outward * 78.0 + Start.Forward * 30.0;
	PlaceModel(TEXT("Duck"), 2.3f, FVector(DuckSpot.X * 100.0, DuckSpot.Y * 100.0, -40.0), YawOf(Start.Forward) + 140.f, 3.4f, true);

	// --- Floating sponsor banners just outside the course on the long straights.
	for (int32 Index : { 1, 4, 7, 10, 13 })
	{
		const FBoatGate& Gate = Gates[Index];
		const FVector2D Spot = Gate.Center / 100.0 + FVector2D(Gate.Forward.Y, -Gate.Forward.X) * 62.0 + Gate.Forward * 40.0;
		if (BayTerrain::GetClearanceMetres(Spot.X, Spot.Y) > 10.0f)
		{
			PlaceModel(TEXT("Banner"), 6.3f, FVector(Spot.X * 100.0, Spot.Y * 100.0, 30.0), YawOf(Gate.Forward), 1.f, true);
		}
	}
	RepaintAll();
}

void ABayTerrain::AddCrowd(UStaticMeshComponent* Stand)
{
	// Spectators in the grandstand: three builds of person, six shirt
	// colours, about three seats in four taken. One instanced component per
	// combination keeps it to eighteen draw calls for a couple of hundred people.
	static const TCHAR* const Builds[] = { TEXT("SpectatorA"), TEXT("SpectatorB"), TEXT("SpectatorC") };
	const FLinearColor Shirts[] =
	{
		FLinearColor::FromSRGBColor(FColor(229, 57, 53)), FLinearColor::FromSRGBColor(FColor(30, 136, 229)),
		FLinearColor::FromSRGBColor(FColor(253, 216, 53)), FLinearColor::FromSRGBColor(FColor(67, 160, 71)),
		FLinearColor::FromSRGBColor(FColor(245, 245, 245)), FLinearColor::FromSRGBColor(FColor(255, 112, 67)),
	};
	TArray<UInstancedStaticMeshComponent*> Groups;
	Groups.SetNumZeroed(UE_ARRAY_COUNT(Builds) * UE_ARRAY_COUNT(Shirts));
	const FTransform StandTransform = Stand->GetComponentTransform();
	int32 Seated = 0;
	for (int32 Tier = 0; Tier < 7; ++Tier)
	{
		for (int32 Seat = 0; Seat < 16; ++Seat)
		{
			for (const int32 Side : { -1, 1 })
			{
				const int64 Key = Tier * 100 + Seat * 2 + (Side > 0 ? 1 : 0);
				if (Hash2(Key, 7, 211) > 0.74)
				{
					continue;
				}
				const int32 Build = static_cast<int32>(Hash2(Key, 9, 223) * 2.999);
				const int32 Shirt = static_cast<int32>(Hash2(Key, 11, 227) * 5.999);
				const int32 Group = Build * UE_ARRAY_COUNT(Shirts) + Shirt;
				if (Groups[Group] == nullptr)
				{
					UStaticMesh* Mesh = BoatArt::Model(Builds[Build]);
					if (Mesh == nullptr)
					{
						return;
					}
					UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
					Instances->SetStaticMesh(Mesh);
					Instances->SetupAttachment(Land);
					Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Instances->SetCastShadow(true);
					Instances->SetCullDistances(0, 60000);
					Instances->RegisterComponent();
					BoatArt::FLook Look;
					Look.Paint = Shirts[Shirt];
					BoatArt::Paint(Instances, Look);
					Groups[Group] = Instances;
				}
				// Seat positions in the grandstand model, in its own metres.
				const FVector Local((-1.1 * Tier + 0.1) * 100.0, (-18.75 + Seat * 2.5 + Side * 0.55) * 100.0, (1.245 + 0.75 * Tier) * 100.0);
				const FVector World = StandTransform.TransformPosition(Local);
				const float Size = BoatArt::UnitScale(Groups[Group]->GetStaticMesh(), 1.4f) * (0.92f + 0.14f * static_cast<float>(Hash2(Key, 13, 229)));
				const FRotator Facing(0.f, static_cast<float>(StandTransform.Rotator().Yaw) + static_cast<float>(Hash2(Key, 17, 233) - 0.5) * 30.f, 0.f);
				Groups[Group]->AddInstance(FTransform(Facing, World, FVector(Size)), true);
				++Seated;
			}
		}
	}
	UE_LOG(LogBoatRace, Log, TEXT("%d spectators in the grandstand."), Seated);
}

void ABayTerrain::RepaintAll()
{
	BoatArt::FLook Look;
	Look.Snow = Snow;
	Look.bNight = bNight;
	for (UMeshComponent* Part : PaintedParts)
	{
		BoatArt::Paint(Part, Look);
	}
}

void ABayTerrain::SetSnow(float Amount)
{
	Amount = FMath::Clamp(Amount, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Amount, Snow))
	{
		return;
	}
	Snow = Amount;
	RecolourSection(0);
	RecolourSection(1);
	RepaintAll();
}

void ABayTerrain::SetNight(bool bInNight)
{
	if (bNight == bInNight)
	{
		return;
	}
	bNight = bInNight;
	RepaintAll();
}
