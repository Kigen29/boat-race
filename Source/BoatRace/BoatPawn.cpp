#include "BoatPawn.h"

#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
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

	// The camera hangs off the hull but is told not to inherit its roll and
	// pitch, so a wave or a barrel roll does not throw the view around.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Hull);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 190.f));
	SpringArm->SetRelativeRotation(FRotator(-14.f, 0.f, 0.f));
	SpringArm->TargetArmLength = 1450.f;
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
		SimulatePhysics(DeltaSeconds, WaveTime);
		SendMoveToServer(DeltaSeconds);
	}
	else
	{
		FollowReportedState(DeltaSeconds);
	}

	UpdateCosmetics(DeltaSeconds);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
void ABoatPawn::ReadInput(float DeltaSeconds)
{
	float Throttle = 0.f;
	float Steer = 0.f;

	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController != nullptr && CanDrive())
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

	// A capsize should cost time, not end the race, so help the hull back over
	// once it is past the point where buoyancy alone would right it.
	const FVector Up = GetActorUpVector();
	const float TiltDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(Up.Z), -1.f, 1.f)));
	if (TiltDegrees > SelfRightAngle)
	{
		const FVector Axis = FVector::CrossProduct(Up, FVector::UpVector);
		const float Strength = (TiltDegrees - SelfRightAngle) / 110.f;
		Hull->AddTorqueInRadians(Axis.GetSafeNormal() * (YawInertia * 2.2f * FMath::Clamp(Strength, 0.f, 1.f)));
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
		const float Power = (InputThrottle >= 0.f) ? InputThrottle : InputThrottle * ReverseFraction;
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
	if (WaterContact > 0.f)
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

	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	InvulnerableUntil = (RaceState ? RaceState->GetServerNow() : 0.f) + InvulnerableSeconds;
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
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, 85.f + 13.f * SpeedFraction, DeltaSeconds, 2.f));
		SpringArm->TargetArmLength = FMath::FInterpTo(SpringArm->TargetArmLength, 1450.f + 220.f * SpeedFraction, DeltaSeconds, 2.f);
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
	bColorApplied = true;
}

void ABoatPawn::TryLoadCustomMesh()
{
	UStaticMesh* BoatModel = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Higgsfield/SM_Boat.SM_Boat"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (BoatModel == nullptr)
	{
		return; // No Higgsfield model imported: keep the placeholder boat.
	}

	const FBoxSphereBounds Bounds = BoatModel->GetBounds();
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
	const float Scale = (HullLength / Longest) * FMath::Max(CustomMeshScaleMultiplier, 0.01f);
	const float Yaw = (SizeY > SizeX ? 90.f : 0.f) + CustomMeshYawOffset;
	const FRotator Rotation(0.f, Yaw, 0.f);

	const FVector ScaledOrigin = Bounds.Origin * static_cast<double>(Scale);
	const FVector Centering = -Rotation.RotateVector(FVector(ScaledOrigin.X, ScaledOrigin.Y, 0.0));
	const float ModelBottom = static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
	const float OffsetZ = -HullHeight * 0.5f - ModelBottom + CustomMeshZOffset;

	CustomMesh->SetStaticMesh(BoatModel);
	CustomMesh->SetRelativeScale3D(FVector(Scale));
	CustomMesh->SetRelativeRotation(Rotation);
	CustomMesh->SetRelativeLocation(FVector(Centering.X, Centering.Y, static_cast<double>(OffsetZ)));
	CustomMesh->SetVisibility(true);

	HullMesh->SetVisibility(false);
	BowMesh->SetVisibility(false);
	CabinMesh->SetVisibility(false);
	EngineMesh->SetVisibility(false);
	bUsingCustomMesh = true;

	UE_LOG(LogBoatRace, Log, TEXT("Using the Higgsfield boat model (scale %.3f, yaw %.0f)."), Scale, Yaw);
}
