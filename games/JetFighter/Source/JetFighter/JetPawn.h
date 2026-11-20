#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "JetPawn.generated.h"

class UCameraComponent;
class UBoxComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;

/** Normalised pilot commands, shared by human input and flight verification. */
struct FJetControls
{
    float Pitch = 0.f;
    float Roll = 0.f;
    float Yaw = 0.f;
    float Throttle = 0.6f;
    bool bBoost = false;
    bool bBrake = false;
    bool bAim = false;
};

/** Local flight prototype; network racing follows after handling is validated. */
UCLASS(Config=Game)
class AJetPawn : public APawn
{
    GENERATED_BODY()
public:
    AJetPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    float SpeedKmh() const { return Velocity.Size() * 3.6f; }
    FVector GetFlightVelocity() const { return Velocity; }
    FVector GetAimDirection() const { return AimRotation.Vector(); }
    float GetThrottle() const { return ThrottleInput; }
    float GetRespawnSeconds() const { return RespawnSeconds; }
    float GetAirspaceSeconds() const { return AirspaceGraceSeconds - OutsideSeconds; }
    bool IsOutsideAirspace() const { return OutsideSeconds > 0.f; }
    bool IsMouseAim() const { return bMouseAim; }
    bool IsBoosting() const { return bBoosting; }
    bool IsStalling() const { return FMath::Abs(AngleOfAttack) > StallAoA; }
    float GetSurfaceClearance() const;
    bool NeedsPullUp() const;
    int32 GetAirframeIndex() const { return AirframeIndex; }
    bool HasAirframeArt() const { return bHasAirframeArt; }
    void SelectAirframe(int32 Index);
    float LoadG = 1.f;
    bool bAssisted = true;
    bool bCrashed = false;
    UPROPERTY(Config) float Thrust = 14.f;
    UPROPERTY(Config) float BoostThrust = 26.f;
    UPROPERTY(Config) float Drag = 3.5e-4f;
    UPROPERTY(Config) float BrakeDrag = 1.e-3f;
    UPROPERTY(Config) float InducedDrag = 2.5e-4f;
    UPROPERTY(Config) float Lift = 2.1e-3f;
    UPROPERTY(Config) float LiftSlope = 5.73f;
    UPROPERTY(Config) float StallAoA = 15.f;
    UPROPERTY(Config) float SideForce = 1.5e-3f;
    UPROPERTY(Config) float MaxG = 9.f;
    UPROPERTY(Config) float MaxRollRate = 220.f;
    UPROPERTY(Config) float MaxYawRate = 20.f;
    UPROPERTY(Config) float RateResponse = 8.f;
    UPROPERTY(Config) float AoAGain = 6.f;
    UPROPERTY(Config) float CruiseThrottle = 0.6f;
    UPROPERTY(Config) float StepRate = 120.f;
    UPROPERTY(Config) float MouseSensitivity = 0.18f;
    UPROPERTY(Config) float AimBankGain = 2.5f;
    UPROPERTY(Config) float AimPitchGain = 0.6f;
    UPROPERTY(Config) float AimMaxBank = 65.f;
    UPROPERTY(Config) float AirspaceRadiusMetres = 6000.f;
    UPROPERTY(Config) float AirspaceCeilingMetres = 6000.f;
    UPROPERTY(Config) float AirspaceGraceSeconds = 10.f;
    UPROPERTY(Config) float RespawnDelay = 3.f;
private:
    friend struct FJetFlightTestAccess;
    void ResetFlight();
    void Crash();
    void RunFlightTour(float Dt);
    FJetControls ReadControls(APlayerController* PC, float Dt);
    FJetControls AimControls() const;
    void AdvanceFlight(float DeltaSeconds, const FJetControls& Controls);
    void Step(float Dt, float Pitch, float Roll, float Yaw, float Throttle, bool Boost, bool Brake);
    UPROPERTY() TObjectPtr<UBoxComponent> Collision;
    UPROPERTY() TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY() TObjectPtr<UProceduralMeshComponent> FallbackWing;
    FVector Velocity = FVector(150, 0, 0); // metres/second; engine positions are centimetres
    FVector Rates = FVector::ZeroVector;
    double Accumulator = 0.;
    float RespawnSeconds = 0.f;
    float OutsideSeconds = 0.f;
    float AngleOfAttack = 0.f;
    float ThrottleInput = 0.6f;
    FRotator AimRotation = FRotator::ZeroRotator;
    bool bMouseAim = true;
    bool bBoosting = false;
    bool bFlightTour = false;
    bool bTourShotPending = false;
    int32 TourStage = 0;
    float TourClock = 0.f;
    int32 AirframeIndex = 0;
    bool bHasAirframeArt = false;
};
