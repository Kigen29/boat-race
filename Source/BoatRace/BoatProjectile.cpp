#include "BoatProjectile.h"

#include "BoatPawn.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatRacePlayerState.h"
#include "BoatTrack.h"
#include "OceanWaves.h"
#include "RaceEnvironment.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Server clock, which every machine shares. */
	float ServerNow(const UWorld* World)
	{
		const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
		return RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
	}

	UStaticMesh* LoadBasicShape(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (Mesh == nullptr)
		{
			return;
		}
		if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}

	/** Puts an actor on the wave surface at its anchor point. */
	void FloatAt(AActor* Actor, const FVector2D& Anchor, float Seconds, float Rise)
	{
		if (Actor == nullptr)
		{
			return;
		}
		const float SurfaceZ = OceanWaves::GetHeight(Anchor, Seconds);
		Actor->SetActorLocation(FVector(Anchor.X, Anchor.Y, static_cast<double>(SurfaceZ + Rise)));
	}
}

// ===========================================================================
// Blast
// ===========================================================================
void BoatBlast::Detonate(UWorld* World, const FVector& Origin, float Radius, float Damage,
	float Impulse, ABoatPawn* FiredBy)
{
	if (World == nullptr)
	{
		return;
	}

	// The visible flash, spawned by the server and replicated to everyone.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ABoatExplosion* Effect = World->SpawnActor<ABoatExplosion>(ABoatExplosion::StaticClass(), Origin, FRotator::ZeroRotator, Params))
	{
		Effect->SetBlastRadius(Radius);
	}

	ABoatRacePlayerState* Attacker = (FiredBy != nullptr) ? FiredBy->GetPlayerState<ABoatRacePlayerState>() : nullptr;
	ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr)
		{
			continue;
		}

		const float Distance = static_cast<float>(FVector::Dist(Boat->GetRaceLocation(), Origin));
		if (Distance > Radius)
		{
			continue;
		}

		// Linear falloff: a direct hit hurts, the edge of the blast only shoves.
		const float Falloff = FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);

		// The shove reaches everyone, including whoever fired: a close shot
		// knocks you off your own line too.
		Boat->ApplyExplosionImpulse(Origin, Impulse * Falloff);

		if (Boat == FiredBy)
		{
			continue; // Your own weapon cannot damage you.
		}

		const bool bWrecked = Boat->ApplyDamage(Damage * Falloff, Attacker);
		if (bWrecked && RaceState != nullptr)
		{
			const ABoatRacePlayerState* Victim = Boat->GetPlayerState<ABoatRacePlayerState>();
			RaceState->ReportWreck(
				Victim != nullptr ? Victim->GetPlayerName() : FString(TEXT("A boat")),
				Attacker != nullptr ? Attacker->GetPlayerName() : FString());
		}
	}

	// Scatter any floating debris caught in it.
	for (TActorIterator<ADebrisBarrel> It(World); It; ++It)
	{
		if (ADebrisBarrel* Barrel = *It)
		{
			Barrel->PushFromBlast(Origin, Radius, Impulse);
		}
	}
}

// ===========================================================================
// Explosion effect
// ===========================================================================
ABoatExplosion::ABoatExplosion()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	// Long enough for the animation; the server also destroys it.
	InitialLifeSpan = 2.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Flash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flash"));
	SetRootComponent(Flash);
	Flash->SetStaticMesh(SphereFinder.Object);
	Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Flash->SetCastShadow(false);

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Flash);
	Ring->SetStaticMesh(SphereFinder.Object);
	Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ring->SetCastShadow(false);
}

void ABoatExplosion::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatExplosion, BlastRadius);
}

void ABoatExplosion::SetBlastRadius(float Radius)
{
	BlastRadius = FMath::Max(Radius, 50.f);
}

void ABoatExplosion::BeginPlay()
{
	Super::BeginPlay();

	// A translucent additive material if import_assets.sh built one, otherwise
	// a plain bright sphere, which still reads as a blast.
	UMaterialInterface* BlastMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Higgsfield/M_Explosion.M_Explosion"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (BlastMaterial != nullptr)
	{
		Flash->SetMaterial(0, BlastMaterial);
		Ring->SetMaterial(0, BlastMaterial);
	}
	TintMesh(Flash, FLinearColor(6.f, 2.2f, 0.3f));
	TintMesh(Ring, FLinearColor(2.f, 2.f, 2.2f));
}

