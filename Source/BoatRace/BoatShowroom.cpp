#include "BoatShowroom.h"

#include "BayTerrain.h"
#include "BoatArt.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"

#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	/** The hangar model is 30 m deep and 28 m wide (Scripts/make_models.py). */
	constexpr double HangarDepth = 30.0;
	constexpr double HangarHalfWidth = 14.0;
	constexpr float HangarHeightMetres = 15.7f;

	double DistanceToLine(const FVector2D& Point)
	{
		const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
		double Best = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Gates.Num(); ++Index)
		{
			const FVector2D A = Gates[Index].Center / 100.0;
			const FVector2D B = Gates[(Index + 1) % Gates.Num()].Center / 100.0;
			const FVector2D AB = B - A;
			const double T = FMath::Clamp(FVector2D::DotProduct(Point - A, AB) / FMath::Max(AB.SizeSquared(), 1.0), 0.0, 1.0);
			Best = FMath::Min(Best, FVector2D::Distance(Point, A + AB * T));
		}
		return Best;
	}
}

AGarageShowroom::AGarageShowroom()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Hangar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hangar"));
	Hangar->SetupAttachment(GetRootComponent());
	Hangar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Boat = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boat"));
	Boat->SetupAttachment(GetRootComponent());
	Boat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Boat->SetUsingAbsoluteLocation(true);
	Boat->SetUsingAbsoluteRotation(true);
}

AGarageShowroom* AGarageShowroom::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<AGarageShowroom> It(World);
	return It ? *It : nullptr;
}

bool AGarageShowroom::FindSpot(FVector2D& OutMouth, FVector2D& OutSeaward) const
{
	// Walk out from every gate to the bank on either side; the back wall goes
	// on the bank, the mouth faces the open water. Keep the one that is all
	// water underfoot, well off the course and nearest the start.
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	double BestScore = TNumericLimits<double>::Max();
	bool bFound = false;
	for (int32 Index = 0; Index < Gates.Num(); ++Index)
	{
		for (const double Side : { 1.0, -1.0 })
		{
			const FVector2D Centre = Gates[Index].Center / 100.0;
			const FVector2D Out = FVector2D(Gates[Index].Forward.Y, -Gates[Index].Forward.X) * Side;
			for (double Along = -60.0; Along <= 60.0; Along += 30.0)
			{
				const FVector2D Start = Centre + FVector2D(Gates[Index].Forward) * Along;
				FVector2D Bank = Start;
				bool bBank = false;
				for (double Distance = 50.0; Distance < 400.0; Distance += 3.0)
				{
					Bank = Start + Out * Distance;
					if (BayTerrain::GetClearanceMetres(Bank.X, Bank.Y) < 1.0)
					{
						bBank = true;
						break;
					}
				}
				if (!bBank)
				{
					continue;
				}
				const FVector2D Sea = BayTerrain::GetSeaward(Bank * 100.0);
				if (Sea.IsNearlyZero())
				{
					continue;
				}
				const FVector2D Across(-Sea.Y, Sea.X);
				const FVector2D Back = Bank + Sea * 2.0;
				const FVector2D Mouth = Back + Sea * HangarDepth;
				// Water under the whole floor, deep water off the mouth.
				bool bOk = BayTerrain::GetClearanceMetres(Mouth.X + Sea.X * 20.0, Mouth.Y + Sea.Y * 20.0) > 25.0;
				for (double X = 0.0; bOk && X <= HangarDepth; X += 5.0)
				{
					for (double Y = -HangarHalfWidth; bOk && Y <= HangarHalfWidth; Y += 7.0)
					{
						const FVector2D P = Back + Sea * X + Across * Y;
						bOk = BayTerrain::GetClearanceMetres(P.X, P.Y) > (X < 6.0 ? -1.0 : 2.0) && DistanceToLine(P) > 75.0;
					}
				}
				bOk = bOk && DistanceToLine(Mouth + Sea * 15.0) > 75.0;
				for (const FBoatCircle& Rock : BoatTrack::GetRocks())
				{
					bOk = bOk && FVector2D::Distance(Mouth - Sea * 15.0, Rock.Center / 100.0) > Rock.Radius / 100.0 + 40.0;
				}
				// Clear of the start's grandstand and piers.
				const double FromStart = FVector2D::Distance(Mouth, Gates[0].Center / 100.0);
				bOk = bOk && FromStart > 180.0;
				if (bOk && FromStart < BestScore)
				{
					BestScore = FromStart;
					OutMouth = Mouth;
					OutSeaward = Sea;
					bFound = true;
				}
			}
		}
	}
	return bFound;
}

