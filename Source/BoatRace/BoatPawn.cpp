#include "BoatPawn.h"

#include "BayTerrain.h"
#include "BoatArt.h"
#include "BoatAudio.h"
#include "BoatPrefs.h"
#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerController.h"
#include "BoatRivalController.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"

#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "RaceWeather.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const float NetSendInterval = 1.f / 30.f;
	const float MaxExtrapolationTime = 0.3f;
	/** Stops a bad frame or a glancing blow from turning the boat into a top. */
	const float MaxSpinDegreesPerSecond = 190.f;

	void SetPartColor(UStaticMeshComponent* Part, const FLinearColor& Color)
	{
		if (Part == nullptr)
		{
			return;
		}
		// The engine's basic shapes use BasicShapeMaterial, which exposes "Color".
		if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

ABoatPawn::ABoatPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false); // Movement travels in FBoatNetState instead.

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = CubeFinder.Object;
	UStaticMesh* Cone = ConeFinder.Object;
	UStaticMesh* Cylinder = CylinderFinder.Object;
	UStaticMesh* Sphere = SphereFinder.Object;

	Hull = CreateDefaultSubobject<UBoxComponent>(TEXT("Hull"));
	Hull->InitBoxExtent(FVector(350.f, 120.f, 65.f));
	Hull->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Hull->SetCollisionObjectType(ECC_Pawn);
	Hull->SetCollisionResponseToAllChannels(ECR_Block);
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetCanEverAffectNavigation(false);
	Hull->SetNotifyRigidBodyCollision(true);
	// Our own drag and buoyancy damping do the work; engine damping would
	// fight them. A little angular damping keeps the hull from ringing.
	Hull->SetLinearDamping(0.f);
	Hull->SetAngularDamping(1.1f);
	Hull->SetEnableGravity(true);
	SetRootComponent(Hull);

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Hull);

	auto MakePart = [this](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(VisualRoot);
		Part->SetStaticMesh(Mesh);
		Part->SetRelativeLocation(Location);
		Part->SetRelativeRotation(Rotation);
		Part->SetRelativeScale3D(Scale);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	};

	// Placeholder boat from engine shapes, used until SM_Boat is imported.
	// Each basic shape is 100 uu across before scaling.
	HullMesh = MakePart(TEXT("HullMesh"), Cube, FVector(-110.f, 0.f, -15.f), FRotator::ZeroRotator, FVector(4.8f, 2.2f, 0.7f));
	// Pitch -90 turns the cone's tip from +Z to +X, making a bow.
	BowMesh = MakePart(TEXT("BowMesh"), Cone, FVector(240.f, 0.f, -15.f), FRotator(-90.f, 0.f, 0.f), FVector(0.7f, 2.2f, 2.2f));
	CabinMesh = MakePart(TEXT("CabinMesh"), Cube, FVector(-20.f, 0.f, 50.f), FRotator::ZeroRotator, FVector(1.3f, 1.6f, 0.6f));
	EngineMesh = MakePart(TEXT("EngineMesh"), Cube, FVector(-365.f, 0.f, 5.f), FRotator::ZeroRotator, FVector(0.4f, 0.6f, 0.9f));
	FlagPoleMesh = MakePart(TEXT("FlagPoleMesh"), Cylinder, FVector(-300.f, 0.f, 150.f), FRotator::ZeroRotator, FVector(0.04f, 0.04f, 2.6f));
	FlagMesh = MakePart(TEXT("FlagMesh"), Cube, FVector(-335.f, 0.f, 255.f), FRotator::ZeroRotator, FVector(0.7f, 0.03f, 0.45f));

	CustomMesh = MakePart(TEXT("CustomMesh"), nullptr, FVector::ZeroVector, FRotator::ZeroRotator, FVector::OneVector);
	CustomMesh->SetVisibility(false);

	DamageMesh = MakePart(TEXT("DamageMesh"), Sphere, FVector(-150.f, 0.f, 220.f), FRotator::ZeroRotator, FVector(1.6f));
	DamageMesh->SetVisibility(false);

	FireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FireLight"));
	FireLight->SetupAttachment(VisualRoot);
	FireLight->SetRelativeLocation(FVector(-330.f, 0.f, 160.f));
	FireLight->SetLightColor(FLinearColor(1.f, 0.45f, 0.12f));
	FireLight->SetIntensity(0.f);
	FireLight->SetAttenuationRadius(2400.f);
	FireLight->SetCastShadows(false);
	FireLight->SetVisibility(false);

	Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
	Headlight->SetupAttachment(VisualRoot);
	Headlight->SetRelativeLocation(FVector(330.f, 0.f, 90.f));
	Headlight->SetRelativeRotation(FRotator(-6.f, 0.f, 0.f));
	Headlight->SetIntensity(0.f);
	Headlight->SetAttenuationRadius(9000.f);
	Headlight->SetOuterConeAngle(32.f);
	Headlight->SetInnerConeAngle(18.f);
	Headlight->SetCastShadows(false);
	Headlight->SetVisibility(false);

	// The camera hangs off the hull but is told not to inherit its roll and
	// pitch, so a wave or a barrel roll does not throw the view around.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Hull);
	// Low and close behind, so the water rushes past and the bow fills the view.
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 230.f));
	SpringArm->SetRelativeRotation(FRotator(-9.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 1150.f;
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 7.f;
	SpringArm->bEnableCameraRotationLag = true;
	SpringArm->CameraRotationLagSpeed = 4.f;
	SpringArm->bInheritPitch = false;
	SpringArm->bInheritRoll = false;
	SpringArm->bInheritYaw = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 85.f;
}

void ABoatPawn::BeginPlay()
{
	Super::BeginPlay();

	// --- Derive the physics constants from the configured hull -------------
	HullMassKg = FMath::Max(HullMassKg, 50.f);
	Draft = FMath::Max(Draft, 5.f);
	ReferenceTopSpeed = FMath::Max(ReferenceTopSpeed, 100.f);

	Hull->SetBoxExtent(FVector(HullLength * 0.5f, HullWidth * 0.5f, HullHeight * 0.5f), false);
	Hull->SetMassOverrideInKg(NAME_None, HullMassKg, true);
	// Low and slightly aft: the boat then sits stern-down under power and
	// rights itself instead of wanting to roll over.
	Hull->SetCenterOfMass(FVector(-HullLength * 0.06f, 0.f, -HullHeight * 0.28f));

	const float ProbeX = HullLength * 0.42f;
	const float ProbeY = HullWidth * 0.38f;
	const float ProbeZ = -HullHeight * 0.5f;
	Probes[0] = FVector( ProbeX,  ProbeY, ProbeZ); // bow starboard
	Probes[1] = FVector( ProbeX, -ProbeY, ProbeZ); // bow port
	Probes[2] = FVector(-ProbeX,  ProbeY, ProbeZ); // stern starboard
	Probes[3] = FVector(-ProbeX, -ProbeY, ProbeZ); // stern port

	const float Weight = HullMassKg * OceanWaves::Gravity;
	const float TotalBuoyancy = BuoyancyMultiplier * Weight;
	BuoyancyPerProbe = TotalBuoyancy / 4.f;

	// Treat the hull as a spring of stiffness (buoyancy / draft) and damp it to
	// the configured ratio, so it settles instead of bobbing for ever.
	const float Stiffness = TotalBuoyancy / Draft;
	const float NaturalFrequency = FMath::Sqrt(Stiffness / HullMassKg);
	DampingPerProbe = (2.f * HeaveDampingRatio * HullMassKg * NaturalFrequency) / 4.f;

	// Quadratic drag chosen so full thrust tops out at ReferenceTopSpeed.
	ForwardDragCoefficient = (HullMassKg * ThrustAcceleration) / (ReferenceTopSpeed * ReferenceTopSpeed);

	// Planing lift, set from the fraction of the boat's weight the hull carries
	// on its own bow wave at full speed.
	PlaningLiftCoefficient = (PlaningLiftFraction * Weight) / (ReferenceTopSpeed * ReferenceTopSpeed);

	// Yaw inertia of a box, used by the rudder control loop.
	YawInertia = HullMassKg * (HullLength * HullLength + HullWidth * HullWidth) / 12.f;

	Health = MaxHealth;
	if (HasAuthority())
	{
		TorpedoAmmo = StartTorpedoes;
		MineAmmo = StartMines;
		NetState.Location = GetActorLocation();
		NetState.Rotation = GetActorRotation();
		bHasNetState = true;
	}

	SetPartColor(CabinMesh, FLinearColor(0.9f, 0.9f, 0.9f));
	SetPartColor(EngineMesh, FLinearColor(0.03f, 0.03f, 0.03f));
	SetPartColor(FlagPoleMesh, FLinearColor(0.6f, 0.6f, 0.6f));
	SetPartColor(DamageMesh, FLinearColor(0.06f, 0.06f, 0.07f));

	TryLoadCustomMesh();
	UpdatePhysicsMode();
	Hull->OnComponentHit.AddDynamic(this, &ABoatPawn::OnHullHit);
	BuildEffects();

	UE_LOG(LogBoatRace, Log,
		TEXT("Boat ready: %.0f kg, buoyancy %.0f per probe, heave damping %.0f, drag %.4f, yaw inertia %.3g"),
		HullMassKg, BuoyancyPerProbe, DampingPerProbe, ForwardDragCoefficient, YawInertia);
}

void ABoatPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The owner is simulating its own boat, so it does not need the echo back.
	DOREPLIFETIME_CONDITION(ABoatPawn, NetState, COND_SkipOwner);
	DOREPLIFETIME(ABoatPawn, Health);
	DOREPLIFETIME(ABoatPawn, bWrecked);
	DOREPLIFETIME(ABoatPawn, InvulnerableUntil);
	DOREPLIFETIME_CONDITION(ABoatPawn, TorpedoAmmo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ABoatPawn, MineAmmo, COND_OwnerOnly);
}

