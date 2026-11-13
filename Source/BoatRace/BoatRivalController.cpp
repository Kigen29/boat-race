#include "BoatRivalController.h"

#include "BoatPawn.h"
#include "BoatRaceGameMode.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "BayTerrain.h"

#include "Engine/World.h"
#include "EngineUtils.h"

ABoatRivalController::ABoatRivalController()
{
	// A player state, so a rival is on the timing screen, gets a grid slot
	// and a colour, and counts laps exactly like a person.
	bWantsPlayerState = true;
	PrimaryActorTick.bCanEverTick = true;
	LaneDriftPhase = FMath::FRandRange(0.f, 10.f);
}

FVector2D ABoatRivalController::PickTarget(const ABoatPawn* Boat, int32 NextGate, float Speed) const
{
	const TArray<FBoatGate>& Gates = BoatTrack::GetGates();
	const int32 Count = Gates.Num();
	const FVector Location = Boat->GetActorLocation();
	const FVector2D Position(Location.X, Location.Y);

	// Where we are along the leg from the last gate to the next one.
	const FVector2D From = Gates[(NextGate + Count - 1) % Count].Center;
	const FVector2D To = Gates[NextGate].Center;
	const FVector2D Leg = To - From;
	const double LegLength = FMath::Max(Leg.Size(), 1.0);
	double Along = FMath::Clamp(FVector2D::DotProduct(Position - From, Leg / LegLength), 0.0, LegLength);

	// Look further ahead the faster we go, as a driver would.
	double Ahead = 2600.0 + Speed * 1.1;
	int32 Gate = NextGate;
	FVector2D A = From, B = To;
	double Remaining = LegLength - Along;
	while (Ahead > Remaining)
	{
		Ahead -= Remaining;
		A = Gates[Gate].Center;
		Gate = (Gate + 1) % Count;
		B = Gates[Gate].Center;
		Remaining = (B - A).Size();
		Along = 0.0;
	}
	const FVector2D Direction = (B - A).GetSafeNormal();
	FVector2D Target = A + Direction * (Along + Ahead);
	const FVector2D ToInside(-Direction.Y, Direction.X);
	Target += ToInside * (LaneOffset * 100.0);

	// Swerve round anything solid between here and there.
	auto Avoid = [&](const FVector2D& Centre, float Radius)
	{
		const FVector2D Path = Target - Position;
		const double PathLength = FMath::Max(Path.Size(), 1.0);
		const FVector2D PathDir = Path / PathLength;
		const double T = FVector2D::DotProduct(Centre - Position, PathDir);
		if (T < 0.0 || T > PathLength + Radius)
		{
			return;
		}
		const FVector2D Closest = Position + PathDir * T;
		const double Miss = (Closest - Centre).Size();
		const double Need = Radius + 1100.0;
		if (Miss < Need)
		{
			FVector2D Away = Closest - Centre;
			if (Away.IsNearlyZero())
			{
				Away = FVector2D(-PathDir.Y, PathDir.X);
			}
			Target += Away.GetSafeNormal() * (Need - Miss + 600.0);
		}
	};
	for (const FBoatCircle& Rock : BoatTrack::GetRocks())
	{
		Avoid(Rock.Center, Rock.Radius);
	}
	for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools())
	{
		Avoid(Pool.Center, Pool.Radius * 0.8f);
	}
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const float Now = RaceState ? RaceState->GetServerNow() : 0.f;
	for (const FBoatBarge& Barge : BoatTrack::GetBarges())
	{
		Avoid(BoatTrack::GetBargeLocation(Barge, Now + 1.5f), 2600.f);
	}
	for (const FBoatLog& Log : BoatTrack::GetLogs())
	{
		Avoid(Log.Center, 350.f);
	}
	return Target;
}

