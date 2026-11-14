#include "BoatCoins.h"

#include "BayTerrain.h"
#include "BoatArt.h"
#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatRaceGameState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float CoinPickupRadius = 650.f;
	constexpr float CoinMagnetRadius = 3500.f;
	constexpr float CoinReturnSeconds = 25.f;

	/** True when nothing else is within Margin of the spot and it is open water. */
	bool IsClearForCoin(const FVector2D& Spot)
	{
		if (BayTerrain::GetClearanceMetres(Spot.X / 100.0, Spot.Y / 100.0) < 14.f)
		{
			return false;
		}
		for (const FBoatCircle& Rock : BoatTrack::GetRocks())
		{
			if (FVector2D::Distance(Spot, Rock.Center) < Rock.Radius + 1500.f) return false;
		}
		for (const FBoatRamp& Ramp : BoatTrack::GetRamps())
		{
			if (FVector2D::Distance(Spot, Ramp.Center) < 1800.f) return false;
		}
		for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools())
		{
			if (FVector2D::Distance(Spot, Pool.Center) < Pool.Radius + 800.f) return false;
		}
		for (const FBoatLog& Log : BoatTrack::GetLogs())
		{
			if (FVector2D::Distance(Spot, Log.Center) < 1200.f) return false;
		}
		for (const FVector2D& Point : BoatTrack::GetCratePoints())
		{
			if (FVector2D::Distance(Spot, Point) < 900.f) return false;
		}
		for (const FVector2D& Point : BoatTrack::GetDebrisPoints())
		{
			if (FVector2D::Distance(Spot, Point) < 1000.f) return false;
		}
		return true;
	}
}

ACourseCoins::ACourseCoins()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	Coins = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Coins"));
	SetRootComponent(Coins);
	Coins->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Coins->SetCastShadow(false);
	Coins->SetMobility(EComponentMobility::Movable);
	// Beyond 350 m a coin is a speck: not drawn at all.
	Coins->SetCullDistances(30000, 35000);
}

ACourseCoins* ACourseCoins::Find(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	TActorIterator<ACourseCoins> It(World);
	return It ? *It : nullptr;
}

void ACourseCoins::BeginPlay()
{
	Super::BeginPlay();
	UStaticMesh* Mesh = BoatArt::Model(TEXT("Coin"));
	if (Mesh == nullptr)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		Scale = 1.2f;
	}
	else
	{
		Scale = BoatArt::UnitScale(Mesh, 1.2f) * 1.7f; // a 2 m coin reads from a moving boat
	}
	Coins->SetStaticMesh(Mesh);
	BoatArt::Paint(Coins);
	Build();
}

void ACourseCoins::Build()
{
	BuiltFor = BoatTrack::IsReversed() ? 1 : 0;
	Spots.Reset();
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	for (int32 Leg = 0; Leg < Gates.Num(); ++Leg)
	{
		const FBoatGate& From = Gates[Leg];
		const FBoatGate& To = Gates[(Leg + 1) % Gates.Num()];
		const FVector2D Along = To.Center - From.Center;
		const FVector2D Across = FVector2D(-Along.Y, Along.X).GetSafeNormal();
		// Centre, left, right, and a slalom from one side to the other; the
		// sea mines sit at 19 m, so these stay inside 11.
		const int32 Pattern = Leg % 4;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float T = 0.32f + 0.09f * Index;
			float Side = 0.f;
			switch (Pattern)
			{
			case 1: Side = -1100.f; break;
			case 2: Side = 1100.f; break;
			case 3: Side = FMath::Lerp(-1100.f, 1100.f, Index / 4.f); break;
			default: break;
			}
			const FVector2D Spot = From.Center + Along * T + Across * Side;
			if (IsClearForCoin(Spot))
			{
				Spots.Add(Spot);
			}
		}
	}
	BackAt.Init(0.f, Spots.Num());
	Transforms.Init(FTransform::Identity, Spots.Num());
	Coins->ClearInstances();
	for (int32 Index = 0; Index < Spots.Num(); ++Index)
	{
		Coins->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Spots[Index], 0.0), FVector(Scale)));
	}
}

void ACourseCoins::ResetRace()
{
	Collected = 0;
	MagnetUntil = 0.f;
	for (float& Time : BackAt)
	{
		Time = 0.f;
	}
}

void ACourseCoins::StartMagnet(float Seconds)
{
	MagnetUntil = static_cast<float>(GetWorld()->GetTimeSeconds()) + Seconds;
}

bool ACourseCoins::IsMagnetOn() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < MagnetUntil;
}

void ACourseCoins::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (BuiltFor != (BoatTrack::IsReversed() ? 1 : 0))
	{
		Build(); // the track was turned round
	}
	if (Spots.Num() == 0)
	{
		return;
	}

	const ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();
	const float WaveTime = RaceState ? RaceState->GetServerNow() : static_cast<float>(World->GetTimeSeconds());
	const float Now = static_cast<float>(World->GetTimeSeconds());
	const bool bRacing = RaceState && RaceState->RaceState == ERaceState::Racing && !RaceState->bTimeTrial;

	// Only this machine's own boat collects.
	const APlayerController* Local = World->GetFirstPlayerController();
	const ABoatPawn* Boat = Local ? Cast<ABoatPawn>(Local->GetPawn()) : nullptr;
	const FVector2D BoatXY = Boat ? FVector2D(Boat->GetActorLocation()) : FVector2D(1e9, 1e9);
	const float Reach = IsMagnetOn() ? CoinMagnetRadius : CoinPickupRadius;

	// Pick-ups are checked every frame; the coins are only moved twenty times
	// a second, since every move re-sends the whole instance buffer.
	const bool bMove = Now - LastMoved >= 0.05f;
	if (bMove)
	{
		LastMoved = Now;
	}
	const float Spin = Now * 140.f;
	for (int32 Index = 0; Index < Spots.Num(); ++Index)
	{
		if (BackAt[Index] > 0.f && Now >= BackAt[Index])
		{
			BackAt[Index] = 0.f;
		}
		const bool bThere = BackAt[Index] <= 0.f;
		if (bThere && bRacing && Boat != nullptr && !Boat->IsWrecked() && FVector2D::DistSquared(BoatXY, Spots[Index]) < Reach * Reach)
		{
			BackAt[Index] = Now + CoinReturnSeconds;
			++Collected;
			BoatAudio::Play2D(this, EBoatSound::Coin, 0.55f, 1.f + 0.04f * (Collected % 5));
		}
		if (!bMove)
		{
			continue;
		}
		const float Height = OceanWaves::GetHeight(Spots[Index], WaveTime);
		// A collected coin shrinks away to nothing rather than vanishing.
		const float Size = BackAt[Index] > 0.f ? 0.f : Scale;
		Transforms[Index] = FTransform(FRotator(0.f, Spin + Index * 23.f, 0.f),
			FVector(Spots[Index].X, Spots[Index].Y, Height + 40.f + 25.f * FMath::Sin(Now * 2.f + Index)), FVector(FMath::Max(Size, 0.001f)));
	}
	if (bMove)
	{
		Coins->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
	}
}