float ABoatPawn::GetWaveTime() const
{
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	// The server's clock, so all four players ride the same sea.
	return RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
}

void ABoatPawn::UpdatePhysicsMode()
{
	// Exactly one machine simulates each boat: the one holding its controls.
	// Everywhere else it is a kinematic copy, which still blocks and shoves
	// the locally simulated boats it touches.
	const bool bShouldSimulate = IsLocallyControlled();
	if (bShouldSimulate == bPhysicsActive)
	{
		return;
	}

	bPhysicsActive = bShouldSimulate;
	Hull->SetSimulatePhysics(bShouldSimulate);
	if (!bShouldSimulate)
	{
		Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	Hull->SetPhysicsMaxAngularVelocityInDegrees(MaxSpinDegreesPerSecond, false);
}

bool ABoatPawn::CanDrive() const
{
	if (bWrecked)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const ABoatRacePlayerState* RacePlayer = GetPlayerState<ABoatRacePlayerState>();
	if (RaceState == nullptr)
	{
		return true;
	}
	if (RaceState->RaceState == ERaceState::Countdown)
	{
		return false; // Held on the grid.
	}
	if (RaceState->RaceState == ERaceState::Racing && RacePlayer != nullptr && RacePlayer->bFinished)
	{
		return false; // Coasting after the chequered flag.
	}
	return true;
}

void ABoatPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdatePhysicsMode();
	if (!bColorApplied)
	{
		TryApplyPlayerColor();
	}

	const float WaveTime = GetWaveTime();

	if (IsLocallyControlled())
	{
		ReadInput(DeltaSeconds);
		// The boost tank: fills slowly while racing and quickly in the air off
		// a ramp, so jumps are worth taking; empties while it fires.
		const ABoatRaceGameState* BoostState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
		const bool bRacing = BoostState == nullptr || BoostState->RaceState != ERaceState::Countdown;
		const bool bWasBoosting = bBoostActive;
		bBoostActive = bRacing && CanDrive() && bWantBoost && InputThrottle > 0.f && (bBoostActive ? Boost > 0.f : Boost > 0.15f);
		if (bBoostActive)
		{
			Boost = FMath::Max(Boost - DeltaSeconds / FMath::Max(BoostSeconds, 0.2f), 0.f);
		}
		else if (bRacing)
		{
			Boost = FMath::Min(Boost + DeltaSeconds * (BoostTrickle + (AirTime > 0.2f ? BoostPerAirSecond : 0.f)), 1.f);
		}
		if (bBoostActive && !bWasBoosting)
		{
			BoatAudio::PlayAt(this, EBoatSound::Torpedo, GetActorLocation(), 0.8f, 0.7f);
			Rumble(0.5f, 0.35f);
		}
		SimulatePhysics(DeltaSeconds, WaveTime);
		SendMoveToServer(DeltaSeconds);
	}
	else
	{
		FollowReportedState(DeltaSeconds);
	}

	UpdateCosmetics(DeltaSeconds);
	UpdateEffects(DeltaSeconds);
	UpdateAudio(DeltaSeconds);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void ABoatPawn::ReadInput(float DeltaSeconds)
{
	float Throttle = 0.f;
	float Steer = 0.f;

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const ABoatRacePlayerController* MenuOwner = Cast<ABoatRacePlayerController>(PlayerController);
	if (MenuOwner != nullptr && MenuOwner->IsMenuOpen())
	{
		// The keys are steering the menu, not the boat.
		InputThrottle = 0.f;
		InputSteer = FMath::FInterpTo(InputSteer, 0.f, DeltaSeconds, 8.f);
		return;
	}
	if (PlayerController == nullptr)
	{
		// A computer rival has already set the controls through SetDriverInput.
		if (!CanDrive())
		{
			InputThrottle = 0.f;
			InputSteer = 0.f;
		}
		return;
	}
	if (CanDrive())
	{
		if (PlayerController->IsInputKeyDown(EKeys::W) || PlayerController->IsInputKeyDown(EKeys::Up))
		{
			Throttle += 1.f;
		}
		if (PlayerController->IsInputKeyDown(EKeys::S) || PlayerController->IsInputKeyDown(EKeys::Down))
		{
			Throttle -= 1.f;
		}
		if (PlayerController->IsInputKeyDown(EKeys::D) || PlayerController->IsInputKeyDown(EKeys::Right))
		{
			Steer += 1.f;
		}
		if (PlayerController->IsInputKeyDown(EKeys::A) || PlayerController->IsInputKeyDown(EKeys::Left))
		{
			Steer -= 1.f;
		}

		// Gamepad: triggers for throttle, left stick to steer.
		Throttle += PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
		Throttle -= PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		const float StickX = PlayerController->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		if (FMath::Abs(StickX) > 0.15f)
		{
			Steer += StickX;
		}
	}

	// -PhotoTour drives the player's boat at the next gate, so the screenshots
	// show the boat under way with its wake and spray.
	static const bool bAutopilot = FParse::Param(FCommandLine::Get(), TEXT("PhotoTour")) || FParse::Param(FCommandLine::Get(), TEXT("Autopilot"));
	const ABoatRacePlayerState* Racer = GetPlayerState<ABoatRacePlayerState>();
	if (bAutopilot && CanDrive() && Racer != nullptr && BoatTrack::GetGates().IsValidIndex(Racer->NextGate))
	{
		// The rivals' own line and obstacle avoidance, and their trick of
		// backing out when beached or pinned.
		const float Speed = FMath::Abs(GetForwardSpeed());
		const FVector2D Target = ABoatRivalController::RacingTarget(GetWorld(), this, Racer->NextGate, Speed, 0.f);
		const FVector2D ToTarget = Target - FVector2D(GetActorLocation().X, GetActorLocation().Y);
		const float Error = FMath::FindDeltaAngleDegrees(static_cast<float>(GetActorRotation().Yaw),
			FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X)));
		const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
		AutopilotStuck = Speed < 220.f ? AutopilotStuck + DeltaSeconds : 0.f;
		if (AutopilotStuck > 2.5f)
		{
			AutopilotReverseUntil = Now + 1.4f;
			AutopilotStuck = 0.f;
		}
		const bool bReversing = Now < AutopilotReverseUntil;
		static float NextReport = 0.f;
		if (Now > NextReport && FParse::Param(FCommandLine::Get(), TEXT("AutopilotLog")))
		{
			NextReport = Now + 4.f;
			UE_LOG(LogBoatRace, Log, TEXT("Autopilot: at %.0f %.0f %.0f, %.0f km/h, gate %d, error %.0f, pitch %.0f roll %.0f, hull %.0f, submerged %.2f, rev %d"),
				GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z, Speed * 0.036f, Racer->NextGate + 1, Error,
				GetActorRotation().Pitch, GetActorRotation().Roll, Health, LastSubmersion, bReversing ? 1 : 0);
		}
		Throttle = bReversing ? -1.f : (FMath::Abs(Error) > 55.f ? 0.5f : 0.85f);
		Steer = FMath::Clamp(Error / 28.f, -1.f, 1.f) * (bReversing ? -1.f : 1.f);
	}

	// Boost: hold SHIFT or the right bumper.
	bWantBoost = PlayerController->IsInputKeyDown(EKeys::LeftShift) || PlayerController->IsInputKeyDown(EKeys::RightShift)
		|| PlayerController->IsInputKeyDown(EKeys::Gamepad_RightShoulder);
	// Camera: C cycles the views, B (or the left bumper) looks behind.
	if (PlayerController->WasInputKeyJustPressed(EKeys::C) || PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_RightThumbstick))
	{
		if (UBoatPrefs* Prefs = UBoatPrefs::Get(this))
		{
			Prefs->CycleCamera(1);
		}
	}
	bLookBack = PlayerController->IsInputKeyDown(EKeys::B) || PlayerController->IsInputKeyDown(EKeys::Gamepad_LeftShoulder);
	if (const UBoatPrefs* Prefs = UBoatPrefs::Get(this))
	{
		Steer *= Prefs->SteeringSensitivity;
	}

	InputThrottle = FMath::Clamp(Throttle, -1.f, 1.f);
	// Easing the rudder gives the hull time to lean instead of snapping flat.
	InputSteer = FMath::FInterpTo(InputSteer, FMath::Clamp(Steer, -1.f, 1.f), DeltaSeconds, 8.f);

	// Weapons are server-decided; the key press is only a request.
	if (PlayerController != nullptr && !bWrecked)
	{
		const bool bFirePressed = PlayerController->WasInputKeyJustPressed(EKeys::SpaceBar)
			|| PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton)
			|| PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom);
		const bool bMinePressed = PlayerController->WasInputKeyJustPressed(EKeys::Q)
			|| PlayerController->WasInputKeyJustPressed(EKeys::RightMouseButton)
			|| PlayerController->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left);

		if (bFirePressed)
		{
			ServerFireTorpedo();
		}
		if (bMinePressed)
		{
			ServerDropMine();
		}
	}
}

