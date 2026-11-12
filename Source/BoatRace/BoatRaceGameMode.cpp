#include "BoatRaceGameMode.h"

#include "BoatPawn.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRaceHUD.h"
#include "BoatRacePlayerController.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"

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
	return 0;
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
			RacePlayer->SlotIndex = FindFreeSlot();
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
		}
		break;

	default:
		break;
	}
}

void ABoatRaceGameMode::RequestStartRace(APlayerController* Requester)
{
	ABoatRaceGameState* RaceState = GetGameState<ABoatRaceGameState>();
	if (RaceState == nullptr || Requester == nullptr)
	{
		return;
	}

	// The host is the player whose controller lives on the server machine. On
	// a dedicated server nobody is, so there any player may start.
	const bool bMayStart = Requester->IsLocalController() || GetNetMode() == NM_DedicatedServer;
	const bool bIdle = RaceState->RaceState == ERaceState::Lobby || RaceState->RaceState == ERaceState::Results;
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