FVector AGarageShowroom::ToWorld(const FVector& LocalMetres) const
{
	return GetActorTransform().TransformPosition(LocalMetres * 100.0);
}

void AGarageShowroom::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Mesh = BoatArt::Model(TEXT("Hangar"));
	FVector2D Mouth, Sea;
	if (Mesh == nullptr || !FindSpot(Mouth, Sea))
	{
		UE_LOG(LogBoatRace, Warning, TEXT("Garage: no hangar model or no cove for it; the garage uses the boat on the water."));
		SetActorHiddenInGame(true);
		return;
	}
	const float Yaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Sea.Y, Sea.X)));
	SetActorLocationAndRotation(FVector(Mouth.X * 100.0, Mouth.Y * 100.0, BoatTrack::WaterZ), FRotator(0.f, Yaw, 0.f));
	Hangar->SetStaticMesh(Mesh);
	Hangar->SetRelativeScale3D(FVector(BoatArt::UnitScale(Mesh, HangarHeightMetres)));
	BoatArt::Paint(Hangar);

	// Solid walls and quays, so a boat out exploring bumps into the building.
	struct FBlock { FVector Centre; FVector Half; };
	const FBlock Blocks[] =
	{
		{ FVector(-15.0, 9.5, 0.0), FVector(15.0, 5.0, 6.0) },
		{ FVector(-15.0, -9.5, 0.0), FVector(15.0, 5.0, 6.0) },
		{ FVector(-28.0, 0.0, 0.0), FVector(2.5, 5.0, 6.0) },
	};
	for (const FBlock& Block : Blocks)
	{
		UBoxComponent* Box = NewObject<UBoxComponent>(this);
		Box->SetupAttachment(GetRootComponent());
		Box->SetRelativeLocation(Block.Centre * 100.0);
		Box->SetBoxExtent(Block.Half * 100.0);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Box->RegisterComponent();
		Walls.Add(Box);
	}

	// Work lights under the roof: cool overhead fill, one warm key on the boat.
	for (const FVector& At : { FVector(-6.0, -6.0, 8.0), FVector(-6.0, 6.0, 8.0), FVector(-20.0, -6.0, 8.0), FVector(-20.0, 6.0, 8.0) })
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
		Light->SetupAttachment(GetRootComponent());
		Light->SetRelativeLocation(At * 100.0);
		Light->SetIntensity(9000.f);
		Light->SetAttenuationRadius(2200.f);
		Light->SetLightColor(FLinearColor(0.85f, 0.92f, 1.f));
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		Light->RegisterComponent();
		WorkLights.Add(Light);
	}
	KeyLight = NewObject<USpotLightComponent>(this);
	KeyLight->SetupAttachment(GetRootComponent());
	KeyLight->SetRelativeLocation(FVector(-4.0, -7.0, 7.5) * 100.0);
	KeyLight->SetRelativeRotation((BoatSpot - FVector(-4.0, -7.0, 7.5)).Rotation());
	KeyLight->SetIntensity(60000.f);
	KeyLight->SetAttenuationRadius(3000.f);
	KeyLight->SetOuterConeAngle(32.f);
	KeyLight->SetInnerConeAngle(14.f);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.9f, 0.75f));
	KeyLight->SetCastShadows(true);
	KeyLight->SetVisibility(false);
	KeyLight->RegisterComponent();

	bPlaced = true;
	UE_LOG(LogBoatRace, Log, TEXT("Garage: boathouse at (%.0f, %.0f) m facing %.0f degrees."), Mouth.X, Mouth.Y, Yaw);
}

void AGarageShowroom::ClearTerrainProps()
{
	// Rocks, trees and other scenery planted along the shore must not grow
	// through the floor. Done once, and again whenever the bay is rebuilt.
	TActorIterator<ABayTerrain> Terrain(GetWorld());
	if (!Terrain || ClearedTerrain == *Terrain)
	{
		return;
	}
	ClearedTerrain = *Terrain;
	const FTransform Frame = GetActorTransform();
	auto Inside = [&Frame](const FVector& World)
	{
		const FVector Local = Frame.InverseTransformPosition(World) / 100.0;
		return Local.X > -HangarDepth - 3.0 && Local.X < 4.0 && FMath::Abs(Local.Y) < HangarHalfWidth + 3.0;
	};
	int32 Removed = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == this || It->IsA<APawn>())
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Parts;
		It->GetComponents(Parts);
		for (UStaticMeshComponent* Part : Parts)
		{
			if (UInstancedStaticMeshComponent* Set = Cast<UInstancedStaticMeshComponent>(Part))
			{
				for (int32 Index = Set->GetInstanceCount() - 1; Index >= 0; --Index)
				{
					FTransform Instance;
					Set->GetInstanceTransform(Index, Instance, true);
					if (Inside(Instance.GetLocation()))
					{
						Set->RemoveInstance(Index);
						++Removed;
					}
				}
			}
			else if (Part->GetStaticMesh() != nullptr && Inside(Part->GetComponentLocation())
				&& Part->Bounds.SphereRadius < 1500.f) // the sea and the land are far bigger
			{
				Part->SetVisibility(false);
				Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				++Removed;
			}
		}
	}
	UE_LOG(LogBoatRace, Log, TEXT("Garage: cleared %d props from under the boathouse."), Removed);
}