void ABoatRivalController::UseWeapons(ABoatPawn* Boat, float Now)
{
	if (Now < NextWeaponTime)
	{
		return;
	}
	const FVector Location = Boat->GetActorLocation();
	const FVector Forward = Boat->GetActorForwardVector();
	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		const ABoatPawn* Other = *It;
		if (Other == Boat || Other->IsWrecked())
		{
			continue;
		}
		const FVector Offset = Other->GetRaceLocation() - Location;
		const float Distance = static_cast<float>(Offset.Size2D());
		const float Facing = static_cast<float>(FVector::DotProduct(Offset.GetSafeNormal2D(), Forward.GetSafeNormal2D()));
		if (Boat->GetTorpedoAmmo() > 0 && Distance > 2500.f && Distance < 11000.f && Facing > 0.985f)
		{
			Boat->RequestFire(false);
			NextWeaponTime = Now + FMath::FRandRange(2.5f, 5.f) / Skill;
			return;
		}
		if (Boat->GetMineAmmo() > 0 && Distance < 5000.f && Facing < -0.9f)
		{
			Boat->RequestFire(true);
			NextWeaponTime = Now + FMath::FRandRange(3.f, 6.f) / Skill;
			return;
		}
	}
}

void ABoatRivalController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ABoatPawn* Boat = Cast<ABoatPawn>(GetPawn());
	const ABoatRacePlayerState* Racer = GetPlayerState<ABoatRacePlayerState>();
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (Boat == nullptr || Racer == nullptr || RaceState == nullptr)
	{
		return;
	}
	if (RaceState->RaceState != ERaceState::Racing || Racer->bFinished || Boat->IsWrecked())
	{
		Boat->SetDriverBoost(false);
		Boat->SetDriverInput(0.f, 0.f);
		StuckTime = 0.f;
		return;
	}

	const float Now = RaceState->GetServerNow();
	const float Speed = FMath::Abs(Boat->GetForwardSpeed());
	const FVector Location = Boat->GetActorLocation();

	// Drift across the lane slowly, so rivals overtake and swap sides.
	LaneDriftPhase += DeltaSeconds * 0.11f;
	const float Lane = LaneOffset + 9.f * FMath::Sin(LaneDriftPhase);
	const float SavedLane = LaneOffset;
	LaneOffset = Lane;
	const FVector2D Target = PickTarget(Boat, Racer->NextGate, Speed);
	LaneOffset = SavedLane;

	const FVector2D ToTarget = Target - FVector2D(Location.X, Location.Y);
	const float WantYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	const float Error = FMath::FindDeltaAngleDegrees(static_cast<float>(Boat->GetActorRotation().Yaw), WantYaw);

	float Steer = FMath::Clamp(Error / 28.f, -1.f, 1.f);
	float Throttle = Skill;
	if (FMath::Abs(Error) > 55.f)
	{
		Throttle *= 0.55f; // lift for a hairpin rather than spin out
	}

	// Beached, pinned on a rock or nosed into a barge: back out and try again.
	const bool bShallow = BayTerrain::GetHeight(FVector2D(Location.X, Location.Y)) > -150.f;
	StuckTime = (Speed < 220.f || bShallow) ? StuckTime + DeltaSeconds : 0.f;
	if (StuckTime > 2.2f && ReverseUntil < Now)
	{
		ReverseUntil = Now + 1.4f;
		StuckTime = 0.f;
	}
	if (Now < ReverseUntil)
	{
		Throttle = -1.f;
		Steer = -Steer;
	}
	if (StuckTime > 7.f)
	{
		if (ABoatRaceGameMode* Mode = World->GetAuthGameMode<ABoatRaceGameMode>())
		{
			Mode->ResetBoatToLastGate(Boat);
		}
		StuckTime = 0.f;
	}

	// Boost on the straights when the tank is worth spending; better
	// rivals are greedier with it.
	Boat->SetDriverBoost(Throttle > 0.8f && FMath::Abs(Error) < 7.f && Boat->GetBoost() > (1.15f - Skill * 0.6f));
	Boat->SetDriverInput(Throttle, Steer);
	UseWeapons(Boat, Now);
}
