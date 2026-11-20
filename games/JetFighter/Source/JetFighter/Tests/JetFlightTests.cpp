#include "../JetPawn.h"

#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
// Test access keeps artificial input and teleporting out of the player API.
struct FJetFlightTestAccess
{
    static void Reset(AJetPawn* Jet, float Speed = 150.f)
    {
        Jet->ResetFlight();
        Jet->Velocity = FVector(Speed, 0, 0);
        Jet->bAssisted = true;
    }
    static void Advance(AJetPawn* Jet, float Dt, const FJetControls& Controls)
    {
        Jet->AdvanceFlight(Dt, Controls);
    }
    static void Velocity(AJetPawn* Jet, const FVector& V) { Jet->Velocity = V; }
    static void Aim(AJetPawn* Jet, const FRotator& R) { Jet->AimRotation = R; }
    static FJetControls AimInput(const AJetPawn* Jet) { return Jet->AimControls(); }
};

namespace JetFlightTests
{
    struct FWorld
    {
        UWorld* World;
        AJetPawn* Jet;
        FWorld()
        {
            UWorld::InitializationValues Values;
            Values.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &Values);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            Jet = World->SpawnActor<AJetPawn>();
            // Envelope checks can travel further than the playable arena.
            Jet->AirspaceRadiusMetres = 1.e8f;
            Jet->AirspaceCeilingMetres = 1.e8f;
            FJetFlightTestAccess::Reset(Jet);
        }
        ~FWorld()
        {
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        float Fly(float Seconds, int32 Fps, const FJetControls& Controls)
        {
            float Peak = 0.f;
            for (int32 Frame = 0; Frame < FMath::RoundToInt(Seconds*Fps); ++Frame)
            {
                FJetFlightTestAccess::Advance(Jet, 1.f/Fps, Controls);
                Peak = FMath::Max(Peak, Jet->LoadG);
            }
            return Peak;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetEnvelopeTest, "JetFighter.Flight.FlightEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetEnvelopeTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    FJetControls Input;
    Input.Throttle = F.Jet->CruiseThrottle;
    F.Fly(30.f, 60, Input);
    TestTrue(TEXT("Hands-off cruise holds altitude within 60 m"), FMath::Abs(F.Jet->GetActorLocation().Z/100.-1500.) < 60.);
    TestTrue(TEXT("Cruise matches numerical mirror"), FMath::Abs(F.Jet->SpeedKmh()-544.f) < 3.f);
    FJetFlightTestAccess::Reset(F.Jet);
    Input.Throttle = 1.f;
    F.Fly(60.f, 60, Input);
    TestTrue(TEXT("Full throttle matches mirror, 704 km/h"), FMath::Abs(F.Jet->SpeedKmh()-704.f) < 3.f);
    FJetFlightTestAccess::Reset(F.Jet);
    Input.bBoost = true;
    F.Fly(60.f, 60, Input);
    TestTrue(TEXT("Afterburner matches mirror, 973 km/h"), FMath::Abs(F.Jet->SpeedKmh()-973.f) < 3.f);
    FJetFlightTestAccess::Reset(F.Jet, 170.f);
    Input.bBoost = false;
    Input.Pitch = 1.f;
    const float Peak = F.Fly(2.f, 60, Input);
    TestTrue(TEXT("Full pull respects G limit"), Peak <= F.Jet->MaxG+0.4f);
    TestTrue(TEXT("Pull raises flight path at least 40 degrees"), F.Jet->GetFlightVelocity().Rotation().Pitch > 40.f);
    TestFalse(TEXT("Assisted pull does not stall"), F.Jet->IsStalling());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetAimTest, "JetFighter.Flight.ControlAxesAndMouseAim",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetAimTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    FJetControls Input;
    Input.Roll = 0.5f;
    F.Fly(0.5f, 120, Input);
    TestTrue(TEXT("Right stick lowers right wing"), F.Jet->GetActorRightVector().Z < -0.4);
    FJetFlightTestAccess::Reset(F.Jet);
    Input.Roll = 0.f;
    Input.Yaw = 0.5f;
    F.Fly(1.f, 120, Input);
    TestTrue(TEXT("Right rudder turns right"), F.Jet->GetActorForwardVector().Y > 0.05);

    for (float Heading : {60.f, -60.f})
    {
        FJetFlightTestAccess::Reset(F.Jet);
        FJetFlightTestAccess::Aim(F.Jet, FRotator(10.f, Heading, 0.f));
        for (int32 I = 0; I < 15*120; ++I)
        {
            Input = FJetFlightTestAccess::AimInput(F.Jet);
            FJetFlightTestAccess::Advance(F.Jet, 1.f/120.f, Input);
        }
        const FRotator Path = F.Jet->GetFlightVelocity().Rotation();
        AddInfo(FString::Printf(TEXT("Aim %.0f: heading %.1f, pitch %.1f"), Heading, Path.Yaw, Path.Pitch));
        TestTrue(TEXT("Mouse aim captures heading within five degrees"), FMath::Abs(FMath::FindDeltaAngleDegrees(Path.Yaw, Heading)) < 5.f);
        TestTrue(TEXT("Mouse aim captures climb within three degrees"), FMath::Abs(Path.Pitch-10.f) < 3.f);
        TestFalse(TEXT("Aim does not stall"), F.Jet->IsStalling());
        TestFalse(TEXT("Aim remains airborne"), F.Jet->bCrashed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetFrameRateTest, "JetFighter.Flight.FrameRateIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetFrameRateTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    FVector Reference;
    for (int32 Fps : {20, 60, 144})
    {
        FJetFlightTestAccess::Reset(F.Jet, 170.f);
        FJetControls Input;
        Input.Pitch = 0.6f;
        Input.Roll = 0.4f;
        Input.Yaw = 0.1f;
        Input.Throttle = 1.f;
        F.Fly(1.f, Fps, Input);
        Input.Roll = 0.f;
        F.Fly(5.f, Fps, Input);
        if (Fps == 20) Reference = F.Jet->GetActorLocation();
        TestTrue(TEXT("20/60/144 fps finish within two metres"), FVector::Dist(Reference, F.Jet->GetActorLocation()) < 200.f);
        TestFalse(TEXT("Manoeuvre remains airborne"), F.Jet->bCrashed);
    }
    for (int32 Fps : {20, 60, 144})
    {
        FJetFlightTestAccess::Reset(F.Jet);
        FJetFlightTestAccess::Aim(F.Jet, FRotator(10.f, 60.f, 0.f));
        FJetControls Input;
        Input.bAim = true;
        Input.Throttle = F.Jet->CruiseThrottle;
        F.Fly(15.f, Fps, Input);
        if (Fps == 20) Reference = F.Jet->GetActorLocation();
        TestTrue(TEXT("Mouse autopilot also agrees across render frame rates"), FVector::Dist(Reference, F.Jet->GetActorLocation()) < 200.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetCollisionTest, "JetFighter.Flight.CollisionAndRespawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetCollisionTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    FJetControls Input;
    F.Jet->SetActorLocation(FVector(0, 0, 2000));
    FJetFlightTestAccess::Velocity(F.Jet, FVector(100, 0, -200));
    TestTrue(TEXT("Fast descent warns pilot before impact"), F.Jet->NeedsPullUp());
    F.Fly(0.2f, 120, Input);
    TestTrue(TEXT("Sea crossing crashes aircraft"), F.Jet->bCrashed);
    F.Fly(F.Jet->RespawnDelay+0.1f, 120, Input);
    TestFalse(TEXT("Crash respawns automatically"), F.Jet->bCrashed);
    TestTrue(TEXT("Respawn restores safe altitude"), F.Jet->GetActorLocation().Z > 149000.f);
    TestTrue(TEXT("Respawn clears descent warning"), !F.Jet->NeedsPullUp());

    FJetFlightTestAccess::Reset(F.Jet, 250.f);
    AActor* Cover = F.World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Cover);
    Cover->SetRootComponent(Box);
    // Only 2 cm thick: the jet crosses this between fixed steps.
    Box->SetBoxExtent(FVector(1, 1000, 1000));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Cover->SetActorLocation(FVector(3000, 0, 150000));
    F.Fly(0.2f, 120, Input);
    TestTrue(TEXT("Swept movement catches a thin obstacle"), F.Jet->bCrashed);
    TestTrue(TEXT("Jet stops before the obstacle"), F.Jet->GetActorLocation().X < 3000.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetAirspaceTest, "JetFighter.Flight.AirspaceRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetAirspaceTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    F.Jet->AirspaceRadiusMetres = 6000.f;
    F.Jet->AirspaceCeilingMetres = 6000.f;
    FJetControls Input;
    F.Jet->SetActorLocation(FVector(610000, 0, 150000));
    F.Fly(1.f, 120, Input);
    TestTrue(TEXT("Leaving radius starts grace timer"), F.Jet->IsOutsideAirspace());
    TestFalse(TEXT("Boundary grants time to return"), F.Jet->bCrashed);
    F.Jet->SetActorLocation(FVector(0, 0, 150000));
    F.Fly(0.1f, 120, Input);
    TestFalse(TEXT("Reentry clears warning"), F.Jet->IsOutsideAirspace());
    F.Jet->SetActorLocation(FVector(0, 0, 610000));
    F.Fly(1.f, 120, Input);
    TestTrue(TEXT("Ceiling also starts grace timer"), F.Jet->IsOutsideAirspace());
    F.Fly(F.Jet->AirspaceGraceSeconds, 120, Input);
    TestTrue(TEXT("Ignoring boundary eventually respawns"), F.Jet->bCrashed);
    return true;
}
#endif