void AGarageShowroom::SetActive(bool bInActive)
{
	if (bActive == bInActive || !bPlaced)
	{
		return;
	}
	bActive = bInActive;
	for (UPointLightComponent* Light : WorkLights)
	{
		Light->SetVisibility(bActive);
	}
	KeyLight->SetVisibility(bActive);
	Boat->SetVisibility(bActive);
}

void AGarageShowroom::ShowLoadout(const FBoatLoadout& Loadout, int32 Slot)
{
	if (!bPlaced || (bHasShown && Loadout == Shown && Slot == ShownSlot))
	{
		return;
	}
	const bool bNewBoat = !bHasShown || Loadout.Boat != Shown.Boat;
	Shown = Loadout;
	ShownSlot = Slot;
	bHasShown = true;
	const FBoatClass& Class = BoatGarage::GetBoat(Loadout.Boat);
	if (bNewBoat)
	{
		UStaticMesh* Mesh = BoatArt::Model(Class.Model);
		if (Mesh == nullptr)
		{
			Mesh = BoatArt::Model(TEXT("RaceBoat"));
		}
		Boat->SetStaticMesh(Mesh);
		if (Mesh != nullptr)
		{
			// Drawn at the class's length, like on the course, centred in the slip.
			const FBoxSphereBounds Bounds = Mesh->GetBounds();
			const float Longest = static_cast<float>(FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y)) * 2.f;
			const float Scale = Longest > 1.f ? Class.LengthCm / Longest : 1.f;
			Boat->SetWorldScale3D(FVector(Scale));
			// As on the course: outboards hang aft, so bounds centred forward
			// mean the importer turned the model round.
			BoatYaw = Bounds.Origin.X > 0.0 ? 180.f : 0.f;
			BoatCentring = FVector(-Bounds.Origin.X * Scale, -Bounds.Origin.Y * Scale, 0.0); // in the mesh's own frame
		}
	}
	BoatArt::FLook Look;
	Look.Paint = BoatGarage::PaintColor(Loadout.Paint, Slot);
	BoatArt::Paint(Boat, Look);
}

FTransform AGarageShowroom::GetCameraTransform(bool bGarageView, float Seconds) const
{
	const FVector Target = ToWorld(BoatSpot + FVector(0.0, 0.0, 1.0));
	FVector Eye;
	if (bGarageView)
	{
		// From the port quay, walking slowly round the boat and back.
		const double Angle = FMath::DegreesToRadians(-100.0 + 35.0 * FMath::Sin(Seconds * 0.15));
		Eye = ToWorld(BoatSpot + FVector(10.5 * FMath::Cos(Angle), 10.5 * FMath::Sin(Angle), 3.3));
	}
	else
	{
		// From just inside the doors, the boat coming at you out of the shed.
		Eye = ToWorld(FVector(5.0 + FMath::Sin(Seconds * 0.1) * 0.8, -3.8, 2.2));
	}
	return FTransform((Target - Eye).Rotation(), Eye);
}

void AGarageShowroom::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bPlaced)
	{
		return;
	}
	ClearTerrainProps(); // cheap once done; again if the bay is rebuilt
	if (!bActive)
	{
		return;
	}
	// The boat rides the same sea everything else does, nodding gently.
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const float Time = RaceState ? RaceState->GetServerNow() : static_cast<float>(World->GetTimeSeconds());
	const FVector Spot = ToWorld(BoatSpot);
	const float Height = OceanWaves::GetHeight(FVector2D(Spot.X, Spot.Y), Time);
	const FRotator Facing = GetActorRotation();
	const FRotator Nod(1.2f * FMath::Sin(Time * 0.9f), Facing.Yaw + BoatYaw, 1.5f * FMath::Sin(Time * 0.7f + 1.f));
	Boat->SetWorldLocationAndRotation(FVector(Spot.X, Spot.Y, Height) + Nod.RotateVector(BoatCentring), Nod);
}
