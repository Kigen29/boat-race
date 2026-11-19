#include "BoatNetCombatTest.h"

#include "BoatPawn.h"
#include "BoatRace.h"
#include "BoatRacePlayerState.h"
#include "BoatWeapons.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ABoatNetCombatTest::ABoatNetCombatTest()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ABoatNetCombatTest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatNetCombatTest, Shooter);
	DOREPLIFETIME(ABoatNetCombatTest, Victim);
	DOREPLIFETIME(ABoatNetCombatTest, Tracked);
	DOREPLIFETIME(ABoatNetCombatTest, Phase);
}

void ABoatNetCombatTest::Freeze(ABoatPawn* Boat)
{
	Boat->SetActorTickEnabled(false);
	Boat->Hull->SetSimulatePhysics(false);
	Boat->bPhysicsActive = false;
}

void ABoatNetCombatTest::SetPhase(uint8 Next)
{
	Phase = Next;
	ForceNetUpdate();
	UE_LOG(LogBoatRace, Log, TEXT("Net combat phase %d"), Phase);
}

void ABoatNetCombatTest::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	if (Age > 80.f)
	{
		UE_LOG(LogBoatRace, Error, TEXT("NETWORK COMBAT FAILED: timeout in phase %d"), Phase);
		FPlatformMisc::RequestExit(false);
		return;
	}
	if (HasAuthority())
	{
		TickServer(DeltaSeconds);
	}
	else if (GetOwner() != nullptr && Cast<APlayerController>(GetOwner()) != nullptr
		&& Cast<APlayerController>(GetOwner())->IsLocalController() && Shooter != nullptr && Victim != nullptr)
	{
		TickClient(DeltaSeconds);
	}
}

void ABoatNetCombatTest::TickServer(float DeltaSeconds)
{
	if (Phase == 0)
	{
		APlayerController* Remote = nullptr;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (PC->IsLocalController())
			{
				Victim = Cast<ABoatPawn>(PC->GetPawn());
			}
			else
			{
				Remote = PC;
				Shooter = Cast<ABoatPawn>(PC->GetPawn());
			}
		}
		if (Shooter == nullptr || Victim == nullptr || Remote == nullptr)
		{
			return;
		}
		SetOwner(Remote);
		Freeze(Shooter);
		Freeze(Victim);
		Shooter->TeleportBoat(FVector(0.f, 0.f, 50000.f), 0.f);
		Victim->TeleportBoat(FVector(6000.f, 0.f, 50000.f), 0.f);
		Shooter->Revive(0.f);
		Victim->Revive(0.f);
		SetPhase(1);
	}
	// Only the gun is advanced manually. Movement is held still so damage
	// assertions do not depend on random racing lines or collision impulses.
	Shooter->UpdateGun(DeltaSeconds);
	if (Phase == 1 && Victim->GetHealth() <= Victim->GetMaxHealth() - 9.f)
	{
		SetPhase(2);
	}
	if (Phase == 3)
	{
		if (Tracked == nullptr)
		{
			for (TActorIterator<ABoatMissile> It(GetWorld()); It; ++It)
			{
				if (It->GetOwner() == Shooter)
				{
					Tracked = *It;
					Tracked->Speed = 2000.f; // leave time for the client to observe the lock
					break;
				}
			}
		}
		if (bClientSawLock && Victim->GetHealth() < Victim->GetMaxHealth() && Shooter->GetMissileAmmo() == 2)
		{
			SetPhase(4);
		}
	}
}

void ABoatNetCombatTest::TrackHostMissile()
{
	Tracked = nullptr;
	Victim->LastFireTime = -100.f;
	Victim->GiveSpecials(3, 0, 0);
	Victim->FireMissile(Shooter, false);
	for (TActorIterator<ABoatMissile> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == Victim)
		{
			Tracked = *It;
			Tracked->Speed = 2000.f;
			break;
		}
	}
}

