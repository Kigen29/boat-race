#include "BoatRaceGameState.h"

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
}

void ABoatRaceGameState::BeginPlay()
{
	Super::BeginPlay();

	// The bay is identical on every machine and never changes, so each one
	// builds its own rather than having it replicated piece by piece.
	UWorld* World = GetWorld();
	if (World != nullptr)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<ARaceEnvironment>(ARaceEnvironment::StaticClass(), FTransform::Identity, SpawnParameters);
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
