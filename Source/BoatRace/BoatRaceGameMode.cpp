#include "BoatRaceGameMode.h"

#include "BoatPawn.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRaceHUD.h"
#include "BoatRacePlayerController.h"
#include "BoatRacePlayerState.h"
#include "BoatRivalController.h"
#include "BoatTrack.h"

#include "Misc/CommandLine.h"
#include "Engine/World.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"

ABoatRaceGameMode::ABoatRaceGameMode()
{
	PrimaryActorTick.bCanEverTick = true;

	DefaultPawnClass = ABoatPawn::StaticClass();
	PlayerControllerClass = ABoatRacePlayerController::StaticClass();
	PlayerStateClass = ABoatRacePlayerState::StaticClass();
	GameStateClass = ABoatRaceGameState::StaticClass();
	HUDClass = ABoatRaceHUD::StaticClass();
	bStartPlayersAsSpectators = false;
}

void ABoatRaceGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	if (ErrorMessage.IsEmpty() && GetNumPlayers() >= BoatTrack::MaxPlayers)
	{
		ErrorMessage = TEXT("This race is full (4 players).");
	}
}

int32 ABoatRaceGameMode::FindFreeSlot() const
{
	bool bUsed[BoatTrack::MaxPlayers] = { false, false, false, false };
	if (GameState != nullptr)
	{
		for (APlayerState* Player : GameState->PlayerArray)
		{
			const ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
			if (RacePlayer != nullptr && RacePlayer->SlotIndex >= 0 && RacePlayer->SlotIndex < BoatTrack::MaxPlayers)
			{
				bUsed[RacePlayer->SlotIndex] = true;
			}
		}
	}
	for (int32 Slot = 0; Slot < BoatTrack::MaxPlayers; ++Slot)
	{
		if (!bUsed[Slot])
		{
			return Slot;
		}
	}
	return -1;
}

void ABoatRaceGameMode::BeginPlay()
{
	Super::BeginPlay();

	FString Choice = Weather;
	FParse::Value(FCommandLine::Get(), TEXT("Weather="), Choice);
	// Test hooks: -Laps=N, and -AutoRace=S starts a race S seconds in.
	FParse::Value(FCommandLine::Get(), TEXT("Laps="), TotalLaps);
	FParse::Value(FCommandLine::Get(), TEXT("AutoRace="), AutoRaceAfter);
	if (ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>())
	{
		RaceState->SetWeather(RaceWeather::Parse(Choice));
		RaceState->TotalLaps = FMath::Clamp(TotalLaps, 1, 9);
		UE_LOG(LogBoatRace, Log, TEXT("Weather: %s"), RaceWeather::Get(RaceState->GetWeather()).Name);
	}
}

void ABoatRaceGameMode::RequestWeatherChange(APlayerController* Requester, int32 Step)
{
	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr || Requester == nullptr)
	{
		return;
	}
	const bool bHost = Requester->IsLocalController() || GetNetMode() == NM_DedicatedServer;
	if (!bHost || RaceState->RaceState == ERaceState::Countdown || RaceState->RaceState == ERaceState::Racing)
	{
		return; // the weather is the host's call, and only between races
	}
	RaceState->SetWeather(RaceWeather::FromIndex(static_cast<int32>(RaceState->GetWeather()) + Step));
	UE_LOG(LogBoatRace, Log, TEXT("Weather changed to %s"), RaceWeather::Get(RaceState->GetWeather()).Name);
}

void ABoatRaceGameMode::RequestRaceOption(APlayerController* Requester, int32 Option, int32 Step)
{
	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	const bool bHost = Requester != nullptr && (Requester->IsLocalController() || GetNetMode() == NM_DedicatedServer);
	if (RaceState == nullptr || !bHost || RaceState->RaceState == ERaceState::Countdown || RaceState->RaceState == ERaceState::Racing)
	{
		return;
	}
	if (Option == 0)
	{
		TotalLaps = FMath::Clamp(TotalLaps + Step, 1, 9);
		RaceState->TotalLaps = TotalLaps;
	}
	else
	{
		RaceState->RivalLevel = static_cast<uint8>(FMath::Clamp(static_cast<int32>(RaceState->RivalLevel) + Step, 0, 3));
	}
}

