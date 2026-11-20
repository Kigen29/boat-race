#include "../JetPawn.h"
#include "../JetTerrain.h"
#include "../JetScenery.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ProceduralMeshComponent.h"

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
    static void WaterView(AJetWaterSurface* Water, const FVector& Eye) { Water->RefreshForView(Eye); }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetTerrainMeshTest, "JetFighter.World.TerrainMatchesMesh",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetTerrainMeshTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    auto* Scenery = F.World->SpawnActor<AJetScenery>();
    Scenery->DispatchBeginPlay();
    const auto* Section = Scenery->GetTerrainMesh()->GetProcMeshSection(0);
    if (!TestNotNull(TEXT("Terrain section exists"), Section)) return false;
    TestEqual(TEXT("Terrain has a fixed triangle budget"), Section->ProcIndexBuffer.Num()/3, JetTerrain::Cells*JetTerrain::Cells*2);
    double LargestError = 0.;
    int32 LandSamples = 0;
    for (int32 I = 0; I < Section->ProcIndexBuffer.Num(); I += 3*97)
    {
        const FVector A = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I]].Position;
        const FVector B = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+1]].Position;
        const FVector C = Section->ProcVertexBuffer[Section->ProcIndexBuffer[I+2]].Position;
        // Sample inside actual rendered triangles, rather than just checking
        // the generating function against itself at grid vertices.
        const FVector P = A*0.2+B*0.3+C*0.5;
        const double Error = FMath::Abs(JetTerrain::HeightMetres(P.X/100., P.Y/100.)*100.-P.Z);
        LargestError = FMath::Max(LargestError, Error);
        if (P.Z > 0.) ++LandSamples;
    }
    TestTrue(TEXT("Samples include islands above water"), LandSamples > 10);
    TestTrue(TEXT("Collision height matches visible triangles within 1 cm"), LargestError < 1.);
    TestTrue(TEXT("Spawn corridor remains open water"), JetTerrain::HeightMetres(0, 0) < 0.);
    TestTrue(TEXT("East landmark rises above 900 m"), JetTerrain::HeightMetres(2400, 1200) > 900.);
    for (double X : {-500., 0., 500.})
        for (double Y : {-2835., -2800., -2765.})
            TestTrue(TEXT("The full airstrip sits on its level plateau"), FMath::Abs(JetTerrain::HeightMetres(X, Y)-220.) < 0.01);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetGroundTest, "JetFighter.World.GroundImpactAndWarning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetGroundTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    F.Jet->SetActorLocation(FVector(240000, 120000, 96000));
    FJetFlightTestAccess::Velocity(F.Jet, FVector(20, 0, -100));
    TestTrue(TEXT("Clearance is relative to mountain, not sea level"), F.Jet->GetSurfaceClearance() < 12.f);
    TestTrue(TEXT("Descent towards mountain warns before contact"), F.Jet->NeedsPullUp());
    F.Fly(0.2f, 120, FJetControls());
    TestTrue(TEXT("Aircraft crashes on mountain well above the sea"), F.Jet->bCrashed && F.Jet->GetActorLocation().Z > 90000.);
    F.Fly(F.Jet->RespawnDelay+0.1f, 120, FJetControls());
    TestFalse(TEXT("Terrain impact respawns safely"), F.Jet->bCrashed);
    TestTrue(TEXT("Respawn has safe surface clearance"), F.Jet->GetSurfaceClearance() > 1400.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJetWaterBudgetTest, "JetFighter.World.AltitudeWaterBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FJetWaterBudgetTest::RunTest(const FString& Parameters)
{
    JetFlightTests::FWorld F;
    auto* Water = F.World->SpawnActor<AJetWaterSurface>();
    Water->DispatchBeginPlay();
    auto* Detail = Cast<UProceduralMeshComponent>(Water->GetRootComponent());
    auto* Distant = Water->FindComponentByClass<UStaticMeshComponent>();
    if (!TestNotNull(TEXT("Detail water exists"), Detail) || !TestNotNull(TEXT("Distant water exists"), Distant)) return false;
    const int32 VertexBudget = Detail->GetProcMeshSection(0)->ProcVertexBuffer.Num();
    FJetFlightTestAccess::WaterView(Water, FVector(0, 0, 150000));
    TestFalse(TEXT("Aerial view draws only one flat water sheet"), Detail->IsVisible());
    TestTrue(TEXT("Distant sea remains visible"), Distant->IsVisible());
    FJetFlightTestAccess::WaterView(Water, FVector(0, 0, 3000));
    TestTrue(TEXT("Descending restores detailed waves"), Detail->IsVisible());
    TestEqual(TEXT("Altitude changes preserve vertex budget"), Detail->GetProcMeshSection(0)->ProcVertexBuffer.Num(), VertexBudget);
    TestTrue(TEXT("Detail perimeter stays above far sheet to avoid depth fighting"),
        Detail->GetProcMeshSection(0)->ProcVertexBuffer[0].Position.Z > Distant->GetComponentLocation().Z+1.);
    TestEqual(TEXT("Water sheets share the same material"), Detail->GetMaterial(0), Distant->GetMaterial(0));
    TestEqual(TEXT("Low, central water keeps full wave detail"), AJetWaterSurface::WaveDetailWeight(30, 0, 600), 1.f);
    TestEqual(TEXT("Aerial water suppresses subpixel crest patches"), AJetWaterSurface::WaveDetailWeight(1500, 0, 600), 0.f);
    TestEqual(TEXT("Grid edge always meets flat distant sea"), AJetWaterSurface::WaveDetailWeight(30, 1, 600), 0.f);
    TestEqual(TEXT("Low flight keeps original wave resolution"), AJetWaterSurface::GridScaleForAltitude(3000, 700), 1.f);
    TestTrue(TEXT("1.5 km view covers several kilometres of sea"), AJetWaterSurface::GridScaleForAltitude(150000, 700)*700.f*40.f > 600000.f);
    float Previous = 0.f;
    for (int32 Height = 0; Height <= 1000000; Height += 1000)
    {
        const float Scale = AJetWaterSurface::GridScaleForAltitude(Height, 700);
        TestTrue(TEXT("Spacing grows monotonically within fixed cap"), Scale >= Previous && Scale <= 64.f);
        TestTrue(TEXT("Grid spacing stays a power of two"), FMath::IsNearlyEqual(FMath::Log2(Scale), FMath::RoundToFloat(FMath::Log2(Scale))));
        Previous = Scale;
    }
    return true;
}
#endif