void ABoatNetCombatTest::TickClient(float DeltaSeconds)
{
	Freeze(Shooter);
	Freeze(Victim);
	if (LocalPhase != Phase)
	{
		LocalPhase = Phase;
		bActed = false;
		ReportClock = 0.f;
	}
	ReportClock -= DeltaSeconds;
	if (Phase == 1 && !bActed)
	{
		Shooter->HoldGun(true);
		bActed = true;
	}
	else if (Phase == 2)
	{
		Shooter->HoldGun(false);
		if (!Shooter->IsGunFiring() && Victim->GetHealth() < Victim->GetMaxHealth() && ReportClock <= 0.f)
		{
			ServerConfirm(2, Victim->GetHealth());
			ReportClock = 0.2f;
		}
	}
	else if (Phase == 3)
	{
		if (!bActed)
		{
			Shooter->UpdateMissileLock(DeltaSeconds, false);
			if (Shooter->GetLockProgress() >= 1.f)
			{
				Shooter->ServerFireMissile(Shooter->GetLockTarget());
				bActed = true;
			}
		}
		if (IsValid(Tracked) && Tracked->GetTarget() == Victim && ReportClock <= 0.f)
		{
			ServerConfirm(3, 0.f);
			ReportClock = 0.2f;
		}
	}
	else if (Phase == 4 && Victim->GetHealth() < Victim->GetMaxHealth() && Shooter->GetMissileAmmo() == 2 && ReportClock <= 0.f)
	{
		ServerConfirm(4, Victim->GetHealth());
		ReportClock = 0.2f;
	}
	else if (Phase == 5 && IsValid(Tracked))
	{
		if (!bActed && Tracked->GetTarget() == Shooter)
		{
			Shooter->ServerDropFlares();
			bActed = true;
		}
		if (bActed && Tracked->GetTarget() == nullptr && Shooter->GetFlareAmmo() == 2 && ReportClock <= 0.f)
		{
			ServerConfirm(5, Shooter->GetHealth());
			ReportClock = 0.2f;
		}
	}
	else if (Phase == 6 && !IsValid(Tracked) && Shooter->GetHealth() == Shooter->GetMaxHealth() && ReportClock <= 0.f)
	{
		ServerConfirm(6, Shooter->GetHealth());
		ReportClock = 0.2f;
	}
	else if (Phase == 7 && Victim->IsWrecked() && ReportClock <= 0.f)
	{
		ServerConfirm(7, Victim->GetHealth());
		ReportClock = 0.2f;
	}
	else if (Phase == 8 && !Victim->IsWrecked() && Victim->GetHealth() == Victim->GetMaxHealth()
		&& Victim->IsInvulnerable() && ReportClock <= 0.f)
	{
		ServerConfirm(8, Victim->GetHealth());
		ReportClock = 0.2f;
	}
	else if (Phase == 9)
	{
		UE_LOG(LogBoatRace, Log, TEXT("NETWORK COMBAT CLIENT PASSED"));
		ServerConfirm(9, 0.f);
		SetActorTickEnabled(false);
		FTimerHandle ExitTimer;
		GetWorld()->GetTimerManager().SetTimer(ExitTimer, [] { FPlatformMisc::RequestExit(false); }, 1.f, false);
	}
}

void ABoatNetCombatTest::ServerConfirm_Implementation(uint8 SeenPhase, float SeenHealth)
{
	if (SeenPhase != Phase)
	{
		return;
	}
	if (Phase == 2 && !Shooter->IsGunFiring() && FMath::IsNearlyEqual(SeenHealth, Victim->GetHealth()))
	{
		UE_LOG(LogBoatRace, Log, TEXT("Net combat verified: gun damage and trigger release"));
		Victim->Revive(0.f);
		Shooter->GiveSpecials(3, 3, 0);
		Shooter->LastFireTime = -100.f;
		SetPhase(3);
	}
	else if (Phase == 3)
	{
		bClientSawLock = true;
	}
	else if (Phase == 4 && FMath::IsNearlyEqual(SeenHealth, Victim->GetHealth()))
	{
		UE_LOG(LogBoatRace, Log, TEXT("Net combat verified: missile lock ammo and damage"));
		Shooter->Revive(0.f);
		Victim->Revive(0.f);
		Victim->TeleportBoat(FVector(0.f, 0.f, 50000.f), 0.f);
		Shooter->TeleportBoat(FVector(6000.f, 0.f, 50000.f), 0.f);
		Shooter->GiveSpecials(0, 3, 0);
		TrackHostMissile();
		SetPhase(5);
	}
	else if (Phase == 5 && IsValid(Tracked) && Tracked->GetTarget() == nullptr && Shooter->IsFlaring() && Shooter->GetFlareAmmo() == 2)
	{
		SetPhase(6);
	}
	else if (Phase == 6 && !IsValid(Tracked) && FMath::IsNearlyEqual(SeenHealth, Shooter->GetHealth())
		&& Shooter->GetHealth() == Shooter->GetMaxHealth())
	{
		UE_LOG(LogBoatRace, Log, TEXT("Net combat verified: client flares and no blast damage"));
		Victim->ApplyDamage(10000.f, nullptr, true);
		SetPhase(7);
	}
	else if (Phase == 7 && Victim->IsWrecked() && SeenHealth == 0.f)
	{
		SetPhase(8);
	}
	else if (Phase == 8 && !Victim->IsWrecked() && Victim->IsInvulnerable() && SeenHealth == Victim->GetHealth())
	{
		UE_LOG(LogBoatRace, Log, TEXT("Net combat verified: wreck recovery and respawn protection"));
		SetPhase(9);
	}
	else if (Phase == 9)
	{
		UE_LOG(LogBoatRace, Log, TEXT("NETWORK COMBAT SERVER PASSED"));
		SetActorTickEnabled(false);
		FTimerHandle ExitTimer;
		GetWorld()->GetTimerManager().SetTimer(ExitTimer, [] { FPlatformMisc::RequestExit(false); }, 2.f, false);
	}
}
