#include "JetPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "BoatArt.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "OceanWaves.h"
#include "JetGame.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"
#include "JetFighter.h"

AJetPawn::AJetPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("AirframeCollision"));
    SetRootComponent(Collision);
    Collision->SetBoxExtent(FVector(400.f, 450.f, 70.f));
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_Pawn);
    Collision->SetCollisionResponseToAllChannels(ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    Collision->SetGenerateOverlapEvents(false);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PrototypeAirframe"));
    Body->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Mesh.Succeeded()) Body->SetStaticMesh(Mesh.Object);
    Body->SetRelativeScale3D(FVector(8.f, 1.2f, 0.7f));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto* Wing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wing"));
    Wing->SetupAttachment(RootComponent);
    if (Mesh.Succeeded()) Wing->SetStaticMesh(Mesh.Object);
    Wing->SetRelativeScale3D(FVector(2.f, 9.f, 0.15f));
    Wing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
    CameraArm->SetupAttachment(RootComponent);
    CameraArm->TargetArmLength = 1800.f;
    CameraArm->SocketOffset = FVector(0.f, 0.f, 400.f);
    CameraArm->bEnableCameraLag = true;
    CameraArm->CameraLagSpeed = 8.f;
    CameraArm->bEnableCameraRotationLag = true;
    CameraArm->CameraRotationLagSpeed = 10.f;
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm);
    Camera->SetRelativeRotation(FRotator(-5.f, 0.f, 0.f));
    Camera->SetFieldOfView(90.f);
}

void AJetPawn::BeginPlay()
{
    Super::BeginPlay();
    ResetFlight();
    bFlightTour = FParse::Param(FCommandLine::Get(), TEXT("JetFlightTour"));
    if (auto* Paint = Body->CreateDynamicMaterialInstance(0, BoatArt::BasicMaterial()))
        Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 0.28f, 0.38f));
}
void AJetPawn::ResetFlight()
{
    SetActorLocationAndRotation(FVector(0, 0, 150000), FRotator::ZeroRotator);
    Velocity = FVector(150, 0, 0);
    Rates = FVector::ZeroVector;
    Accumulator = 0.;
    bCrashed = false;
    RespawnSeconds = 0.f;
    OutsideSeconds = 0.f;
    LoadG = 1.f;
    AngleOfAttack = 0.f;
    ThrottleInput = CruiseThrottle;
    AimRotation = FRotator::ZeroRotator;
    bBoosting = false;
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    // Discard camera history across a respawn instead of flying the camera
    // through the sea from the wreck back to the new aircraft.
    CameraArm->bEnableCameraLag = false;
    CameraArm->bEnableCameraRotationLag = false;
    CameraArm->TickComponent(0.f, LEVELTICK_All, nullptr);
    CameraArm->bEnableCameraLag = true;
    CameraArm->bEnableCameraRotationLag = true;
}

void AJetPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    auto* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->IsLocalController()) return;
    // A controller can pause after this frame's actor ticks were queued.
    // Honour the menu immediately instead of advancing one more flight step.
    if (const auto* JetPC = Cast<AJetPlayerController>(PC); JetPC && JetPC->IsFlightPaused()) return;
    if (bFlightTour)
    {
        RunFlightTour(DeltaSeconds);
        return;
    }
    if (PC->WasInputKeyJustPressed(EKeys::R) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top)) ResetFlight();
    if (PC->WasInputKeyJustPressed(EKeys::F) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left)) bAssisted = !bAssisted;
    if (PC->WasInputKeyJustPressed(EKeys::M))
    {
        bMouseAim = !bMouseAim;
        AimRotation = Velocity.Rotation();
    }
    AdvanceFlight(DeltaSeconds, ReadControls(PC, DeltaSeconds));
    const float TargetFov = 82.f + FMath::Clamp(SpeedKmh() / 80.f, 0.f, 12.f) + (bBoosting ? 6.f : 0.f);
    Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFov, DeltaSeconds, 3.f));
}

