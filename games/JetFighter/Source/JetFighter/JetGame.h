#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "JetGame.generated.h"

class AWaterSurface;
class AJetScenery;
class UCameraComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;

/**
 * Standalone JetFighter game mode. Only the MaritimeCore plugin is shared
 * with BoatRace; this project never loads boat game rules or scenery.
 */
UCLASS(Config = Game)
class AJetGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AJetGameMode();
};

/** The jet game's shared state. Builds its own world on every machine. */
UCLASS()
class AJetGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	/** The server's clock in seconds. Double, unlike the boat game's float:
	 *  at jet speeds a millisecond is a quarter of a metre. */
	double GetServerNow() const { return GetServerWorldTimeSeconds(); }

protected:
	virtual void BeginPlay() override;
};

/**
 * The jet airspace: islands, a marked airstrip, drifting puff clouds and sea.
 * Every machine builds the same scenery without replicating static geometry.
 */
UCLASS()
class AJetWorld : public AActor
{
	GENERATED_BODY()

public:
	AJetWorld();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UDirectionalLightComponent> Sun;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USkyAtmosphereComponent> Atmosphere;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UExponentialHeightFogComponent> Haze;

	UPROPERTY()
	TObjectPtr<AWaterSurface> Water;
    UPROPERTY() TObjectPtr<AJetScenery> Scenery;
};

/**
 * The altitude-measurement camera used by -JetCheck. Normal play uses
 * AJetPawn; keeping this separate makes performance measurements repeatable.
 */
UCLASS()
class AJetScoutPawn : public APawn
{
	GENERATED_BODY()

public:
	AJetScoutPawn();

	virtual void Tick(float DeltaSeconds) override;

	float GetSpeedMetres() const { return Velocity.Size() / 100.f; }

	/** -JetCheck: the controller steers the camera to set heights. */
	void SetAutopilot(bool bOn, const FVector& Target, float Yaw);

protected:
	UPROPERTY(VisibleAnywhere, Category = "Jet")
	TObjectPtr<UCameraComponent> Camera;

private:
	FVector Velocity = FVector::ZeroVector;
	FRotator View = FRotator(-5.f, 0.f, 0.f);
	bool bAutopilot = false;
	bool bPlaced = false;
};

/** Standalone pause/quit controls and the altitude survey. */
UCLASS()
class AJetPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
    AJetPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;

	/** Smoothed frames per second, for the HUD and -JetCheck. */
	float GetFps() const { return SmoothedFps; }
    bool IsFlightPaused() const { return bPauseMenuOpen; }
    bool SetFlightPaused(bool bPaused);
    int32 GetNavigationIndex() const { return NavigationIndex; }

private:
	void RunJetCheck(float DeltaTime);
    void RunPauseCheck();

	float SmoothedFps = 60.f;
	bool bJetCheck = false;
	bool bInputReady = false;
    bool bPauseMenuOpen = false;
    bool bPauseCheck = false;
    int32 PauseCheckStage = 0;
    double PauseCheckAt = 0.;
    FVector PauseCheckPosition = FVector::ZeroVector;
    int32 PauseCheckRounds = 0;
    int32 PauseCheckShots = 0;
    float PauseCheckHeat = 0.f;
    int32 NavigationIndex = 0;
	bool bShotPending = false;
	int32 CheckStop = -1;
	float CheckClock = 0.f;
	float CheckFrames = 0.f;
	float CheckSeconds = 0.f;
};

/** Flight instruments, navigation landmarks and pause menu. */
UCLASS()
class AJetHUD : public AHUD
{
	GENERATED_BODY()

public:
    virtual void BeginPlay() override;
	virtual void DrawHUD() override;
private:
    void Label(const FString& Text, float X, float Y, float Size, const FLinearColor& Color, float Align = 0.f);
    void Circle(const FVector2D& Centre, float Radius, const FLinearColor& Color);
    UPROPERTY() TObjectPtr<UFont> FlightFont;
    bool bTechnicalTypeface = false;
};
