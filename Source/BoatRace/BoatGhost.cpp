#include "BoatGhost.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "BoatRacePlayerState.h"
#include "BoatPawn.h"
#include "BoatTrack.h"
#include "BoatRace.h"
#include "BayTerrain.h"

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
		// One ghost per direction: a lap of the reverse circuit is no guide to the other.
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BoatRace"),
			BoatTrack::IsReversed() ? TEXT("GhostReverse.dat") : TEXT("Ghost.dat"));
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

namespace
{
	constexpr float ReplayRate = 15.f;
	constexpr int32 ReplayFrames = 60 * 15;
	constexpr float ReplaySpeed = 0.5f;
}

AFinishReplay::AFinishReplay()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AFinishReplay::Clear()
{
	Stop();
	Tracks.Reset();
	Clock = 0.f;
	NextSample = 0.f;
	WinAt = -1.f;
}

void AFinishReplay::Record(float DeltaSeconds)
{
	Clock += DeltaSeconds;
	if (Clock < NextSample)
	{
		return;
	}
	NextSample = Clock + 1.f / ReplayRate;

	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		UStaticMeshComponent* Look = It->GetLookMesh();
		if (Look == nullptr || Look->GetStaticMesh() == nullptr)
		{
			continue;
		}
		FTrack* Track = Tracks.FindByPredicate([&](const FTrack& T) { return T.Boat == *It; });
		if (Track == nullptr)
		{
			Track = &Tracks.AddDefaulted_GetRef();
			Track->Boat = *It;
		}
		Track->Frames.Add(Look->GetComponentTransform());
		Track->Times.Add(Clock);
		// A ring would be cheaper, but a minute of four boats is 4 x 900
		// transforms, and dropping the front once a second keeps it simple.
		if (Track->Frames.Num() > ReplayFrames + 15)
		{
			Track->Frames.RemoveAt(0, 15, EAllowShrinking::No);
			Track->Times.RemoveAt(0, 15, EAllowShrinking::No);
		}
	}
}

void AFinishReplay::MarkWin()
{
	if (WinAt < 0.f)
	{
		WinAt = Clock;
	}
}

FTransform AFinishReplay::FTrack::Sample(float At) const
{
	if (Frames.Num() == 0)
	{
		return FTransform::Identity;
	}
	// The first frame recorded after At, and the one before it.
	const int32 B = FMath::Clamp(Algo::UpperBound(Times, At), 0, Frames.Num() - 1);
	const int32 A = FMath::Max(B - 1, 0);
	const float Span = Times[B] - Times[A];
	const float Alpha = Span > KINDA_SMALL_NUMBER ? FMath::Clamp((At - Times[A]) / Span, 0.f, 1.f) : 1.f;
	FTransform Result;
	Result.Blend(Frames[A], Frames[B], Alpha);
	return Result;
}

bool AFinishReplay::Play(APlayerController* InViewer)
{
	if (InViewer == nullptr || Tracks.Num() == 0 || bPlaying)
	{
		return false;
	}
	// Six seconds before the winner crossed the line to two after; without a
	// winner (a race abandoned), the last eight seconds.
	const float End = WinAt >= 0.f ? FMath::Min(WinAt + 2.f, Clock) : Clock;
	PlayFrom = FMath::Max(End - 8.f, 0.f);
	PlayTo = End;
	if (PlayTo - PlayFrom < 2.f)
	{
		return false;
	}

	// The camera follows whichever boat was nearest the front at the end:
	// the one that crossed the line, as near as this machine can tell.
	Focus = 0;
	float Nearest = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < Tracks.Num(); ++Index)
	{
		const ABoatPawn* Boat = Tracks[Index].Boat.Get();
		const ABoatRacePlayerState* Racer = Boat ? Boat->GetPlayerState<ABoatRacePlayerState>() : nullptr;
		if (Racer != nullptr && Racer->FinishPosition == 1)
		{
			Focus = Index;
			Nearest = -1.f;
		}
		else if (Nearest >= 0.f && Boat != nullptr)
		{
			const float Distance = FVector2D::Distance(FVector2D(Tracks[Index].Sample(PlayTo).GetLocation()), FVector2D(BoatTrack::GetGates()[0].Center));
			if (Distance < Nearest) { Nearest = Distance; Focus = Index; }
		}
	}

	for (FTrack& Track : Tracks)
	{
		ABoatPawn* Boat = Track.Boat.Get();
		if (Boat == nullptr || Boat->GetLookMesh() == nullptr)
		{
			continue;
		}
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this);
		Copy->SetupAttachment(GetRootComponent());
		Copy->SetUsingAbsoluteLocation(true);
		Copy->SetUsingAbsoluteRotation(true);
		Copy->SetUsingAbsoluteScale(true);
		Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Copy->RegisterComponent();
		UStaticMeshComponent* Look = Boat->GetLookMesh();
		Copy->SetStaticMesh(Look->GetStaticMesh());
		for (int32 Slot = 0; Slot < Look->GetNumMaterials(); ++Slot)
		{
			Copy->SetMaterial(Slot, Look->GetMaterial(Slot));
		}
		Track.Copy = Copy;
		Copies.Add(Copy);
		Boat->SetActorHiddenInGame(true);
	}

	if (Camera == nullptr)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
		if (Camera != nullptr)
		{
			Camera->GetCameraComponent()->bConstrainAspectRatio = false;
			Camera->GetCameraComponent()->SetFieldOfView(40.f);
		}
	}
	if (Camera == nullptr)
	{
		Stop();
		return false;
	}

	UE_LOG(LogBoatRace, Log, TEXT("Finish replay: %d boats, %.1f-%.1f s of %.1f, following #%d (%d frames from %.1f), at %s"),
		Tracks.Num(), PlayFrom, PlayTo, Clock, Focus, Tracks[Focus].Frames.Num(), Tracks[Focus].Times.Num() ? Tracks[Focus].Times[0] : -1.f,
		*Tracks[Focus].Sample(PlayTo).GetLocation().ToString());
	Viewer = InViewer;
	PlayAt = PlayFrom;
	SpotChosenAt = -100.f;
	bPlaying = true;
	InViewer->SetViewTargetWithBlend(Camera, 0.4f);
	Advance(0.f);
	return true;
}