void AJetPawn::RunFlightTour(float Dt)
{
    static const float Durations[] = {6.f, 3.f, 10.f, 0.7f, 4.f};
    static const TCHAR* Names[] = {TEXT("Level"), TEXT("Bank"), TEXT("Climb"), TEXT("Impact"), TEXT("Respawn")};
    if (bTourShotPending)
    {
        bTourShotPending = false;
        ++TourStage;
        TourClock = 0.f;
        if (TourStage == UE_ARRAY_COUNT(Durations))
        {
            UE_LOG(LogJetFighter, Log, TEXT("Jet flight tour finished."));
            Cast<APlayerController>(GetController())->ConsoleCommand(TEXT("quit"));
            return;
        }
        if (TourStage == 1) AimRotation = FRotator(10.f, 60.f, 0.f);
        if (TourStage == 2) AimRotation = FRotator(15.f, 60.f, 0.f);
        if (TourStage == 3)
        {
            SetActorLocationAndRotation(FVector(0, 0, 2000), FRotator(-45.f, 0, 0));
            Velocity = FVector(100, 0, -150);
            Rates = FVector::ZeroVector;
        }
    }
    if (TourStage >= UE_ARRAY_COUNT(Durations)) return;
    TourClock += Dt;
    FJetControls Controls;
    Controls.bAim = TourStage != 3;
    Controls.Throttle = CruiseThrottle;
    AdvanceFlight(Dt, Controls);
    if (TourClock >= Durations[TourStage])
    {
        FScreenshotRequest::RequestScreenshot(FString::Printf(TEXT("JetFlight_%s"), Names[TourStage]), true, false);
        UE_LOG(LogJetFighter, Log, TEXT("Jet flight tour: %s, crashed=%d, speed=%.0f, height=%.0f"),
            Names[TourStage], bCrashed ? 1 : 0, SpeedKmh(), GetActorLocation().Z/100.);
        bTourShotPending = true;
    }
}

FJetControls AJetPawn::ReadControls(APlayerController* PC, float Dt)
{
    auto Key = [PC](const FKey& K) { return PC->IsInputKeyDown(K) ? 1.f : 0.f; };
    auto Stick = [PC](const FKey& K)
    {
        const float V = PC->GetInputAnalogKeyState(K);
        return FMath::Abs(V) <= 0.15f ? 0.f : FMath::Sign(V) * (FMath::Abs(V)-0.15f)/0.85f;
    };
    float MX = 0.f, MY = 0.f;
    PC->GetInputMouseDelta(MX, MY);
    if (bMouseAim && !bCrashed)
    {
        AimRotation.Yaw = FMath::UnwindDegrees(AimRotation.Yaw + MX * MouseSensitivity);
        AimRotation.Pitch = FMath::Clamp(AimRotation.Pitch + MY * MouseSensitivity, -80., 80.);
    }
    FJetControls Controls;
    Controls.Pitch = FMath::Clamp(Key(EKeys::Down)-Key(EKeys::Up) - Stick(EKeys::Gamepad_LeftY), -1.f, 1.f);
    Controls.Roll = FMath::Clamp(Key(EKeys::D)-Key(EKeys::A) + Stick(EKeys::Gamepad_LeftX), -1.f, 1.f);
    Controls.Yaw = FMath::Clamp(Key(EKeys::E)-Key(EKeys::Q) + Stick(EKeys::Gamepad_RightX), -1.f, 1.f);
    if (FMath::Abs(Controls.Pitch)+FMath::Abs(Controls.Roll)+FMath::Abs(Controls.Yaw) > 0.01f)
    {
        // Manual manoeuvres take priority. When released, mouse aim holds
        // the new flight direction instead of turning back to the old one.
        AimRotation = Velocity.Rotation();
    }
    else if (bMouseAim) Controls.bAim = true;
    const float Change = Key(EKeys::W)-Key(EKeys::S)
        + PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis)
        - PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
    ThrottleInput = FMath::Clamp(ThrottleInput + Change * Dt * 0.4f, 0.f, 1.f);
    Controls.Throttle = ThrottleInput;
    Controls.bBoost = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::Gamepad_LeftShoulder);
    Controls.bBrake = PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::Gamepad_RightShoulder);
    return Controls;
}