// ---------------------------------------------------------------------------
// Physics, run only on the machine that owns this boat
// ---------------------------------------------------------------------------
void ABoatPawn::SimulatePhysics(float DeltaSeconds, float WaveTime)
{
	if (!bPhysicsActive || DeltaSeconds <= 0.f)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	if (Location.ContainsNaN())
	{
		// Something went badly wrong in the solver; put the boat back rather
		// than feeding NaN into every force below.
		UE_LOG(LogBoatRace, Warning, TEXT("Boat position became invalid; resetting to the grid."));
		const FTransform Grid = BoatTrack::GetGridTransform(0);
		Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		SetActorLocationAndRotation(Grid.GetLocation(), Grid.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	ApplyBuoyancy(WaveTime);
	ApplyHydrodynamics();
	ApplyEngineAndRudder(DeltaSeconds);
	ApplyWhirlpools();
	KeepInsideBay();
	ApplyWindAndShore();
	LastVelocity = Hull->GetPhysicsLinearVelocity();

	// A capsize should cost time, not end the race, so help the hull back over
	// once it is past the point where buoyancy alone would right it.
	const FVector Up = GetActorUpVector();
	const float TiltDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Up.Z), -1.f, 1.f)));
	if (TiltDegrees > SelfRightAngle)
	{
		// Roll back towards upright. Fully inverted, "towards upright" has no
		// direction (the cross product is zero), so roll about the keel.
		FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
		if (Axis.SizeSquared() < 0.01)
		{
			Axis = GetActorForwardVector();
		}
		const float Strength = (TiltDegrees - SelfRightAngle) / 110.f;
		Hull->AddTorqueInRadians(Axis.GetSafeNormal() * (YawInertia * 2.2f * FMath::Clamp(Strength, 0.f, 1.f)));
	}

	// Capsize recovery. Buoyancy holds an upside-down hull upside down, like a
	// raft, so torque alone cannot always bring it back; after a couple of
	// seconds over, put the boat upright where it is, as arcade racers do.
	CapsizedTime = TiltDegrees > 100.f ? CapsizedTime + DeltaSeconds : 0.f;
	if (CapsizedTime > 2.5f)
	{
		CapsizedTime = 0.f;
		const FVector Velocity = Hull->GetPhysicsLinearVelocity();
		const float Surface = OceanWaves::GetHeight(FVector2D(Location.X, Location.Y), WaveTime);
		SetActorLocationAndRotation(FVector(Location.X, Location.Y, Surface + BoatTrack::BoatRideHeight + 40.f),
			FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Hull->SetPhysicsLinearVelocity(FVector(Velocity.X * 0.3, Velocity.Y * 0.3, 0.0));
		BoatAudio::PlayAt(this, EBoatSound::Splash, GetActorLocation(), 0.8f);
	}
}

void ABoatPawn::ApplyBuoyancy(float WaveTime)
{
	const FTransform HullTransform = Hull->GetComponentTransform();
	float SubmersionSum = 0.f;

	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FVector ProbeWorld = HullTransform.TransformPosition(Probes[Index]);
		const FVector2D ProbeXY(ProbeWorld.X, ProbeWorld.Y);

		FVector2D Slope;
		const float SurfaceZ = OceanWaves::GetHeightAndSlope(ProbeXY, WaveTime, Slope);
		const float Depth = SurfaceZ - static_cast<float>(ProbeWorld.Z);
		const float Submersion = FMath::Clamp(Depth / Draft, 0.f, 1.f);
		SubmersionSum += Submersion;

		if (Submersion <= 0.f)
		{
			continue; // This corner is in the air.
		}

		// Up-thrust in proportion to depth. Four separate probes is what turns
		// a flat push into pitch and roll.
		FVector Force = FVector::UpVector * (BuoyancyPerProbe * Submersion);

		// Damping against the water's own vertical motion, not against the
		// world, so the boat rides a swell instead of being pinned through it.
		const FVector ProbeVelocity = Hull->GetPhysicsLinearVelocityAtPoint(ProbeWorld);
		const float WaterRise = OceanWaves::GetVerticalSpeed(ProbeXY, WaveTime);
		const float RelativeVertical = static_cast<float>(ProbeVelocity.Z) - WaterRise;
		Force -= FVector::UpVector * (RelativeVertical * DampingPerProbe * Submersion);

		// The component of gravity along the wave face: this is what slides a
		// boat sideways off a crest and makes a steep sea hard to hold a line in.
		Force += FVector(-Slope.X, -Slope.Y, 0.0) * (HullMassKg * OceanWaves::Gravity * 0.25f * WaveSlopeFactor * Submersion);

		if (!Force.ContainsNaN())
		{
			Hull->AddForceAtLocation(Force, ProbeWorld);
		}
	}

	LastSubmersion = SubmersionSum / 4.f;
}

void ABoatPawn::ApplyHydrodynamics()
{
	// A floating hull only sinks far enough for buoyancy to match its weight,
	// so the probes read well under 1 even when it is sitting perfectly
	// normally, and a planing hull reads less again. Rescale so that anything
	// still touching the water gets full resistance, and only a boat genuinely
	// in the air loses it.
	const float WaterContact = FMath::Clamp(LastSubmersion * BuoyancyMultiplier * 2.5f, 0.f, 1.f);
	if (WaterContact <= 0.f)
	{
		return; // Airborne: nothing to push against.
	}

	// Water resistance acts horizontally. Using the hull's own tilted axes
	// lets a heeled boat drag itself under the surface.
	FVector ForwardFlat = GetActorForwardVector();
	ForwardFlat.Z = 0.0;
	ForwardFlat = ForwardFlat.GetSafeNormal();
	if (ForwardFlat.IsNearlyZero())
	{
		return; // Bow pointing at the sky: nothing sensible to work from.
	}
	const FVector RightFlat(-ForwardFlat.Y, ForwardFlat.X, 0.0);

	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const float Along = static_cast<float>(Velocity.X * ForwardFlat.X + Velocity.Y * ForwardFlat.Y);
	const float Across = static_cast<float>(Velocity.X * RightFlat.X + Velocity.Y * RightFlat.Y);

	// Quadratic drag, as water resistance actually behaves. Sideways is far
	// stiffer than forwards: that is what gives a hull its grip, while still
	// letting it break away into a slide when pushed too hard into a turn.
	const float DragAlong = -FMath::Sign(Along) * ForwardDragCoefficient * Along * Along;
	const float DragAcross = -FMath::Sign(Across) * ForwardDragCoefficient * LateralDragRatio * Across * Across;

	FVector Drag = (ForwardFlat * DragAlong + RightFlat * DragAcross) * WaterContact;
	if (bWrecked)
	{
		Drag *= 2.5f; // A wreck slows quickly.
	}
	if (!Drag.ContainsNaN())
	{
		// Through the centre of mass. Applying it lower looks tempting, but in
		// a hard turn the sideways force is several times the boat's weight,
		// and any lever at all rolls it straight over.
		Hull->AddForce(Drag);
	}

	// Planing: the hull climbs onto its own bow wave, lifting the nose and
	// riding higher the faster it goes.
	if (Along > 0.f)
	{
		const float Lift = FMath::Min(PlaningLiftCoefficient * Along * Along * WaterContact,
			0.8f * HullMassKg * OceanWaves::Gravity);
		const FVector LiftPoint = Hull->GetComponentTransform().TransformPosition(
			FVector(HullLength * 0.15f, 0.f, -HullHeight * 0.28f));
		if (FMath::IsFinite(Lift))
		{
			Hull->AddForceAtLocation(FVector(0.f, 0.f, Lift), LiftPoint);
		}
	}
}

void ABoatPawn::ApplyEngineAndRudder(float DeltaSeconds)
{
	const FTransform HullTransform = Hull->GetComponentTransform();

	// The propeller hangs below the keel. Measured down here, applied higher
	// up: pushing from this low would drive the bow straight under.
	const FVector PropellerWorld = HullTransform.TransformPosition(
		FVector(-HullLength * 0.46f, 0.f, -HullHeight * 0.5f - PropellerDepth));
	const float WaveTime = GetWaveTime();
	const float PropSurface = OceanWaves::GetHeight(FVector2D(PropellerWorld.X, PropellerWorld.Y), WaveTime);
	// Out of the water over a jump or a crest, and the engine loses its bite.
	const float PropBite = FMath::Clamp((PropSurface - static_cast<float>(PropellerWorld.Z)) / Draft, 0.f, 1.f);

	if (bWrecked)
	{
		return; // Engine dead and rudder useless.
	}

	if (InputThrottle != 0.f && PropBite > 0.f)
	{
		float Power = (InputThrottle >= 0.f) ? InputThrottle : InputThrottle * ReverseFraction;
		if (bBoostActive && InputThrottle > 0.f)
		{
			Power *= BoostThrust;
		}
		const FVector Thrust = GetActorForwardVector() * (Power * ThrustAcceleration * HullMassKg * PropBite);
		// At the transom but level with the centre of mass, so the engine
		// drives the boat forward without trimming it nose-down.
		const FVector ThrustPoint = HullTransform.TransformPosition(
			FVector(-HullLength * 0.46f, 0.f, -HullHeight * 0.28f));
		if (!Thrust.ContainsNaN())
		{
			Hull->AddForceAtLocation(Thrust, ThrustPoint);
		}
	}

	// --- Rudder -----------------------------------------------------------
	FVector ForwardFlat = GetActorForwardVector();
	ForwardFlat.Z = 0.0;
	ForwardFlat = ForwardFlat.GetSafeNormal();
	if (ForwardFlat.IsNearlyZero())
	{
		return;
	}

	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const float Along = static_cast<float>(Velocity.X * ForwardFlat.X + Velocity.Y * ForwardFlat.Y);
	// Water has to be flowing past the blade, and it works the other way in reverse.
	const float Flow = FMath::Clamp(FMath::Abs(Along) / 600.f, 0.f, 1.f) * FMath::Sign(Along);
	const float SpeedFraction = FMath::Clamp(FMath::Abs(Along) / ReferenceTopSpeed, 0.f, 1.f);
	// Less authority flat out, so top speed asks for a steadier hand.
	const float DesiredYawRate = InputSteer * TurnRate * Flow * (1.f - 0.35f * SpeedFraction) * PropBite;

	const float CurrentYawRate = static_cast<float>(Hull->GetPhysicsAngularVelocityInDegrees().Z);
	const float Torque = FMath::DegreesToRadians(DesiredYawRate - CurrentYawRate) * YawInertia * TurnResponse;
	if (FMath::IsFinite(Torque))
	{
		Hull->AddTorqueInRadians(FVector(0.f, 0.f, Torque));
	}

	// --- Lean into the turn ----------------------------------------------
	// Steered towards an angle rather than pushed by a raw torque: a torque
	// keeps accelerating the roll whenever the hull leaves the water, and the
	// boat ends up upside down.
	const float WaterContact = FMath::Clamp(LastSubmersion * BuoyancyMultiplier * 2.5f, 0.f, 1.f);
	const float UpZ = static_cast<float>(GetActorUpVector().Z);
	// The lean is for small angles; past 60 degrees it would fight the righting.
	if (WaterContact > 0.f && UpZ > 0.5f)
	{
		const float CurrentRoll = FMath::DegreesToRadians(static_cast<float>(GetActorRotation().Roll));
		const float TargetRoll = FMath::DegreesToRadians(-InputSteer * MaxHeelDegrees * SpeedFraction);
		const FVector AngularVelocity = Hull->GetPhysicsAngularVelocityInRadians();
		const float RollRate = static_cast<float>(FVector::DotProduct(AngularVelocity, ForwardFlat));

		const float RollInertia = HullMassKg * (HullWidth * HullWidth + HullHeight * HullHeight) / 12.f;
		float Heel = ((TargetRoll - CurrentRoll) * HeelResponse - RollRate * 2.f) * RollInertia * WaterContact;
		Heel = FMath::Clamp(Heel, -RollInertia * 12.f, RollInertia * 12.f);
		if (FMath::IsFinite(Heel))
		{
			Hull->AddTorqueInRadians(ForwardFlat * Heel);
		}
	}
}