void ABoatRaceGameMode::SpawnRivals(ABoatRaceGameState* RaceState)
{
	RemoveRivals(0);
	// The host's choice: none, or easy / normal / hard around RivalSkill.
	const int32 Level = RaceState ? RaceState->RivalLevel : 2;
	if (!bFillWithRivals || Level == 0)
	{
		return;
	}
	static const float LevelSkill[] = { 0.f, 0.72f, 0.0f, 1.0f };
	const float BaseSkill = Level == 2 ? RivalSkill : LevelSkill[Level];
	static const TCHAR* const Names[] = { TEXT("Kestrel"), TEXT("Marlin"), TEXT("Barracuda"), TEXT("Osprey") };
	UWorld* World = GetWorld();
	for (int32 Attempt = 0; Attempt < BoatTrack::MaxPlayers; ++Attempt)
	{
		const int32 Slot = FindFreeSlot();
		if (Slot < 0 || World == nullptr)
		{
			break;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABoatRivalController* Rival = World->SpawnActor<ABoatRivalController>(ABoatRivalController::StaticClass(), FTransform::Identity, Params);
		ABoatRacePlayerState* Racer = Rival ? Rival->GetPlayerState<ABoatRacePlayerState>() : nullptr;
		if (Racer == nullptr)
		{
			if (Rival != nullptr)
			{
				Rival->Destroy();
			}
			break;
		}
		Racer->SlotIndex = Slot;
		Racer->SetIsABot(true);
		Racer->SetPlayerName(Names[Slot % UE_ARRAY_COUNT(Names)]);
		Racer->ResetRace();
		// Spread the field: one a touch quicker, one a touch slower.
		Rival->Skill = FMath::Clamp(BaseSkill + (Slot - 1.5f) * 0.03f, 0.5f, 1.f);
		Rival->LaneOffset = (Slot % 2 ? 1.f : -1.f) * (6.f + 4.f * (Slot / 2));
		RestartPlayerAtTransform(Rival, BoatTrack::GetGridTransform(Slot));
		Rivals.Add(Rival);
	}
	if (Rivals.Num() > 0)
	{
		UE_LOG(LogBoatRace, Log, TEXT("%d computer rival(s) on the grid."), Rivals.Num());
	}
}

void ABoatRaceGameMode::RemoveRivals(int32 Keep)
{
	while (Rivals.Num() > Keep)
	{
		ABoatRivalController* Rival = Rivals.Pop();
		if (Rival == nullptr)
		{
			continue;
		}
		if (APawn* Boat = Rival->GetPawn())
		{
			Boat->Destroy();
		}
		Rival->Destroy();
	}
}

void ABoatRaceGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	Rivals.RemoveAll([Exiting](const TObjectPtr<ABoatRivalController>& Rival) { return Rival == Exiting; });
}

void ABoatRaceGameMode::ResetBoatToLastGate(ABoatPawn* Boat)
{
	const ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	const ABoatRacePlayerState* Racer = Boat ? Boat->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	if (RaceState == nullptr || Racer == nullptr)
	{
		return;
	}
	const FTransform Target = (RaceState->RaceState == ERaceState::Racing && Racer->GatesPassed > 0)
		? BoatTrack::GetGateResetTransform(Racer->NextGate - 1)
		: BoatTrack::GetGridTransform(Racer->SlotIndex);
	Boat->TeleportBoat(Target.GetLocation(), static_cast<float>(Target.Rotator().Yaw));
}

void ABoatRaceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Deliberately not calling Super: the engine version looks for PlayerStart
	// actors, and this project has no map to put them in.
	if (NewPlayer == nullptr)
	{
		return;
	}

	int32 Slot = 0;
	if (ABoatRacePlayerState* RacePlayer = NewPlayer->GetPlayerState<ABoatRacePlayerState>())
	{
		if (RacePlayer->SlotIndex < 0)
		{
			// A person always beats a computer rival to a seat.
			if (FindFreeSlot() < 0 && Rivals.Num() > 0)
			{
				RemoveRivals(Rivals.Num() - 1);
			}
			RacePlayer->SlotIndex = FMath::Max(FindFreeSlot(), 0);
		}
		Slot = RacePlayer->SlotIndex;
		UE_LOG(LogBoatRace, Log, TEXT("%s joined in slot %d."), *RacePlayer->GetPlayerName(), Slot + 1);
	}

	RestartPlayerAtTransform(NewPlayer, BoatTrack::GetGridTransform(Slot));
}

void ABoatRaceGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr)
	{
		return;
	}

	if (AutoRaceAfter > 0.f && RaceState->RaceState == ERaceState::Lobby && GetWorld()->GetTimeSeconds() > AutoRaceAfter)
	{
		AutoRaceAfter = 0.f;
		StartCountdown(RaceState);
	}

	// Wrecked boats are rebuilt in every phase, so nobody is left stranded
	// waiting in the lobby either.
	UpdateWrecks(RaceState);

	switch (RaceState->RaceState)
	{
	case ERaceState::Countdown:
		if (RaceState->GetServerNow() >= RaceState->StateEndTime)
		{
			StartRacing(RaceState);
		}
		break;

	case ERaceState::Racing:
		UpdateRacing(RaceState);
		break;

	case ERaceState::Results:
		if (RaceState->GetServerNow() >= RaceState->StateEndTime)
		{
			RaceState->RaceState = ERaceState::Lobby;
			RemoveRivals(0);
		}
		break;

	default:
		break;
	}
}

void ABoatRaceGameMode::RequestStartRace(APlayerController* Requester, bool bRestart)
{
	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr || Requester == nullptr)
	{
		return;
	}

	// The host is the player whose controller lives on the server machine. On
	// a dedicated server nobody is, so there any player may start.
	const bool bMayStart = Requester->IsLocalController() || GetNetMode() == NM_DedicatedServer;
	// A restart from the pause menu may interrupt a race; ENTER may not.
	const bool bIdle = bRestart || RaceState->RaceState == ERaceState::Lobby || RaceState->RaceState == ERaceState::Results;
	if (bMayStart && bIdle)
	{
		StartCountdown(RaceState);
	}
}

void ABoatRaceGameMode::RequestResetBoat(APlayerController* Requester)
{
	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr || Requester == nullptr || RaceState->RaceState == ERaceState::Countdown)
	{
		return;
	}

	ABoatPawn* Boat = Cast<ABoatPawn>(Requester->GetPawn());
	const ABoatRacePlayerState* RacePlayer = Requester->GetPlayerState<ABoatRacePlayerState>();
	if (Boat == nullptr || RacePlayer == nullptr)
	{
		return;
	}

	FTransform Target = BoatTrack::GetGridTransform(RacePlayer->SlotIndex);
	if (RaceState->RaceState == ERaceState::Racing && !RacePlayer->bFinished && RacePlayer->GatesPassed > 0)
	{
		Target = BoatTrack::GetGateResetTransform(RacePlayer->NextGate - 1);
	}
	Boat->TeleportBoat(Target.GetLocation(), static_cast<float>(Target.Rotator().Yaw));
}

void ABoatRaceGameMode::UpdateWrecks(ABoatRaceGameState* RaceState)
{
	const float Now = RaceState->GetServerNow();

	for (APlayerState* Player : RaceState->PlayerArray)
	{
		ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		ABoatPawn* Boat = RacePlayer ? Cast<ABoatPawn>(RacePlayer->GetPawn()) : nullptr;
		if (Boat == nullptr || !Boat->IsWrecked())
		{
			continue;
		}

		if (Boat->ReviveAtServerTime <= 0.f)
		{
			// Let the wreck drift for a moment before it is pulled out: being
			// blown up has to cost time, which is the whole point of shooting.
			Boat->ReviveAtServerTime = Now + WreckSeconds;
			continue;
		}

		if (Now >= Boat->ReviveAtServerTime)
		{
			Boat->ReviveAtServerTime = 0.f;
			const FTransform Target = (RaceState->RaceState == ERaceState::Racing && RacePlayer->GatesPassed > 0)
				? BoatTrack::GetGateResetTransform(RacePlayer->NextGate - 1)
				: BoatTrack::GetGridTransform(RacePlayer->SlotIndex);
			Boat->TeleportBoat(Target.GetLocation(), static_cast<float>(Target.Rotator().Yaw));
			Boat->Revive(RespawnInvulnerableSeconds);
		}
	}
}

void ABoatRaceGameMode::StartCountdown(ABoatRaceGameState* RaceState)
{
	FinishedCount = 0;
	FirstFinishServerTime = -1.f;

	for (APlayerState* Player : RaceState->PlayerArray)
	{
		ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		if (RacePlayer == nullptr)
		{
			continue;
		}
		RacePlayer->ResetRace();

		if (ABoatPawn* Boat = Cast<ABoatPawn>(RacePlayer->GetPawn()))
		{
			const FTransform Grid = BoatTrack::GetGridTransform(RacePlayer->SlotIndex);
			Boat->TeleportBoat(Grid.GetLocation(), static_cast<float>(Grid.Rotator().Yaw));
			Boat->ReviveAtServerTime = 0.f;
			Boat->Revive(0.f);
		}
	}

	SpawnRivals(RaceState);

	RaceState->TotalLaps = FMath::Clamp(TotalLaps, 1, 20);
	RaceState->RaceEndTime = 0.f;
	RaceState->StateEndTime = RaceState->GetServerNow() + FMath::Max(CountdownSeconds, 1.f);
	RaceState->RaceState = ERaceState::Countdown;
	UE_LOG(LogBoatRace, Log, TEXT("Countdown started with %d player(s), %d laps."),
		RaceState->PlayerArray.Num(), RaceState->TotalLaps);
}