void ABoatExplosion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	const float Alpha = FMath::Clamp(Age / Lifetime, 0.f, 1.f);

	// Swells fast, then collapses: the shape of a real blast seen from outside.
	const float Swell = FMath::Sin(FMath::Clamp(Alpha * 2.4f, 0.f, 1.f) * HALF_PI);
	const float Collapse = 1.f - FMath::Clamp((Alpha - 0.35f) / 0.65f, 0.f, 1.f);
	const float FlashSize = (BlastRadius / 50.f) * Swell * Collapse;
	Flash->SetWorldScale3D(FVector(FMath::Max(FlashSize, 0.01f)));

	// The shockwave keeps spreading across the water as the flash dies.
	const float RingSize = (BlastRadius / 50.f) * (0.4f + 2.2f * Alpha);
	Ring->SetWorldScale3D(FVector(RingSize, RingSize, 0.04f));

	if (Alpha >= 1.f)
	{
		Destroy();
	}
}

// ===========================================================================
// Torpedo
// ===========================================================================
ABoatTorpedo::ABoatTorpedo()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// Its position is worked out from the launch details rather than sent.
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(45.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Collision);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetStaticMesh(CylinderFinder.Object);
	// Pitch 90 lays the cylinder along the direction of travel.
	Body->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Body->SetRelativeScale3D(FVector(0.35f, 0.35f, 1.6f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Wake = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Wake"));
	Wake->SetupAttachment(Collision);
	Wake->SetStaticMesh(SphereFinder.Object);
	Wake->SetRelativeLocation(FVector(-90.f, 0.f, -10.f));
	Wake->SetRelativeScale3D(FVector(1.1f, 0.5f, 0.12f));
	Wake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wake->SetCastShadow(false);
}

void ABoatTorpedo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatTorpedo, LaunchLocation);
	DOREPLIFETIME(ABoatTorpedo, LaunchDirection);
	DOREPLIFETIME(ABoatTorpedo, LaunchTime);
}

void ABoatTorpedo::InitProjectile(ABoatPawn* Shooter)
{
	FiredBy = Shooter;
	LaunchLocation = GetActorLocation();
	LaunchDirection = GetActorForwardVector().GetSafeNormal2D();
	LaunchTime = ServerNow(GetWorld());
	PreviousLocation = LaunchLocation;
}

void ABoatTorpedo::BeginPlay()
{
	Super::BeginPlay();
	TintMesh(Body, FLinearColor(0.75f, 0.75f, 0.8f));
	TintMesh(Wake, FLinearColor(3.f, 3.f, 3.2f));
	if (PreviousLocation.IsNearlyZero())
	{
		PreviousLocation = GetActorLocation();
	}
}

void ABoatTorpedo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || bExploded)
	{
		return;
	}

	const float Now = ServerNow(World);
	const float Elapsed = Now - LaunchTime;
	if (Elapsed < 0.f)
	{
		return; // Clocks not yet in step on a client that just joined.
	}

	// Same arithmetic on every machine, so it looks identical everywhere
	// without a single position update being sent.
	FVector Target = LaunchLocation + LaunchDirection * (Speed * Elapsed);
	Target.Z = OceanWaves::GetHeight(FVector2D(Target.X, Target.Y), Now) + 25.f;
	SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(LaunchDirection.Rotation());

	if (!HasAuthority())
	{
		return; // Only the server decides what was hit.
	}

	// Sweep the gap since the last check, so nothing is missed at 55 m/s.
	FHitResult Hit;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	if (FiredBy != nullptr)
	{
		QueryParams.AddIgnoredActor(FiredBy); // Cannot hit the boat that fired it.
	}
	const bool bHit = World->SweepSingleByChannel(Hit, PreviousLocation, Target, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(45.f), QueryParams);
	PreviousLocation = Target;

	if (bHit)
	{
		Explode(Hit.ImpactPoint);
		return;
	}

	if (Elapsed > RangeSeconds
		|| FMath::Abs(Target.X) > BoatTrack::BoundsX
		|| FMath::Abs(Target.Y) > BoatTrack::BoundsY)
	{
		Explode(Target); // Runs out of fuel and goes off harmlessly far away.
	}
}

void ABoatTorpedo::Explode(const FVector& At)
{
	if (bExploded || !HasAuthority())
	{
		return;
	}
	bExploded = true;
	BoatBlast::Detonate(GetWorld(), At, BlastRadius, Damage, BlastImpulse, FiredBy);
	Destroy();
}