void ABoatPawn::ApplyWhirlpools()
{
	const FVector Location = GetActorLocation();
	const FVector2D Position(Location.X, Location.Y);

	for (const FBoatWhirlpool& Pool : BoatTrack::GetWhirlpools())
	{
		const FVector2D ToCentre = Pool.Center - Position;
		const float Distance = static_cast<float>(ToCentre.Size());
		if (Distance > Pool.Radius || Distance < 1.f)
		{
			continue;
		}

		// Strongest at the rim and easing off towards the eye, so it drags you
		// round rather than pinning you in the middle.
		const float Strength = FMath::Sin(PI * FMath::Clamp(Distance / Pool.Radius, 0.f, 1.f));
		const FVector2D Inward = ToCentre / Distance;
		const FVector2D Swirl(-Inward.Y, Inward.X);
		const FVector2D Pull = (Inward * 0.45 + Swirl)
			* static_cast<double>(HullMassKg * 420.f * Strength
				* FMath::Clamp(LastSubmersion * BuoyancyMultiplier * 2.5f, 0.f, 1.f));

		const FVector Force(Pull.X, Pull.Y, 0.0);
		if (!Force.ContainsNaN())
		{
			Hull->AddForce(Force);
		}
	}
}

void ABoatPawn::KeepInsideBay()
{
	const FVector Location = GetActorLocation();
	FVector Push = FVector::ZeroVector;

	// A firm shove rather than a wall, so hitting the edge of the bay at speed
	// does not stop the boat dead.
	const float Margin = 2000.f;
	const float Stiffness = HullMassKg * 900.f;
	if (Location.X > BoatTrack::BoundsX - Margin)
	{
		Push.X -= (Location.X - (BoatTrack::BoundsX - Margin)) / Margin * Stiffness;
	}
	else if (Location.X < -BoatTrack::BoundsX + Margin)
	{
		Push.X += ((-BoatTrack::BoundsX + Margin) - Location.X) / Margin * Stiffness;
	}
	if (Location.Y > BoatTrack::BoundsY - Margin)
	{
		Push.Y -= (Location.Y - (BoatTrack::BoundsY - Margin)) / Margin * Stiffness;
	}
	else if (Location.Y < -BoatTrack::BoundsY + Margin)
	{
		Push.Y += ((-BoatTrack::BoundsY + Margin) - Location.Y) / Margin * Stiffness;
	}

	if (!Push.IsNearlyZero() && !Push.ContainsNaN())
	{
		Hull->AddForce(Push);
	}
}

// ---------------------------------------------------------------------------
// Networking
// ---------------------------------------------------------------------------
void ABoatPawn::SendMoveToServer(float DeltaSeconds)
{
	SendAccumulator += DeltaSeconds;
	if (SendAccumulator < NetSendInterval)
	{
		return;
	}
	SendAccumulator = 0.f;

	FBoatNetState Move;
	Move.Location = GetActorLocation();
	Move.Rotation = GetActorRotation();
	Move.Velocity = Hull->GetPhysicsLinearVelocity();
	Move.Steer = InputSteer;
	Move.Throttle = InputThrottle;
	Move.TeleportCounter = LocalTeleportSeq;
	Move.bBoosting = bBoostActive;

	if (HasAuthority())
	{
		// The host's own boat: publish directly, keeping the teleport counter.
		const uint8 Counter = NetState.TeleportCounter;
		NetState = Move;
		NetState.TeleportCounter = Counter;
	}
	else
	{
		ServerSendMove(Move);
	}
}

void ABoatPawn::ServerSendMove_Implementation(FBoatNetState Move)
{
	// (bBoosting is cosmetic for everyone else; the owner applies the thrust.)
	// Ignore anything sent before the client heard about the latest teleport,
	// otherwise a boat snaps back to where it was when it was moved.
	if (Move.TeleportCounter != NetState.TeleportCounter)
	{
		return;
	}
	if (Move.Location.ContainsNaN() || Move.Velocity.ContainsNaN() || Move.Rotation.ContainsNaN())
	{
		return;
	}

	const double LimitX = static_cast<double>(BoatTrack::BoundsX);
	const double LimitY = static_cast<double>(BoatTrack::BoundsY);
	Move.Location.X = FMath::Clamp(Move.Location.X, -LimitX, LimitX);
	Move.Location.Y = FMath::Clamp(Move.Location.Y, -LimitY, LimitY);
	Move.Location.Z = FMath::Clamp(Move.Location.Z, -2000.0, 20000.0);
	Move.Velocity = Move.Velocity.GetClampedToMaxSize(ReferenceTopSpeed * 2.5f);
	Move.Steer = FMath::Clamp(Move.Steer, -1.f, 1.f);
	Move.Throttle = FMath::Clamp(Move.Throttle, -1.f, 1.f);

	NetState = Move;
	bHasNetState = true;
	TimeSinceNetUpdate = 0.f;

	// Keep the server's kinematic copy where the owner says it is, so the host
	// sees collisions and the lap counter reads a real position.
	if (!IsLocallyControlled())
	{
		SetActorLocationAndRotation(NetState.Location, NetState.Rotation, false, nullptr, ETeleportType::None);
	}
}

void ABoatPawn::OnRep_NetState()
{
	bHasNetState = true;
	TimeSinceNetUpdate = 0.f;

	if (NetState.TeleportCounter != LastSeenTeleportCounter)
	{
		LastSeenTeleportCounter = NetState.TeleportCounter;
		SetActorLocationAndRotation(NetState.Location, NetState.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ABoatPawn::FollowReportedState(float DeltaSeconds)
{
	if (!bHasNetState)
	{
		return;
	}

	TimeSinceNetUpdate += DeltaSeconds;

	// Dead reckoning: carry on at the last reported velocity, capped so a
	// dropped connection does not send the boat into the distance.
	const float Lead = FMath::Min(TimeSinceNetUpdate, MaxExtrapolationTime);
	const FVector Target = NetState.Location + NetState.Velocity * Lead;

	const FVector NewLocation = FMath::VInterpTo(GetActorLocation(), Target, DeltaSeconds, 12.f);
	const FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), NetState.Rotation, DeltaSeconds, 10.f);
	// Teleport type None gives the kinematic body a velocity, which is what
	// lets it shove the locally simulated boats it runs into.
	SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);
}

// ---------------------------------------------------------------------------
// Server-driven repositioning
// ---------------------------------------------------------------------------
void ABoatPawn::TeleportBoat(const FVector& Location, float Yaw)
{
	if (!HasAuthority())
	{
		return;
	}

	NetState.TeleportCounter++;
	NetState.Location = Location;
	NetState.Rotation = FRotator(0.f, Yaw, 0.f);
	NetState.Velocity = FVector::ZeroVector;
	NetState.Steer = 0.f;
	NetState.Throttle = 0.f;
	bHasNetState = true;
	TimeSinceNetUpdate = 0.f;
	bRaceHasLastLocation = false;

	if (bPhysicsActive)
	{
		Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);

	// Runs on the owning player's machine, immediately if that is the host.
	ClientTeleport(Location, Yaw, NetState.TeleportCounter);
}

