#include "BoatRaceGameState.h"
#include "EngineUtils.h"
#include "BoatTrack.h"

#include "RaceEnvironment.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

void ABoatRaceGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoatRaceGameState, RaceState);
	DOREPLIFETIME(ABoatRaceGameState, StateEndTime);
	DOREPLIFETIME(ABoatRaceGameState, RaceStartTime);
	DOREPLIFETIME(ABoatRaceGameState, RaceEndTime);
	DOREPLIFETIME(ABoatRaceGameState, TotalLaps);
	DOREPLIFETIME(ABoatRaceGameState, WeatherIndex);
	DOREPLIFETIME(ABoatRaceGameState, bReverse);
	DOREPLIFETIME(ABoatRaceGameState, RivalLevel);
	DOREPLIFETIME(ABoatRaceGameState, bTimeTrial);
	DOREPLIFETIME(ABoatRaceGameState, bElimination);
}

void ABoatRaceGameState::BeginPlay()
{
	Super::BeginPlay();
	BoatTrack::SetReversed(bReverse);
	BuildBay();
}

void ABoatRaceGameState::BuildBay()
{
	// The bay is identical on every machine, so each one builds its own
	// rather than having it replicated piece by piece; only the direction is
	// sent. A rebuild replaces the old one whole.
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	for (TActorIterator<ARaceEnvironment> It(World); It; ++It)
	{
		It->Destroy();
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World->SpawnActor<ARaceEnvironment>(ARaceEnvironment::StaticClass(), FTransform::Identity, SpawnParameters);
}

void ABoatRaceGameState::SetReverse(bool bInReverse)
{
	if (bReverse != bInReverse)
	{
		bReverse = bInReverse;
		OnRep_Reverse();
	}
}

void ABoatRaceGameState::OnRep_Reverse()
{
	BoatTrack::SetReversed(bReverse);
	// Before BeginPlay the first build will use the new direction anyway.
	if (HasActorBegunPlay())
	{
		BuildBay();
	}
}

float ABoatRaceGameState::GetServerNow() const
{
	return static_cast<float>(GetServerWorldTimeSeconds());
}

float ABoatRaceGameState::GetRaceTime() const
{
	switch (RaceState)
	{
	case ERaceState::Racing:
		return FMath::Max(GetServerNow() - RaceStartTime, 0.f);
	case ERaceState::Results:
		return RaceEndTime;
	default:
		return 0.f;
	}
}

void ABoatRaceGameState::ReportWreck(const FString& Victim, const FString& Attacker)
{
	if (HasAuthority())
	{
		MulticastWreck(Victim, Attacker);
	}
}

void ABoatRaceGameState::MulticastWreck_Implementation(const FString& Victim, const FString& Attacker)
{
	FBoatKillFeedLine Line;
	Line.Text = Attacker.IsEmpty()
		? FString::Printf(TEXT("%s was wrecked"), *Victim)
		: FString::Printf(TEXT("%s  wrecked  %s"), *Attacker, *Victim);

	const UWorld* World = GetWorld();
	Line.ArrivedAt = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;

	KillFeed.Add(Line);
	// Only the last few are ever shown.
	while (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}
}

void ABoatRaceGameState::Announce(const FString& Text)
{
	if (HasAuthority())
	{
		MulticastNotice(Text);
	}
}

void ABoatRaceGameState::MulticastNotice_Implementation(const FString& Text)
{
	FBoatKillFeedLine Line;
	Line.Text = Text;
	const UWorld* World = GetWorld();
	Line.ArrivedAt = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
	KillFeed.Add(Line);
	while (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}
}
