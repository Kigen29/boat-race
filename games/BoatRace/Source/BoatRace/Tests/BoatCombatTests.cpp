#include "../BoatPawn.h"
#include "../BoatRaceGameState.h"
#include "../BoatWeapons.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FCombatWorld
	{
		UWorld* World;
		ABoatPawn* Shooter;
		ABoatPawn* Target;

		FCombatWorld()
		{
			UWorld::InitializationValues Values;
			Values.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Values);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->SetGameState(World->SpawnActor<ABoatRaceGameState>());
			Shooter = World->SpawnActor<ABoatPawn>();
			Target = World->SpawnActor<ABoatPawn>();
			// Above all scenery: tests isolate physical cover from coast geometry.
			Shooter->TeleportBoat(FVector(0.f, 0.f, 50000.f), 0.f);
			Target->TeleportBoat(FVector(6000.f, 0.f, 50000.f), 0.f);
		}

		~FCombatWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}

		ABoatMissile* Missile()
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABoatMissile* Result = World->SpawnActor<ABoatMissile>(
				ABoatMissile::StaticClass(), Shooter->GetRaceLocation(), FRotator::ZeroRotator, Params);
			Result->Launch(Shooter, Target, false);
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatCoverTest, "BoatRace.Combat.SolidCover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatCoverTest::RunTest(const FString& Parameters)
{
	FCombatWorld Fixture;
	TestTrue(TEXT("Clear water permits lock"), ABoatMissile::CanLock(Fixture.Shooter, Fixture.Target));
	TestTrue(TEXT("Clear water permits gun target"), Fixture.Shooter->FindGunTarget() == Fixture.Target);

	AActor* Cover = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Cover);
	Cover->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(100.f, 500.f, 500.f));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Cover->SetActorLocation(FVector(3000.f, 0.f, 50120.f));
	TestFalse(TEXT("Solid cover blocks lock"), ABoatMissile::CanLock(Fixture.Shooter, Fixture.Target));
	TestNull(TEXT("Solid cover blocks gun damage target"), Fixture.Shooter->FindGunTarget());
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Removing cover restores lock"), ABoatMissile::CanLock(Fixture.Shooter, Fixture.Target));
	Fixture.Target->TeleportBoat(FVector(-6000.f, 0.f, 50000.f), 0.f);
	TestFalse(TEXT("Boat behind cannot be locked"), ABoatMissile::CanLock(Fixture.Shooter, Fixture.Target));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatMissileStepTest, "BoatRace.Combat.MissileCrossesTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatMissileStepTest::RunTest(const FString& Parameters)
{
	FCombatWorld Fixture;
	Fixture.Target->TeleportBoat(FVector(650.f, 0.f, 50000.f), 0.f);
	ABoatMissile* Missile = Fixture.Missile();
	TestTrue(TEXT("Missile acquires requested target"), Missile->GetTarget() == Fixture.Target);
	Missile->Tick(0.2f); // 1,300 cm of travel crosses the target between endpoints
	TestTrue(TEXT("Crossing missile detonates"), Missile->IsActorBeingDestroyed());
	TestTrue(TEXT("Crossing hit damages hull"), Fixture.Target->GetHealth() < Fixture.Target->GetMaxHealth());
	TestEqual(TEXT("Shooter is immune to its own blast"), Fixture.Shooter->GetHealth(), Fixture.Shooter->GetMaxHealth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatFlareTest, "BoatRace.Combat.FlaresAndLostTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatFlareTest::RunTest(const FString& Parameters)
{
	FCombatWorld Fixture;
	ABoatMissile* Missile = Fixture.Missile();
	Fixture.Target->GiveSpecials(0, 1, 0);
	Fixture.Target->RequestSpecial(1);
	TestTrue(TEXT("Flares activate"), Fixture.Target->IsFlaring());
	Missile->Tick(0.05f);
	TestNull(TEXT("Flares remove missile lock"), Missile->GetTarget());
	TestEqual(TEXT("Flares are consumed"), Fixture.Target->GetFlareAmmo(), 0);
	// Force the decoyed blast beside the hull, so this checks damage
	// suppression rather than merely exploding too far away to hurt it.
	Missile->SetActorLocation(Fixture.Target->GetRaceLocation());
	Missile->Speed = 0.f;
	Missile->Tick(8.f);
	TestEqual(TEXT("Decoyed blast does no hull damage"), Fixture.Target->GetHealth(), Fixture.Target->GetMaxHealth());

	ABoatMissile* Lost = Fixture.Missile();
	Fixture.Target->ApplyDamage(10000.f, nullptr);
	Lost->Tick(0.05f);
	TestNull(TEXT("Wrecked boat no longer receives lock warning"), Lost->GetTarget());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatProtectionTest, "BoatRace.Combat.DamageAndProtection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatProtectionTest::RunTest(const FString& Parameters)
{
	FCombatWorld Fixture;
	Fixture.Target->GrantShield(3.f);
	Fixture.Target->ApplyDamage(10000.f, nullptr, true);
	TestEqual(TEXT("Shield blocks gun damage"), Fixture.Target->GetHealth(), Fixture.Target->GetMaxHealth());
	Fixture.Target->Revive(0.f);
	TestTrue(TEXT("Lethal gun damage wrecks hull"), Fixture.Target->ApplyDamage(10000.f, nullptr, true));
	TestFalse(TEXT("Wrecks cannot be locked"), ABoatMissile::CanLock(Fixture.Shooter, Fixture.Target));
	Fixture.Target->Revive(3.f);
	Fixture.Target->ApplyDamage(10000.f, nullptr);
	TestFalse(TEXT("Revive clears wreck state"), Fixture.Target->IsWrecked());
	TestEqual(TEXT("Respawn protection blocks blast damage"), Fixture.Target->GetHealth(), Fixture.Target->GetMaxHealth());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBoatHomingTest, "BoatRace.Combat.HomingAndUnmatchedLaunch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBoatHomingTest::RunTest(const FString& Parameters)
{
	FCombatWorld Fixture;
	ABoatMissile* Missile = Fixture.Missile();
	Fixture.Target->TeleportBoat(FVector(6000.f, 3000.f, 50000.f), 0.f);
	Missile->Tick(0.5f);
	TestTrue(TEXT("Missile turns after a moving target"), Missile->GetActorLocation().Y > 0.f);
	TestTrue(TEXT("Turn stays within configured rate"), Missile->GetActorRotation().Yaw <= Missile->TurnRate * 0.5f + 0.1f);

	ABoatMissile* Straight = Fixture.World->SpawnActor<ABoatMissile>(
		ABoatMissile::StaticClass(), Fixture.Shooter->GetRaceLocation(), FRotator::ZeroRotator);
	Straight->Launch(Fixture.Shooter, nullptr, false);
	Straight->Tick(0.5f);
	TestNull(TEXT("Unlocked launch does not acquire another boat"), Straight->GetTarget());
	TestTrue(TEXT("Unlocked missile flies straight"), FMath::IsNearlyZero(Straight->GetActorLocation().Y));
	Fixture.Target->Destroy();
	Missile->Tick(0.05f);
	TestNull(TEXT("Destroyed target clears lock safely"), Missile->GetTarget());
	return true;
}
#endif