void ABoatPawn::ClientTeleport_Implementation(FVector Location, float Yaw, uint8 TeleportSeq)
{
	LocalTeleportSeq = TeleportSeq;
	LastSeenTeleportCounter = TeleportSeq;
	InputThrottle = 0.f;
	InputSteer = 0.f;
	SendAccumulator = 0.f;

	if (bPhysicsActive)
	{
		Hull->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Hull->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

// ---------------------------------------------------------------------------
// Damage and weapons
// ---------------------------------------------------------------------------
bool ABoatPawn::IsInvulnerable() const
{
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const float Now = RaceState ? RaceState->GetServerNow() : 0.f;
	return Now < InvulnerableUntil;
}

bool ABoatPawn::ApplyDamage(float Amount, ABoatRacePlayerState* Attacker)
{
	if (!HasAuthority() || bWrecked || Amount <= 0.f || IsInvulnerable())
	{
		return false;
	}

	Health = FMath::Max(Health - Amount, 0.f);
	ClientHitFeedback();

	if (Health <= 0.f)
	{
		bWrecked = true;
		if (ABoatRacePlayerState* Victim = GetPlayerState<ABoatRacePlayerState>())
		{
			Victim->Wrecks++;
		}
		if (Attacker != nullptr && Attacker != GetPlayerState<ABoatRacePlayerState>())
		{
			Attacker->Hits++;
		}
		return true;
	}
	return false;
}

void ABoatPawn::ApplyExplosionImpulse(const FVector& Origin, float Strength)
{
	FVector Away = GetActorLocation() - Origin;
	Away.Z = FMath::Abs(Away.Z) + 150.0; // Always a little lift, so hits throw the boat up.
	const FVector Impulse = Away.GetSafeNormal() * Strength;

	if (IsLocallyControlled())
	{
		if (bPhysicsActive && !Impulse.ContainsNaN())
		{
			Hull->AddImpulse(Impulse);
		}
	}
	else if (HasAuthority())
	{
		// Only the owning machine is simulating this boat, so the shove has to
		// be applied there.
		ClientImpulse(Impulse);
	}
}

void ABoatPawn::ClientImpulse_Implementation(FVector Impulse)
{
	if (bPhysicsActive && !Impulse.ContainsNaN())
	{
		Hull->AddImpulse(Impulse);
	}
}

void ABoatPawn::ClientHitFeedback_Implementation()
{
	const UWorld* World = GetWorld();
	LastHitWallClock = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
	CameraShake = FMath::Max(CameraShake, 0.85f);
	Rumble(0.9f, 0.5f);
}

float ABoatPawn::GetTimeSinceHit() const
{
	const UWorld* World = GetWorld();
	const float Now = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
	return Now - LastHitWallClock;
}

void ABoatPawn::Revive(float InvulnerableSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	Health = MaxHealth;
	bWrecked = false;
	Boost = FMath::Max(Boost, 0.3f);

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	InvulnerableUntil = (RaceState ? RaceState->GetServerNow() : 0.f) + InvulnerableSeconds;
}

void ABoatPawn::GrantShield(float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	const ABoatRaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	const float Now = RaceState ? RaceState->GetServerNow() : 0.f;
	InvulnerableUntil = FMath::Max(InvulnerableUntil, Now + Seconds);
}

void ABoatPawn::ClientPickup_Implementation(uint8 Kind, uint8 Torpedoes, uint8 Mines)
{
	PickupAge = 0.f;
	PickupMessage = Kind == 1 ? FString(TEXT("SHIELD  8 s"))
		: FString::Printf(TEXT("+%d TORPEDOES   +%d MINES"), Torpedoes, Mines);
}

void ABoatPawn::GiveAmmo(int32 Torpedoes, int32 Mines)
{
	if (!HasAuthority())
	{
		return;
	}
	TorpedoAmmo = FMath::Clamp(TorpedoAmmo + Torpedoes, 0, MaxTorpedoes);
	MineAmmo = FMath::Clamp(MineAmmo + Mines, 0, MaxMines);
}

void ABoatPawn::ServerFireTorpedo_Implementation()
{
	UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (World == nullptr || bWrecked || TorpedoAmmo <= 0)
	{
		return;
	}

	const float Now = RaceState ? RaceState->GetServerNow() : 0.f;
	if (Now - LastFireTime < FireCooldownSeconds)
	{
		return;
	}
	LastFireTime = Now;
	TorpedoAmmo--;

	// Launched from in front of the bow so it cannot hit the boat that fired it.
	const FVector Forward = GetActorForwardVector();
	const FVector Muzzle = GetActorLocation() + Forward * (HullLength * 0.75f) + FVector(0.f, 0.f, 20.f);
	const FRotator Aim(0.f, GetActorRotation().Yaw, 0.f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABoatTorpedo* Torpedo = World->SpawnActor<ABoatTorpedo>(ABoatTorpedo::StaticClass(), Muzzle, Aim, Params))
	{
		Torpedo->InitProjectile(this);
	}
}

void ABoatPawn::ServerDropMine_Implementation()
{
	UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (World == nullptr || bWrecked || MineAmmo <= 0)
	{
		return;
	}

	const float Now = RaceState ? RaceState->GetServerNow() : 0.f;
	if (Now - LastFireTime < FireCooldownSeconds)
	{
		return;
	}
	LastFireTime = Now;
	MineAmmo--;

	const FVector Drop = GetActorLocation() - GetActorForwardVector() * (HullLength * 0.9f);

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABoatMine* Mine = World->SpawnActor<ABoatMine>(ABoatMine::StaticClass(), Drop, FRotator::ZeroRotator, Params))
	{
		Mine->InitProjectile(this);
	}
}

// ---------------------------------------------------------------------------
// Readouts and cosmetics
// ---------------------------------------------------------------------------
FVector ABoatPawn::GetRaceLocation() const
{
	return IsLocallyControlled() ? GetActorLocation() : NetState.Location;
}

float ABoatPawn::GetForwardSpeed() const
{
	const FVector Velocity = bPhysicsActive ? Hull->GetPhysicsLinearVelocity() : NetState.Velocity;
	return static_cast<float>(FVector::DotProduct(Velocity, GetActorForwardVector()));
}

float ABoatPawn::GetSpeedKmh() const
{
	return FMath::Abs(GetForwardSpeed()) * 0.036f; // cm/s -> km/h
}

void ABoatPawn::UpdateCosmetics(float DeltaSeconds)
{
	// Wrecked boats smoke and sit low; the physics does the rest of the motion.
	DamageMesh->SetVisibility(bWrecked);
	if (bWrecked)
	{
		const float Pulse = 1.4f + 0.5f * FMath::Sin(static_cast<float>(GetWorld()->GetTimeSeconds()) * 3.f);
		DamageMesh->SetRelativeScale3D(FVector(Pulse));
	}

	if (IsLocallyControlled())
	{
		// A touch more field of view with speed, and the camera pulls back.
		const float SpeedFraction = FMath::Clamp(FMath::Abs(GetForwardSpeed()) / ReferenceTopSpeed, 0.f, 1.f);
		const UBoatPrefs* Prefs = UBoatPrefs::Get(this);
		const EBoatCamera View = Prefs ? Prefs->GetCamera() : EBoatCamera::Chase;
		static const bool bTour = FParse::Param(FCommandLine::Get(), TEXT("PhotoTour"));
		const float Boosting = bBoostActive ? 1.f : 0.f;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, 85.f + 13.f * SpeedFraction + 9.f * Boosting, DeltaSeconds, 2.5f));
		float Arm = 1150.f + 250.f * SpeedFraction + 200.f * Boosting;
		FVector Mount(0.f, 0.f, 230.f);
		float Pitch = -9.f;
		if (View == EBoatCamera::Far)
		{
			Arm = 1900.f + 300.f * SpeedFraction;
			Mount.Z = 420.f;
			Pitch = -13.f;
		}
		else if (View == EBoatCamera::Bumper)
		{
			// Low on the foredeck, looking over the bow.
			Arm = 0.f;
			Mount = FVector(120.f, 0.f, 150.f);
			Pitch = -3.f;
		}
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, Arm, DeltaSeconds, 3.f);
		SpringArm->SetRelativeLocation(FMath::VInterpTo(SpringArm->GetRelativeLocation(), Mount, DeltaSeconds, 4.f));
		if (!bTour)
		{
			SpringArm->SetRelativeRotation(FRotator(Pitch, bLookBack ? 180.f : 0.f, 0.f));
			SpringArm->bEnableCameraRotationLag = !bLookBack && View != EBoatCamera::Bumper;
		}

		// A jolt for crashes, landings and nearby blasts, plus a faint buzz at speed.
		CameraShake = FMath::Max(CameraShake - DeltaSeconds * 1.8f, 0.f);
		const float Shake = CameraShake * CameraShake * 60.f + SpeedFraction * 2.f;
		Camera->SetRelativeLocation(FVector(0.f, FMath::FRandRange(-Shake, Shake), FMath::FRandRange(-Shake, Shake)));
	}
}

void ABoatPawn::TryApplyPlayerColor()
{
	const ABoatRacePlayerState* RacePlayer = GetPlayerState<ABoatRacePlayerState>();
	if (RacePlayer == nullptr || RacePlayer->SlotIndex < 0)
	{
		return; // Not replicated yet; Tick will try again.
	}

	const FLinearColor Color = BoatTrack::GetPlayerColor(RacePlayer->SlotIndex);
	SetPartColor(FlagMesh, Color);
	if (!bUsingCustomMesh)
	{
		SetPartColor(HullMesh, Color);
		SetPartColor(BowMesh, Color);
	}
	else
	{
		// The Blender boat's topsides, stripes and roll cage are its Paint slots.
		BoatArt::FLook Look;
		Look.Paint = Color;
		BoatArt::Paint(CustomMesh, Look);
	}
	bColorApplied = true;
}

