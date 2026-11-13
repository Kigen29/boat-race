#include "BoatGhost.h"

#include "BoatArt.h"

#include "Algo/BinarySearch.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace
{
	FString GhostPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BoatRace"), TEXT("Ghost.dat"));
	}

	const int32 GhostVersion = 1;
}

bool FGhostLap::Sample(float Seconds, FVector& OutLocation, FRotator& OutRotation) const
{
	if (!IsValid() || Seconds < 0.f || Seconds > LapTime)
	{
		return false;
	}
	// Binary search for the samples either side, then blend between them.
	int32 Upper = Algo::LowerBound(Times, Seconds);
	Upper = FMath::Clamp(Upper, 1, Times.Num() - 1);
	const int32 Lower = Upper - 1;
	const float Span = FMath::Max(Times[Upper] - Times[Lower], 1.e-3f);
	const float Alpha = FMath::Clamp((Seconds - Times[Lower]) / Span, 0.f, 1.f);
	OutLocation = FMath::Lerp(Locations[Lower], Locations[Upper], Alpha);
	OutRotation = FMath::Lerp(Rotations[Lower], Rotations[Upper], Alpha);
	return true;
}

bool FGhostLap::Save() const
{
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	int32 Version = GhostVersion;
	float Lap = LapTime;
	Writer << Version << Lap;
	TArray<float> T = Times;
	TArray<FVector> L = Locations;
	TArray<FRotator> R = Rotations;
	TArray<float> S = Splits;
	Writer << T << L << R << S;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(GhostPath()), true);
	return FFileHelper::SaveArrayToFile(Bytes, *GhostPath());
}

bool FGhostLap::Load()
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *GhostPath(), FILEREAD_Silent))
	{
		return false;
	}
	FMemoryReader Reader(Bytes);
	int32 Version = 0;
	Reader << Version;
	if (Version != GhostVersion)
	{
		return false;
	}
	Reader << LapTime << Times << Locations << Rotations << Splits;
	return IsValid() && Times.Num() == Locations.Num() && Times.Num() == Rotations.Num();
}

AGhostBoat::AGhostBoat()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hull->SetCastShadow(false);
}

void AGhostBoat::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Mesh = BoatArt::Model(TEXT("RaceBoat"));
	if (Mesh == nullptr)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	Hull->SetStaticMesh(Mesh);
	// Same size as the real boat: 7.7 m over the outboards, on the hull's line.
	if (Mesh != nullptr)
	{
		const float Longest = static_cast<float>(FMath::Max(Mesh->GetBounds().BoxExtent.X, Mesh->GetBounds().BoxExtent.Y)) * 2.f;
		Hull->SetRelativeScale3D(FVector(Longest > 1.f ? 770.f / Longest : 1.f));
	}
	// Every slot a pale see-through cyan, so it never reads as a real rival.
	for (int32 Slot = 0; Slot < Hull->GetNumMaterials(); ++Slot)
	{
		Hull->SetMaterial(Slot, BoatArt::SoftMaterial(Hull, FLinearColor(0.3f, 1.2f, 1.6f), 0.35f));
	}
	SetActorHiddenInGame(true);
}

void AGhostBoat::Show(const FGhostLap* Lap, float SecondsIntoLap)
{
	FVector Location;
	FRotator Rotation;
	if (Lap == nullptr || !Lap->Sample(SecondsIntoLap, Location, Rotation))
	{
		SetActorHiddenInGame(true);
		return;
	}
	SetActorHiddenInGame(false);
	// The hull's centre rides at about the waterline; the model sits on its keel.
	SetActorLocationAndRotation(Location - FVector(0.f, 0.f, 65.f), Rotation);
}