FJetControls AJetPawn::AimControls() const
{
    FJetControls Controls;
    const FVector Forward = GetActorForwardVector(), Up = GetActorUpVector(), Right = GetActorRightVector();
    const double HeadingError = FMath::FindDeltaAngleRadians(FMath::Atan2(Forward.Y, Forward.X), FMath::DegreesToRadians(AimRotation.Yaw));
    const double Bank = FMath::Atan2(-Right.Z, Up.Z);
    const double MaxBank = FMath::DegreesToRadians(double(AimMaxBank));
    const double DesiredBank = FMath::Clamp(HeadingError * AimBankGain, -MaxBank, MaxBank);
    const double RollRate = FMath::UnwindRadians(DesiredBank-Bank) * 4. - Rates.X * 0.35;
    Controls.Roll = FMath::Clamp(RollRate/FMath::DegreesToRadians(double(MaxRollRate)), -1., 1.);
    const double PathPitch = FMath::Atan2(Velocity.Z, Velocity.Size2D());
    const double PitchError = FMath::DegreesToRadians(AimRotation.Pitch) - PathPitch;
    // Compensate for bank: only the vertical share of lift holds altitude.
    const double Demand = (9.81 * FMath::Cos(PathPitch) + Velocity.Size() * AimPitchGain * PitchError) / FMath::Max(Up.Z, 0.25);
    const double Trim = bAssisted ? 9.81 * FMath::Max(Up.Z, 0.) : 0.;
    Controls.Pitch = FMath::Clamp((Demand-Trim)/(MaxG*9.81), -1., 1.);
    Controls.Yaw = FMath::Clamp(HeadingError * 0.3, -0.3, 0.3);
    return Controls;
}

void AJetPawn::AdvanceFlight(float DeltaSeconds, const FJetControls& Controls)
{
    if (bCrashed)
    {
        RespawnSeconds -= DeltaSeconds;
        if (RespawnSeconds <= 0.f) ResetFlight();
        return;
    }
    bBoosting = Controls.bBoost;
    const double Dt = 1. / FMath::Clamp(StepRate, 30.f, 240.f);
    Accumulator += FMath::Clamp(DeltaSeconds, 0.f, 0.25f);
    while (Accumulator + 1.e-9 >= Dt && !bCrashed)
    {
        // Recompute assistance on the physics clock, so low rendering frame
        // rates cannot change how quickly the autopilot responds.
        const FJetControls Steer = Controls.bAim ? AimControls() : Controls;
        Step(Dt, Steer.Pitch, Steer.Roll, Steer.Yaw, Controls.Throttle, Controls.bBoost, Controls.bBrake);
        Accumulator -= Dt;
        const FVector P = GetActorLocation()/100.;
        if (P.SizeSquared2D() > FMath::Square(double(AirspaceRadiusMetres)) || P.Z > AirspaceCeilingMetres)
            OutsideSeconds += Dt;
        else OutsideSeconds = 0.f;
        if (OutsideSeconds >= AirspaceGraceSeconds) Crash();
    }
}

