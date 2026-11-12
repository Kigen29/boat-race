#include "BoatRacePlayerState.h"

#include "Net/UnrealNetwork.h"

void ABoatRacePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoatRacePlayerState, SlotIndex);
	DOREPLIFETIME(ABoatRacePlayerState, LapsCompleted);
	DOREPLIFETIME(ABoatRacePlayerState, NextGate);
	DOREPLIFETIME(ABoatRacePlayerState, GatesPassed);
	DOREPLIFETIME(ABoatRacePlayerState, bFinished);
	DOREPLIFETIME(ABoatRacePlayerState, FinishTime);
	DOREPLIFETIME(ABoatRacePlayerState, FinishPosition);
	DOREPLIFETIME(ABoatRacePlayerState, BestLapTime);
	DOREPLIFETIME(ABoatRacePlayerState, Hits);
	DOREPLIFETIME(ABoatRacePlayerState, Wrecks);
}

void ABoatRacePlayerState::ResetRace()
{
	LapsCompleted = -1;
	NextGate = 0;
	GatesPassed = 0;
	bFinished = false;
	FinishTime = 0.f;
	FinishPosition = 0;
	BestLapTime = 0.f;
	Hits = 0;
	Wrecks = 0;
	LapStartTime = 0.f;
}