void ABoatPawn::TryLoadCustomMesh()
{
	// The Blender race boat by default; the Higgsfield one if asked for or if
	// the Blender models have not been imported; the placeholder otherwise.
	UStaticMesh* Model = BoatModel.Equals(TEXT("Higgsfield"), ESearchCase::IgnoreCase) ? nullptr : BoatArt::Model(TEXT("RaceBoat"));
	const bool bBlenderBoat = Model != nullptr;
	if (Model == nullptr)
	{
		Model = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Higgsfield/SM_Boat.SM_Boat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Model == nullptr)
	{
		return; // No model imported: keep the placeholder boat.
	}

	const FBoxSphereBounds Bounds = Model->GetBounds();
	const float SizeX = static_cast<float>(Bounds.BoxExtent.X) * 2.f;
	const float SizeY = static_cast<float>(Bounds.BoxExtent.Y) * 2.f;
	const float Longest = FMath::Max(SizeX, SizeY);
	if (Longest < 1.f)
	{
		UE_LOG(LogBoatRace, Warning, TEXT("SM_Boat has no size; keeping the placeholder boat."));
		return;
	}

	// A generated model arrives at an arbitrary scale and heading, so scale it
	// to the hull's real length and lay its long axis along the direction of travel.
	// The Blender boat is 7.7 m over the outboard, on a 7 m hull.
	const float Scale = (HullLength / Longest) * (bBlenderBoat ? 1.1f : 1.f) * FMath::Max(CustomMeshScaleMultiplier, 0.01f);
	float Yaw = (SizeY > SizeX ? 90.f : 0.f) + CustomMeshYawOffset;
	if (bBlenderBoat && SizeX >= SizeY && Bounds.Origin.X > 0.0)
	{
		// The outboard hangs off the stern, so the bounds sit aft of the
		// middle. If they sit forward, the importer turned the boat round.
		Yaw += 180.f;
	}
	const FRotator Rotation(0.f, Yaw, 0.f);

	const FVector ScaledOrigin = Bounds.Origin * static_cast<double>(Scale);
	const FVector Centering = -Rotation.RotateVector(FVector(ScaledOrigin.X, ScaledOrigin.Y, 0.0));
	const float ModelBottom = static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
	const float OffsetZ = -HullHeight * 0.5f - ModelBottom + CustomMeshZOffset;

	CustomMesh->SetStaticMesh(Model);
	CustomMesh->SetRelativeScale3D(FVector(Scale));
	CustomMesh->SetRelativeRotation(Rotation);
	CustomMesh->SetRelativeLocation(FVector(Centering.X, Centering.Y, static_cast<double>(OffsetZ)));
	CustomMesh->SetVisibility(true);

	HullMesh->SetVisibility(false);
	BowMesh->SetVisibility(false);
	CabinMesh->SetVisibility(false);
	EngineMesh->SetVisibility(false);
	if (bBlenderBoat)
	{
		// The real boat carries its colour in its livery; no placeholder flag.
		FlagPoleMesh->SetVisibility(false);
		FlagMesh->SetVisibility(false);
	}
	bUsingCustomMesh = true;

	UE_LOG(LogBoatRace, Log, TEXT("Using the %s boat model (scale %.3f, yaw %.0f, %.0f cm long)."),
		bBlenderBoat ? TEXT("Blender") : TEXT("Higgsfield"), Scale, Yaw, Longest * Scale);
}

// ---------------------------------------------------------------------------
// Computer drivers
// ---------------------------------------------------------------------------
void ABoatPawn::Rumble(float Intensity, float Seconds)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	const UBoatPrefs* Prefs = UBoatPrefs::Get(this);
	if (PlayerController == nullptr || !PlayerController->IsLocalController() || (Prefs && !Prefs->bRumble))
	{
		return;
	}
	PlayerController->PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Seconds, true, true, true, true);
}

void ABoatPawn::SetShowcaseCamera(bool bShowcase)
{
	// A three-quarter view from ahead and to the side, for the photo tour.
	SpringArm->bInheritYaw = true;
	SpringArm->SetRelativeRotation(bShowcase ? FRotator(-6.f, 145.f, 0.f) : FRotator(-9.f, 0.f, 0.f));
	SpringArm->bEnableCameraRotationLag = !bShowcase;
}

void ABoatPawn::SetDriverInput(float Throttle, float Steer)
{
	InputThrottle = FMath::Clamp(Throttle, -1.f, 1.f);
	const UWorld* World = GetWorld();
	const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.016f;
	InputSteer = FMath::FInterpTo(InputSteer, FMath::Clamp(Steer, -1.f, 1.f), DeltaSeconds, 8.f);
}

void ABoatPawn::RequestFire(bool bMine)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bMine)
	{
		ServerDropMine_Implementation();
	}
	else
	{
		ServerFireTorpedo_Implementation();
	}
}

// ---------------------------------------------------------------------------
// Wind and the shore
// ---------------------------------------------------------------------------
void ABoatPawn::ApplyWindAndShore()
{
	const FVector Location = GetActorLocation();
	const FVector2D Position(Location.X, Location.Y);

	// Wind: a steady shove on the hull and windscreen, stronger in the gusts.
	// Every machine works out the same wind from the shared clock and weather.
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	if (RaceState != nullptr)
	{
		const FVector2D Wind = RaceWeather::GetWind(RaceState->GetWeather(), RaceState->GetServerNow());
		const FVector Velocity = Hull->GetPhysicsLinearVelocity();
		const FVector2D Relative = Wind - FVector2D(Velocity.X, Velocity.Y);
		// Only the part of the wind faster than the boat pushes it; a light
		// boat in a 14 m/s gust is pushed at about a tenth of a g.
		const FVector Push(Relative.X * 0.06, Relative.Y * 0.06, 0.0);
		if (!Push.ContainsNaN() && Wind.SizeSquared() > 1.0)
		{
			Hull->AddForce(Push * (HullMassKg * 0.5f));
		}
	}

	// The shore: the land mesh is solid, but a boat that runs up a beach is
	// helped back off it rather than left stranded.
	const float Ground = BayTerrain::GetHeight(Position);
	if (Ground > -140.f)
	{
		const float Depth = FMath::Clamp((Ground + 140.f) / 200.f, 0.f, 2.5f);
		const FVector2D Seaward = BayTerrain::GetSeaward(Position);
		const FVector Velocity = Hull->GetPhysicsLinearVelocity();
		FVector Force(Seaward.X * HullMassKg * 650.f * Depth, Seaward.Y * HullMassKg * 650.f * Depth, 0.0);
		// Grinding on sand slows the boat down too.
		Force -= FVector(Velocity.X, Velocity.Y, 0.0) * (HullMassKg * 0.9f * FMath::Min(Depth, 1.f));
		if (!Force.ContainsNaN())
		{
			Hull->AddForce(Force);
		}
	}
}

// ---------------------------------------------------------------------------
// Crashes
// ---------------------------------------------------------------------------
void ABoatPawn::OnHullHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!IsLocallyControlled() || OtherActor == this)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? static_cast<float>(World->GetTimeSeconds()) : 0.f;
	// How fast we were closing on whatever we hit, from before the bounce.
	const float Closing = static_cast<float>(-FVector::DotProduct(LastVelocity, FVector(Hit.ImpactNormal)));
	if (Closing < 450.f || Now - LastImpactTime < 0.4f)
	{
		return;
	}
	LastImpactTime = Now;
	CameraShake = FMath::Max(CameraShake, FMath::Clamp(Closing / 1800.f, 0.25f, 1.f));
	Rumble(FMath::Clamp(Closing / 1500.f, 0.3f, 1.f), 0.3f);
	BoatAudio::PlayAt(this, EBoatSound::Crash, Hit.ImpactPoint, FMath::Clamp(Closing / 1500.f, 0.3f, 1.f), FMath::FRandRange(0.9f, 1.1f));
	for (int32 Index = 0; Index < 6; ++Index)
	{
		EmitPuff(Hit.ImpactPoint, FVector(FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(-300.f, 300.f), FMath::FRandRange(300.f, 700.f)),
			0.8f, 120.f, 380.f, FLinearColor(0.85f, 0.9f, 0.95f), 0.6f, 900.f);
	}

	ABoatPawn* OtherBoat = Cast<ABoatPawn>(OtherActor);
	if (HasAuthority())
	{
		HandleImpact(Hit.ImpactPoint, Closing, OtherBoat);
	}
	else
	{
		ServerReportImpact(Hit.ImpactPoint, Closing, OtherBoat);
	}
}

void ABoatPawn::ServerReportImpact_Implementation(FVector_NetQuantize Location, float Speed, ABoatPawn* OtherBoat)
{
	HandleImpact(Location, FMath::Clamp(Speed, 0.f, ReferenceTopSpeed * 1.5f), OtherBoat);
}

void ABoatPawn::HandleImpact(const FVector& Location, float Speed, ABoatPawn* OtherBoat)
{
	MulticastImpact(Location, Speed);

	ABoatRacePlayerState* Me = GetPlayerState<ABoatRacePlayerState>();
	ABoatRaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	auto Report = [RaceState](ABoatPawn* Victim, const ABoatRacePlayerState* By)
	{
		if (RaceState != nullptr && Victim != nullptr)
		{
			const ABoatRacePlayerState* VictimState = Victim->GetPlayerState<ABoatRacePlayerState>();
			RaceState->ReportWreck(VictimState ? VictimState->GetPlayerName() : FString(TEXT("A boat")),
				By ? By->GetPlayerName() : FString());
		}
	};

	if (OtherBoat != nullptr)
	{
		// Ramming: the boat that was hit takes the damage, and the rammer gets the credit.
		const float Damage = FMath::Clamp((Speed - 600.f) / 45.f, 0.f, 22.f);
		if (Damage > 0.f && OtherBoat->ApplyDamage(Damage, Me))
		{
			Report(OtherBoat, Me);
		}
	}
	else
	{
		// Rocks, barges, logs and the shore: a hard hit hurts your own hull.
		const float Damage = FMath::Clamp((Speed - 1000.f) / 55.f, 0.f, 30.f);
		if (Damage > 0.f && ApplyDamage(Damage, nullptr))
		{
			Report(this, nullptr);
		}
	}
}