void ABoatRaceGameMode::StartRacing(ABoatRaceGameState* RaceState)
{
	RaceState->RaceStartTime = RaceState->GetServerNow();
	RaceState->RaceState = ERaceState::Racing;
}

void ABoatRaceGameMode::UpdateRacing(ABoatRaceGameState* RaceState)
{
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	const float Now = RaceState->GetServerNow();
	const float RaceTime = Now - RaceState->RaceStartTime;

	int32 RacerCount = 0;
	for (APlayerState* Player : RaceState->PlayerArray)
	{
		ABoatRacePlayerState* RacePlayer = Cast<ABoatRacePlayerState>(Player);
		ABoatPawn* Boat = RacePlayer ? Cast<ABoatPawn>(RacePlayer->GetPawn()) : nullptr;
		if (Boat == nullptr)
		{
			continue;
		}
		++RacerCount;
		if (RacePlayer->bFinished)
		{
			continue;
		}

		const FVector Location = Boat->GetRaceLocation();
		if (!Boat->bRaceHasLastLocation)
		{
			// First sample since a teleport: nothing to compare against yet.
			Boat->RaceLastLocation = Location;
			Boat->bRaceHasLastLocation = true;
			continue;
		}

		const FVector2D From(Boat->RaceLastLocation.X, Boat->RaceLastLocation.Y);
		const FVector2D To(Location.X, Location.Y);
		Boat->RaceLastLocation = Location;

		if (!Gates.IsValidIndex(RacePlayer->NextGate) || !BoatTrack::CrossesGate(Gates[RacePlayer->NextGate], From, To))
		{
			continue;
		}

		if (RacePlayer->NextGate == 0)
		{
			// The start / finish line. The first crossing only begins lap one.
			RacePlayer->LapsCompleted++;
			if (RacePlayer->LapsCompleted >= 1)
			{
				const float LapTime = RaceTime - RacePlayer->LapStartTime;
				if (RacePlayer->BestLapTime <= 0.f || LapTime < RacePlayer->BestLapTime)
				{
					RacePlayer->BestLapTime = LapTime;
				}
			}
			RacePlayer->LapStartTime = RaceTime;

			if (RacePlayer->LapsCompleted >= RaceState->TotalLaps)
			{
				RacePlayer->bFinished = true;
				RacePlayer->FinishTime = RaceTime;
				RacePlayer->FinishPosition = ++FinishedCount;
				if (FirstFinishServerTime < 0.f)
				{
					FirstFinishServerTime = Now;
				}
				UE_LOG(LogBoatRace, Log, TEXT("%s finished in position %d (%.2f s)."),
					*RacePlayer->GetPlayerName(), RacePlayer->FinishPosition, RaceTime);
			}
		}

		RacePlayer->GatesPassed++;
		UE_LOG(LogBoatRace, Log, TEXT("%s (slot %d) passed gate %d (lap %d)."), *RacePlayer->GetPlayerName(), RacePlayer->SlotIndex + 1,
			RacePlayer->NextGate + 1, FMath::Max(RacePlayer->LapsCompleted + 1, 1));
		RacePlayer->NextGate = (RacePlayer->NextGate + 1) % Gates.Num();
	}

	const bool bEveryoneHome = RacerCount == 0 || FinishedCount >= RacerCount;
	const bool bGraceExpired = FirstFinishServerTime >= 0.f && (Now - FirstFinishServerTime) > FinishGraceSeconds;
	if (bEveryoneHome || bGraceExpired)
	{
		EndRace(RaceState);
	}
}

void ABoatRaceGameMode::EndRace(ABoatRaceGameState* RaceState)
{
	const float Now = RaceState->GetServerNow();
	RaceState->RaceEndTime = Now - RaceState->RaceStartTime;
	RaceState->StateEndTime = Now + FMath::Max(ResultsSeconds, 3.f);
	RaceState->RaceState = ERaceState::Results;
}