void AJetPawn::Crash()
{
    bCrashed = true;
    bBoosting = false;
    RespawnSeconds = RespawnDelay;
    Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

float AJetPawn::GetSurfaceClearance() const
{
    const FVector P = GetActorLocation();
    const auto* State = GetWorld()->GetGameState<AJetGameState>();
    const float Now = State ? State->GetServerNow() : GetWorld()->GetTimeSeconds();
    return (P.Z - OceanWaves::GetHeight(FVector2D(P.X, P.Y), Now))/100.f;
}

bool AJetPawn::NeedsPullUp() const
{
    return !bCrashed && Velocity.Z < -5.f && GetSurfaceClearance() < FMath::Max(100., -Velocity.Z*4.);
}

void AJetPawn::Step(float Dt, float Pitch, float Roll, float Yaw, float Throttle, bool Boost, bool Brake)
{
    constexpr float Gravity = 9.81f;
    const FQuat Q = GetActorQuat();
    const FVector Forward = Q.GetForwardVector(), Right = Q.GetRightVector(), Up = Q.GetUpVector();
    const double Speed = FMath::Max(Velocity.Size(), 1.);
    const FVector Vdir = Velocity / Speed;
    const double AoA = FMath::Atan2(-FVector::DotProduct(Velocity, Up), FVector::DotProduct(Velocity, Forward));
    AngleOfAttack = FMath::RadiansToDegrees(AoA);
    const double Beta = FMath::Atan2(FVector::DotProduct(Velocity, Right), FVector::DotProduct(Velocity, Forward));
    double Demand = Pitch * MaxG * Gravity;
    if (bAssisted) Demand += Gravity * FMath::Max(Up.Z, 0.);
    Demand = FMath::Clamp(Demand, -double(MaxG)*Gravity, double(MaxG)*Gravity);
    double TargetAoA = Demand / (Lift * LiftSlope * Speed * Speed);
    const double Stall = FMath::DegreesToRadians(double(StallAoA));
    if (bAssisted) TargetAoA = FMath::Clamp(TargetAoA, -Stall * 0.5, Stall * 0.92);
    const double A = FMath::Abs(AoA), Peak = LiftSlope * Stall;
    const double C = A <= Stall ? LiftSlope * A : (A <= Stall + FMath::DegreesToRadians(10.) ? Peak * (1. - 0.4 * (A-Stall)/FMath::DegreesToRadians(10.)) : 0.6 * Peak);
    const double Cl = AoA < 0 ? -C : C;
    const double LiftAcc = Lift * Cl * Speed * Speed;
    const FVector LiftDir = (Up - Vdir * FVector::DotProduct(Up, Vdir)).GetSafeNormal();
    const double PitchTarget = (LiftAcc - Gravity * LiftDir.Z) / Speed + AoAGain * (TargetAoA-AoA);
    const double Bank = FMath::Atan2(Right.Z, Up.Z);
    double RollTarget = Roll * FMath::DegreesToRadians(double(MaxRollRate));
    if (bAssisted && FMath::Abs(Roll)<0.001 && FMath::Abs(Bank)<FMath::DegreesToRadians(60.)) RollTarget = -1.2 * Bank;
    const double YawTarget = Yaw * FMath::DegreesToRadians(double(MaxYawRate)) + 2. * Beta;
    Rates += (FVector(RollTarget, PitchTarget, YawTarget)-Rates) * FMath::Min(RateResponse*Dt, 1.f);
    const FVector SideDir = (Right - Vdir * FVector::DotProduct(Right, Vdir)).GetSafeNormal();
    const double DragK = Drag + InducedDrag*Cl*Cl + (Brake ? BrakeDrag : 0.f);
    const FVector Aero = LiftDir*LiftAcc - SideDir*(SideForce*Beta*Speed*Speed) - Vdir*(DragK*Speed*Speed);
    LoadG = FVector::DotProduct(Aero, Up)/Gravity;
    Velocity += (Forward*(Boost ? BoostThrust : Thrust*Throttle) + Aero + FVector(0, 0, -Gravity))*Dt;
    const FVector Position = GetActorLocation() + Velocity*(Dt*100.);
    // Explicit body axes match the numerical flight check.
    const FQuat Turn = FQuat(FVector::UpVector, Rates.Z*Dt) * FQuat(FVector::RightVector, -Rates.Y*Dt) * FQuat(FVector::ForwardVector, -Rates.X*Dt);
    FHitResult Hit;
    SetActorLocationAndRotation(Position, (Q*Turn).GetNormalized(), true, &Hit);
    if (Hit.bBlockingHit || GetSurfaceClearance() <= 0.7f) Crash();
}