void ABoatPawn::MulticastImpact_Implementation(FVector_NetQuantize Location, float Speed)
{
	if (!IsLocallyControlled())
	{
		BoatAudio::PlayAt(this, EBoatSound::Crash, Location, FMath::Clamp(Speed / 1500.f, 0.3f, 1.f), FMath::FRandRange(0.9f, 1.1f));
	}
}

// ---------------------------------------------------------------------------
// Spray, wake, smoke and fire
// ---------------------------------------------------------------------------
void ABoatPawn::BuildEffects()
{
	const UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"), nullptr, LOAD_NoWarn | LOAD_Quiet);

	const int32 PoolSize = 30;
	Puffs.SetNum(PoolSize);
	for (int32 Index = 0; Index < PoolSize; ++Index)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
		Puff->SetStaticMesh(Sphere);
		Puff->SetupAttachment(Hull);
		Puff->SetUsingAbsoluteLocation(true);
		Puff->SetUsingAbsoluteRotation(true);
		Puff->SetUsingAbsoluteScale(true);
		Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Puff->SetCastShadow(false);
		Puff->SetVisibility(false);
		Puff->RegisterComponent();
		UMaterialInstanceDynamic* Material = BoatArt::SoftMaterial(Puff, FLinearColor::White, 0.f);
		Puff->SetMaterial(0, Material);
		PuffMeshes.Add(Puff);
		PuffMaterials.Add(Material);
	}

	Wake.Init(this);

	// The shield bubble, hidden until the boat cannot be hurt.
	ShieldBubble = NewObject<UStaticMeshComponent>(this);
	ShieldBubble->SetStaticMesh(Sphere);
	ShieldBubble->SetupAttachment(VisualRoot);
	ShieldBubble->SetRelativeLocation(FVector(-40.f, 0.f, 60.f));
	ShieldBubble->SetRelativeScale3D(FVector(9.f, 4.f, 3.4f));
	ShieldBubble->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShieldBubble->SetCastShadow(false);
	ShieldBubble->RegisterComponent();
	// Glass is clear head-on and bright at grazing angles: a force field's rim.
	if (UMaterialInterface* Glass = BoatArt::Material(TEXT("M_Glass")))
	{
		ShieldMaterial = UMaterialInstanceDynamic::Create(Glass, ShieldBubble);
		ShieldMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.2f, 0.9f, 1.4f));
	}
	else
	{
		ShieldMaterial = BoatArt::SoftMaterial(ShieldBubble, FLinearColor(0.3f, 1.4f, 2.f), 0.f);
	}
	ShieldBubble->SetMaterial(0, ShieldMaterial);
	ShieldBubble->SetVisibility(false);

	// Bow spray: two fans of white water, mirrored, thrown aft and outwards.
	if (UStaticMesh* Sheet = BoatArt::Model(TEXT("SpraySheet")))
	{
		for (const float Side : { 1.f, -1.f })
		{
			UStaticMeshComponent* Spray = NewObject<UStaticMeshComponent>(this);
			Spray->SetStaticMesh(Sheet);
			Spray->SetupAttachment(VisualRoot);
			Spray->SetRelativeLocation(FVector(170.f, Side * 95.f, -35.f));
			// Thrown aft and outwards, and tipped up so it stands as a sheet.
			Spray->SetRelativeRotation(FRotator(4.f, Side * 140.f, Side * 22.f));
			Spray->SetRelativeScale3D(FVector(1.f, Side, 1.f) * BoatArt::UnitScale(Sheet, 0.9f));
			Spray->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Spray->SetCastShadow(false);
			Spray->RegisterComponent();
			const TArray<UMaterialInstanceDynamic*> Painted = BoatArt::Paint(Spray);
			UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Spray->GetMaterial(0));
			Spray->SetVisibility(false);
			SpraySheets.Add(Spray);
			SprayMaterials.Add(Material);
		}
	}

	// The rooster tail the outboard throws up at speed.
	RoosterTail = NewObject<UStaticMeshComponent>(this);
	RoosterTail->SetStaticMesh(Cone);
	RoosterTail->SetupAttachment(VisualRoot);
	RoosterTail->SetRelativeLocation(FVector(-560.f, 0.f, 40.f));
	RoosterTail->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	RoosterTail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RoosterTail->SetCastShadow(false);
	RoosterTail->RegisterComponent();
	RoosterMaterial = BoatArt::SoftMaterial(RoosterTail, FLinearColor(0.9f, 0.95f, 1.f), 0.f);
	RoosterTail->SetMaterial(0, RoosterMaterial);
	RoosterTail->SetVisibility(false);

	// Flames out of the engine cowling when the hull is nearly gone.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Flame = NewObject<UStaticMeshComponent>(this);
		Flame->SetStaticMesh(Cone);
		Flame->SetupAttachment(VisualRoot);
		Flame->SetRelativeLocation(FVector(-360.f + Index * 40.f, (Index - 1) * 35.f, 110.f));
		Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flame->SetCastShadow(false);
		Flame->RegisterComponent();
		Flame->SetMaterial(0, BoatArt::GlowMaterial(Flame, Index == 1 ? FLinearColor(9.f, 3.f, 0.4f) : FLinearColor(7.f, 1.6f, 0.15f)));
		Flame->SetVisibility(false);
		Flames.Add(Flame);
	}
}

void ABoatPawn::EmitPuff(const FVector& Location, const FVector& Velocity, float Life, float StartSize, float EndSize,
	const FLinearColor& Color, float Opacity, float Gravity, float Flat, bool bOnWater)
{
	if (Puffs.Num() == 0)
	{
		return;
	}
	const int32 Index = NextPuff;
	NextPuff = (NextPuff + 1) % Puffs.Num();
	FPuff& Puff = Puffs[Index];
	Puff.Age = 0.f;
	Puff.Life = Life;
	Puff.Velocity = Velocity;
	Puff.StartSize = StartSize;
	Puff.EndSize = EndSize;
	Puff.Color = Color;
	Puff.Opacity = Opacity;
	Puff.Gravity = Gravity;
	Puff.Flat = Flat;
	Puff.bOnWater = bOnWater;
	if (PuffMeshes.IsValidIndex(Index) && PuffMeshes[Index] != nullptr)
	{
		PuffMeshes[Index]->SetWorldLocation(Location);
		PuffMeshes[Index]->SetWorldScale3D(FVector(StartSize / 100.f));
		PuffMeshes[Index]->SetVisibility(true);
	}
}