// ===========================================================================
// Mine
// ===========================================================================
ABoatMine::ABoatMine()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(SphereFinder.Object);
	Body->SetRelativeScale3D(FVector(0.9f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Light = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Light"));
	Light->SetupAttachment(Body);
	Light->SetStaticMesh(SphereFinder.Object);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Light->SetRelativeScale3D(FVector(0.3f));
	Light->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Light->SetCastShadow(false);
}

void ABoatMine::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatMine, Anchor);
	DOREPLIFETIME(ABoatMine, DropTime);
}

void ABoatMine::InitProjectile(ABoatPawn* Shooter)
{
	FiredBy = Shooter;
	const FVector Location = GetActorLocation();
	Anchor = FVector2D(Location.X, Location.Y);
	DropTime = ServerNow(GetWorld());
}

void ABoatMine::BeginPlay()
{
	Super::BeginPlay();
	TintMesh(Body, FLinearColor(0.08f, 0.08f, 0.09f));
	TintMesh(Light, FLinearColor(4.f, 0.2f, 0.2f));
}

void ABoatMine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr || bExploded)
	{
		return;
	}

	const float Now = ServerNow(World);
	const float Age = Now - DropTime;
	FloatAt(this, Anchor, Now, 30.f);

	// The lamp blinks slowly while it arms, then fast once it is live.
	const bool bArmed = Age >= ArmSeconds;
	const float BlinkRate = bArmed ? 7.f : 2.f;
	const float Blink = 0.5f + 0.5f * FMath::Sin(Now * BlinkRate);
	TintMesh(Light, FLinearColor(0.4f + 5.f * Blink, 0.15f, 0.15f));

	if (!HasAuthority() || !bArmed)
	{
		return;
	}

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr || Boat->IsWrecked())
		{
			continue;
		}
		// The boat that laid it gets a moment to clear the area; after that it
		// is as dangerous to them as to anyone.
		if (Boat == FiredBy && Age < ArmSeconds + 2.f)
		{
			continue;
		}
		if (FVector::Dist(Boat->GetRaceLocation(), GetActorLocation()) <= TriggerRadius)
		{
			bExploded = true;
			BoatBlast::Detonate(World, GetActorLocation(), BlastRadius, Damage, BlastImpulse, FiredBy);
			Destroy();
			return;
		}
	}

	if (Age > LifetimeSeconds)
	{
		Destroy(); // Sinks harmlessly rather than littering the course all race.
	}
}

// ===========================================================================
// Ammo crate
// ===========================================================================
AWeaponCrate::AWeaponCrate()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(CubeFinder.Object);
	Body->SetRelativeScale3D(FVector(1.3f, 1.3f, 1.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// A floating marker, so a crate can be spotted from the last gate.
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Body);
	Marker->SetStaticMesh(ConeFinder.Object);
	Marker->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
	Marker->SetRelativeScale3D(FVector(0.9f, 0.9f, 1.4f));
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCastShadow(false);
}

void AWeaponCrate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWeaponCrate, bAvailable);
	DOREPLIFETIME(AWeaponCrate, Anchor);
}

void AWeaponCrate::SetUpCrate(const FVector2D& Position, int32 InTorpedoes, int32 InMines, float InRespawnSeconds)
{
	Anchor = Position;
	Torpedoes = InTorpedoes;
	Mines = InMines;
	RespawnSeconds = FMath::Max(InRespawnSeconds, 1.f);
	bAvailable = true;
}

void AWeaponCrate::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float Now = ServerNow(World);
	FloatAt(this, Anchor, Now, 45.f);
	// Turning slowly so it catches the light and is easier to see.
	SetActorRotation(FRotator(0.f, Now * 45.f, 0.f));

	Body->SetVisibility(bAvailable);
	Marker->SetVisibility(bAvailable);
	TintMesh(Body, FLinearColor(0.5f, 0.35f, 0.12f));
	TintMesh(Marker, FLinearColor(1.6f, 1.2f, 0.1f));

	if (!HasAuthority())
	{
		return;
	}

	if (!bAvailable)
	{
		if (Now >= RespawnAtServerTime)
		{
			bAvailable = true;
		}
		return;
	}

	for (TActorIterator<ABoatPawn> It(World); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == nullptr || Boat->IsWrecked())
		{
			continue;
		}
		if (FVector::Dist(Boat->GetRaceLocation(), GetActorLocation()) <= PickupRadius)
		{
			Boat->GiveAmmo(Torpedoes, Mines);
			bAvailable = false;
			RespawnAtServerTime = Now + RespawnSeconds;
			return;
		}
	}
}