bool AFinishReplay::Advance(float DeltaSeconds)
{
	if (!bPlaying)
	{
		return false;
	}
	PlayAt += DeltaSeconds * ReplaySpeed;
	if (PlayAt >= PlayTo)
	{
		Stop();
		return false;
	}

	for (FTrack& Track : Tracks)
	{
		if (Track.Copy != nullptr)
		{
			const bool bRecorded = Track.Times.Num() > 0 && PlayAt >= Track.Times[0];
			Track.Copy->SetVisibility(bRecorded);
			Track.Copy->SetWorldTransform(Track.Sample(PlayAt));
		}
	}

	// A television replay: a fixed camera on the bank ahead of the boat,
	// panning as it passes, then a cut to the next one along.
	const FTrack& Star = Tracks[Focus];
	const FVector Boat = Star.Sample(PlayAt).GetLocation();
	if (PlayAt - SpotChosenAt > 2.6f || FVector::Distance(CameraSpot, Boat) > 7000.f)
	{
		SpotChosenAt = PlayAt;
		const FVector Ahead = Star.Sample(FMath::Min(PlayAt + 1.6f, PlayTo)).GetLocation();
		FVector Travel = Ahead - Boat;
		Travel.Z = 0.0;
		const FVector Side = FVector::CrossProduct(Travel.GetSafeNormal(), FVector::UpVector);
		float Flip = (FMath::FloorToInt(PlayAt / 2.6f) % 2 == 0) ? 1.f : -1.f;
		// Alternate banks, but never put the camera inside a hill: take the
		// lower side, and stand above whatever ground is there.
		const FVector2D Here(Ahead.X, Ahead.Y);
		const FVector2D Across(Side.X, Side.Y);
		const float GroundA = BayTerrain::GetHeight(Here + Across * (1800.f * Flip));
		const float GroundB = BayTerrain::GetHeight(Here - Across * (1800.f * Flip));
		if (GroundA > 150.f && GroundB < GroundA)
		{
			Flip = -Flip;
		}
		CameraSpot = Ahead + Side * (1800.f * Flip);
		const float Ground = BayTerrain::GetHeight(FVector2D(CameraSpot.X, CameraSpot.Y));
		CameraSpot.Z = FMath::Max(FMath::Max(static_cast<double>(Ground), 0.0) + 300.0, Ahead.Z + 260.0);
	}
	const FVector Look = Boat + FVector(0.f, 0.f, 120.f) - CameraSpot;
	Camera->SetActorLocationAndRotation(CameraSpot, Look.Rotation());
	return true;
}

void AFinishReplay::Stop()
{
	if (!bPlaying && Copies.Num() == 0)
	{
		return;
	}
	bPlaying = false;
	for (FTrack& Track : Tracks)
	{
		Track.Copy = nullptr;
		if (ABoatPawn* Boat = Track.Boat.Get())
		{
			Boat->SetActorHiddenInGame(false);
		}
	}
	for (UStaticMeshComponent* Copy : Copies)
	{
		if (Copy != nullptr) { Copy->DestroyComponent(); }
	}
	Copies.Reset();
	if (APlayerController* PC = Viewer.Get())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(Pawn, 0.4f);
		}
	}
}