void ABoatPawn::UpdateEffects(float DeltaSeconds)
{
	if (PuffMeshes.Num() == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}

	const FVector Location = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const FVector Right = GetActorRightVector();
	const float Speed = GetForwardSpeed();
	const float SpeedFraction = FMath::Clamp(FMath::Abs(Speed) / ReferenceTopSpeed, 0.f, 1.f);
	const float Surface = OceanWaves::GetHeight(FVector2D(Location.X, Location.Y), GetWaveTime());
	const float AboveWater = static_cast<float>(Location.Z) - Surface - BoatTrack::BoatRideHeight;
	const bool bInWater = AboveWater < 120.f;

	// --- Landing from a jump: a sheet of water and a slap.
	if (!bInWater)
	{
		AirTime += DeltaSeconds;
	}
	else
	{
		if (AirTime > 0.45f)
		{
			BoatAudio::PlayAt(this, EBoatSound::Splash, Location, FMath::Clamp(AirTime, 0.5f, 1.f), FMath::FRandRange(0.8f, 1.f));
			for (int32 Index = 0; Index < 10; ++Index)
			{
				const float Angle = Index * 0.628f;
				const FVector Out = Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle);
				EmitPuff(Location + Out * 250.f, Out * 500.f + FVector(0, 0, 650.f), 1.f, 90.f, 320.f,
					FLinearColor(0.92f, 0.96f, 1.f) * 1.6f, 0.45f, 900.f);
			}
			if (IsLocallyControlled())
			{
				CameraShake = FMath::Max(CameraShake, FMath::Clamp(AirTime * 0.7f, 0.3f, 0.9f));
				Rumble(FMath::Clamp(AirTime * 0.6f, 0.3f, 0.9f), 0.25f);
			}
		}
		AirTime = 0.f;
	}

	// Foam is unlit, so match it to the light: bright in sun, dim at night.
	const ABoatRaceGameState* WeatherState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	const FWeatherLook& Light = RaceWeather::Get(WeatherState ? WeatherState->GetWeather() : ERaceWeather::Sunny);
	const float Foam = Light.FoamBrightness;

	// --- Wake and spray, in proportion to speed. The wake is flat foam left
	// lying on the water behind the transom; the bow throws small, short-lived
	// sheets of spray out to the sides.
	if (bInWater && SpeedFraction > 0.12f && !bWrecked)
	{
		const bool bShowBoost = IsLocallyControlled() ? bBoostActive : NetState.bBoosting;
		SprayAccumulator += DeltaSeconds * (3.f + 9.f * SpeedFraction) * (bShowBoost ? 2.5f : 1.f);
		while (SprayAccumulator > 1.f)
		{
			SprayAccumulator -= 1.f;
			// The white water behind is the wake ribbon; puffs are only a
			// fine mist kicked up beside the bow.
			const float Side = FMath::RandBool() ? 1.f : -1.f;
			EmitPuff(Location + Forward * 150.f + Right * (Side * 140.f) + FVector(0, 0, -20),
				Right * (Side * 380.f * SpeedFraction) - Forward * (Speed * 0.1f) + FVector(0, 0, 260.f * SpeedFraction),
				0.45f, 30.f, 95.f, FLinearColor(0.92f, 0.96f, 1.f) * Foam, 0.2f, 800.f, 0.7f, false);
		}
	}

	// --- The wake ribbon, and the sheets of spray off the bow.
	const float WakeNow = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	Wake.Update(Location - Forward * 380.f, Forward, FMath::Abs(Speed), bInWater && !bWrecked, GetWaveTime(), WakeNow, Foam);
	for (int32 Index = 0; Index < SpraySheets.Num(); ++Index)
	{
		UStaticMeshComponent* Spray = SpraySheets[Index];
		const bool bShow = bInWater && !bWrecked && SpeedFraction > 0.3f;
		Spray->SetVisibility(bShow);
		if (bShow)
		{
			const float Flicker = 0.85f + 0.15f * FMath::Sin(WakeNow * (21.f + Index * 3.f)) + FMath::FRandRange(-0.05f, 0.05f);
			const float Side = Index == 0 ? 1.f : -1.f;
			const float Grow = (0.5f + 0.9f * SpeedFraction) * Flicker;
			const float Unit = SpraySheets[Index]->GetStaticMesh() ? BoatArt::UnitScale(SpraySheets[Index]->GetStaticMesh(), 0.9f) : 1.f;
			// Long and low: a sheet skimming off the hull, not a fin standing up.
			Spray->SetRelativeScale3D(FVector(Grow * 1.1f, Side * Grow, Grow * (0.35f + 0.35f * SpeedFraction)) * Unit);
			if (SprayMaterials.IsValidIndex(Index) && SprayMaterials[Index] != nullptr)
			{
				SprayMaterials[Index]->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.96f, 1.f) * Foam);
				SprayMaterials[Index]->SetScalarParameterValue(TEXT("Opacity"), FMath::Clamp((SpeedFraction - 0.3f) * 0.7f, 0.f, 0.28f));
			}
		}
	}

	// --- Smoke from a damaged engine; thick and black from a wreck.
	const float HealthFraction = MaxHealth > 0.f ? Health / MaxHealth : 1.f;
	if (bWrecked || HealthFraction < 0.45f)
	{
		SmokeAccumulator += DeltaSeconds * (bWrecked ? 14.f : (HealthFraction < 0.25f ? 9.f : 4.f));
		while (SmokeAccumulator > 1.f)
		{
			SmokeAccumulator -= 1.f;
			const float Grey = bWrecked ? 0.03f : 0.12f;
			EmitPuff(Location - Forward * 340.f + FVector(0, 0, 150.f),
				FVector(FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(-60.f, 60.f), FMath::FRandRange(220.f, 380.f)) - Forward * Speed * 0.3f,
				bWrecked ? 3.f : 2.2f, 70.f, bWrecked ? 420.f : 260.f, FLinearColor(Grey, Grey, Grey), bWrecked ? 0.55f : 0.4f, -60.f);
		}
	}

	// --- Animate the pool.
	for (int32 Index = 0; Index < Puffs.Num(); ++Index)
	{
		FPuff& Puff = Puffs[Index];
		UStaticMeshComponent* Mesh = PuffMeshes[Index];
		if (Puff.Life <= 0.f || Mesh == nullptr)
		{
			continue;
		}
		Puff.Age += DeltaSeconds;
		const float Alpha = Puff.Age / Puff.Life;
		if (Alpha >= 1.f)
		{
			Puff.Life = 0.f;
			Mesh->SetVisibility(false);
			continue;
		}
		Puff.Velocity.Z -= Puff.Gravity * DeltaSeconds;
		Puff.Velocity *= FMath::Max(1.f - DeltaSeconds * 1.2f, 0.f);
		FVector Position = Mesh->GetComponentLocation() + Puff.Velocity * DeltaSeconds;
		if (Puff.bOnWater)
		{
			Position.Z = OceanWaves::GetHeight(FVector2D(Position.X, Position.Y), GetWaveTime()) + 8.0;
		}
		Mesh->SetWorldLocation(Position);
		const float Size = FMath::Lerp(Puff.StartSize, Puff.EndSize, FMath::Sqrt(Alpha)) / 100.f;
		Mesh->SetWorldScale3D(FVector(Size, Size, Size * Puff.Flat));
		BoatArt::SetPuff(PuffMaterials[Index], Puff.Color, Puff.Opacity * (1.f - Alpha) * FMath::Min(Alpha * 8.f, 1.f));
	}

	// --- Rooster tail.
	if (RoosterTail != nullptr)
	{
		// Kept off: a solid cone reads as a white pole, not water. The bow
		// spray and the foam wake carry the sense of speed instead.
		const bool bShow = false;
		RoosterTail->SetVisibility(bShow);
		if (bShow)
		{
			const float Length = 1.5f + 4.f * SpeedFraction;
			RoosterTail->SetRelativeScale3D(FVector(0.8f + FMath::FRandRange(-0.08f, 0.08f), 0.8f, Length));
			BoatArt::SetPuff(RoosterMaterial, FLinearColor(0.9f, 0.95f, 1.f) * 1.6f, 0.25f * SpeedFraction);
		}
	}

	// --- Fire.
	const bool bBurning = bWrecked || HealthFraction < 0.25f;
	const float Now = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	for (int32 Index = 0; Index < Flames.Num(); ++Index)
	{
		UStaticMeshComponent* Flame = Flames[Index];
		if (Flame == nullptr)
		{
			continue;
		}
		Flame->SetVisibility(bBurning);
		if (bBurning)
		{
			const float Flicker = 0.75f + 0.25f * FMath::Sin(Now * (17.f + Index * 5.f) + Index) + FMath::FRandRange(-0.1f, 0.1f);
			const float Height = (bWrecked ? 2.4f : 1.5f) * Flicker * (Index == 1 ? 1.3f : 1.f);
			Flame->SetRelativeScale3D(FVector(0.55f * Flicker, 0.55f * Flicker, Height));
		}
	}
	FireLight->SetVisibility(bBurning);
	FireLight->SetIntensity(bBurning ? 9000.f * (0.8f + 0.4f * FMath::Sin(Now * 23.f)) : 0.f);

	// --- Shield bubble: shimmering while the boat cannot be hurt.
	PickupAge += DeltaSeconds;
	if (ShieldBubble != nullptr)
	{
		const bool bShielded = IsInvulnerable() && !bWrecked;
		ShieldBubble->SetVisibility(bShielded);
		if (bShielded)
		{
			if (ShieldMaterial != nullptr)
			{
				ShieldMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.04f + 0.03f * FMath::Sin(Now * 6.f));
			}
		}
	}

	// --- Running light at night.
	const ABoatRaceGameState* RaceState = GetWorld() ? GetWorld()->GetGameState<ABoatRaceGameState>() : nullptr;
	const bool bNight = RaceState && RaceWeather::Get(RaceState->GetWeather()).bNight;
	Headlight->SetVisibility(bNight && !bWrecked);
	Headlight->SetIntensity(bNight ? 60000.f : 0.f);
}

void ABoatPawn::UpdateAudio(float DeltaSeconds)
{
	UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this);
	if (Audio == nullptr)
	{
		return;
	}
	if (EngineAudio == nullptr)
	{
		EngineAudio = Audio->MakeLoop(Hull, EBoatSound::Engine, true);
		WaterAudio = Audio->MakeLoop(Hull, EBoatSound::WaterRush, true);
		if (EngineAudio == nullptr)
		{
			return;
		}
		EngineAudio->Play();
		if (WaterAudio != nullptr)
		{
			WaterAudio->Play();
		}
	}

	const float Throttle = IsLocallyControlled() ? InputThrottle : NetState.Throttle;
	const float SpeedFraction = FMath::Clamp(FMath::Abs(GetForwardSpeed()) / ReferenceTopSpeed, 0.f, 1.f);
	// Revs follow speed, with a flare on the throttle and when the prop leaves the water.
	const float Airborne = AirTime > 0.15f ? 0.45f : 0.f;
	const float TargetRevs = bWrecked ? 0.f : FMath::Clamp(0.15f + 0.65f * SpeedFraction + 0.25f * FMath::Abs(Throttle) + Airborne, 0.f, 1.3f);
	EngineRevs = FMath::FInterpTo(EngineRevs, TargetRevs, DeltaSeconds, TargetRevs > EngineRevs ? 3.f : 1.8f);

	const bool bBoostSound = IsLocallyControlled() ? bBoostActive : NetState.bBoosting;
	EngineAudio->SetPitchMultiplier(0.65f + 1.75f * EngineRevs + (bBoostSound ? 0.3f : 0.f));
	const float Loudness = (bWrecked ? 0.f : 0.3f + 0.55f * FMath::Abs(Throttle) + 0.25f * SpeedFraction) * (IsLocallyControlled() ? 0.75f : 1.f);
	EngineAudio->SetVolumeMultiplier(FMath::Max(Loudness * Audio->EffectsVolume, 0.001f));
	if (WaterAudio != nullptr)
	{
		WaterAudio->SetVolumeMultiplier(FMath::Max(SpeedFraction * (AirTime > 0.1f ? 0.1f : 0.9f), 0.001f));
		WaterAudio->SetPitchMultiplier(0.8f + 0.5f * SpeedFraction);
	}
}
