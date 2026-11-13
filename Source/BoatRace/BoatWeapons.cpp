#include "BoatWeapons.h"

#include "BayTerrain.h"
#include "BoatArt.h"
#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "OceanWaves.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	float WeaponServerNow(const UWorld* World)
	{
		const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
		return RaceState ? RaceState->GetServerNow() : (World ? static_cast<float>(World->GetTimeSeconds()) : 0.f);
	}
}

// ===========================================================================
// Missile
// ===========================================================================
ABoatMissile::ABoatMissile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 9.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.25f, 0.25f, 1.6f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCastShadow(false);

	Exhaust = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Exhaust"));
	Exhaust->SetupAttachment(Body);
	Exhaust->SetStaticMesh(Cone.Object);
	Exhaust->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Exhaust->SetCastShadow(false);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Body);
	Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Glow->SetIntensity(15000.f);
	Glow->SetAttenuationRadius(1800.f);
	Glow->SetCastShadows(false);
}

void ABoatMissile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoatMissile, Target);
}

void ABoatMissile::BeginPlay()
{
	Super::BeginPlay();
	// The torpedo model, red, as a rocket; otherwise the cylinder.
	if (UStaticMesh* Model = BoatArt::Model(TEXT("Torpedo")))
	{
		Body->SetStaticMesh(Model);
		Body->SetRelativeScale3D(FVector(BoatArt::UnitScale(Model, 0.5f) * 1.2f));
		BoatArt::FLook Look;
		Look.Paint = FLinearColor(0.8f, 0.05f, 0.03f);
		BoatArt::Paint(Body, Look);
		Exhaust->SetRelativeLocation(FVector(-180.f, 0.f, 0.f));
	}
	else
	{
		Exhaust->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
	}
	Exhaust->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
	Exhaust->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.9f));
	Exhaust->SetMaterial(0, BoatArt::GlowMaterial(Exhaust, FLinearColor(12.f, 5.f, 1.f)));

	const UWorld* World = GetWorld();
	if (World != nullptr && World->GetNetMode() != NM_DedicatedServer)
	{
		BoatAudio::PlayAt(this, EBoatSound::MissileLaunch, GetActorLocation(), 1.f, FMath::FRandRange(0.95f, 1.1f));
		UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		for (int32 Index = 0; Index < 16; ++Index)
		{
			UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
			Puff->SetStaticMesh(Sphere);
			Puff->SetupAttachment(Body);
			Puff->SetUsingAbsoluteLocation(true);
			Puff->SetUsingAbsoluteScale(true);
			Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Puff->SetCastShadow(false);
			Puff->RegisterComponent();
			UMaterialInstanceDynamic* Material = BoatArt::SoftMaterial(Puff, FLinearColor(0.7f, 0.7f, 0.72f), 0.f);
			Puff->SetMaterial(0, Material);
			Puff->SetVisibility(false);
			Trail.Add(Puff);
			TrailMaterials.Add(Material);
			TrailAge.Add(100.f);
		}
	}
}

void ABoatMissile::Launch(ABoatPawn* Shooter)
{
	FiredBy = Shooter;
	// Lock on to the nearest boat in a cone ahead.
	const FVector Forward = GetActorForwardVector();
	float Best = LockRange;
	for (TActorIterator<ABoatPawn> It(GetWorld()); It; ++It)
	{
		ABoatPawn* Boat = *It;
		if (Boat == Shooter || Boat->IsWrecked())
		{
			continue;
		}
		const FVector To = Boat->GetRaceLocation() - GetActorLocation();
		const float Distance = static_cast<float>(To.Size2D());
		if (Distance < Best && FVector::DotProduct(To.GetSafeNormal2D(), Forward.GetSafeNormal2D()) > 0.7f)
		{
			Best = Distance;
			Target = Boat;
		}
	}
}

void ABoatMissile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	const float Seconds = WeaponServerNow(GetWorld());
	const FVector Location = GetActorLocation();

	// Smoke trail and a flickering exhaust on every machine.
	Exhaust->SetRelativeScale3D(FVector(0.25f, 0.25f, 0.7f + 0.4f * FMath::FRand()));
	TrailClock -= DeltaSeconds;
	if (Trail.Num() > 0 && TrailClock <= 0.f)
	{
		TrailClock = 0.05f;
		Trail[NextTrail]->SetWorldLocation(Location - GetActorForwardVector() * 150.f);
		Trail[NextTrail]->SetVisibility(true);
		TrailAge[NextTrail] = 0.f;
		NextTrail = (NextTrail + 1) % Trail.Num();
	}
	for (int32 Index = 0; Index < Trail.Num(); ++Index)
	{
		TrailAge[Index] += DeltaSeconds;
		const float U = TrailAge[Index] / 0.8f;
		if (U >= 1.f)
		{
			Trail[Index]->SetVisibility(false);
			continue;
		}
		Trail[Index]->SetWorldScale3D(FVector(0.5f + 2.f * U));
		BoatArt::SetPuff(TrailMaterials[Index], FLinearColor(0.75f, 0.75f, 0.78f), 0.5f * (1.f - U));
	}

	if (!HasAuthority() || bExploded)
	{
		return;
	}

	// Flares: a target that has fired them throws off the lock.
	if (Target != nullptr && !bDecoyed && Target->IsFlaring() && FVector::Dist(Location, Target->GetRaceLocation()) < 25000.f)
	{
		bDecoyed = true;
		Decoy = Target->GetRaceLocation() - Target->GetActorForwardVector() * 1500.f + FVector(0.f, 0.f, 600.f);
		Target = nullptr;
	}

	// Turn towards the target (or the decoy), limited by the turn rate.
	FVector Aim = GetActorForwardVector();
	if (Age > 0.3f)
	{
		if (Target != nullptr && !Target->IsWrecked())
		{
			Aim = (Target->GetRaceLocation() + FVector(0.f, 0.f, 120.f) - Location).GetSafeNormal();
		}
		else if (bDecoyed)
		{
			Aim = (Decoy - Location).GetSafeNormal();
		}
	}
	const FVector Current = GetActorForwardVector();
	const float MaxTurn = FMath::DegreesToRadians(TurnRate) * DeltaSeconds;
	const float Angle = FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(Current, Aim)), -1.f, 1.f));
	const FVector Heading = Angle > MaxTurn
		? FMath::Lerp(Current, Aim, MaxTurn / FMath::Max(Angle, 1.e-3f)).GetSafeNormal() : Aim;

	FVector Next = Location + Heading * Speed * DeltaSeconds;
	// Sea-skimming: never lower than a couple of metres over the waves.
	const float Surface = OceanWaves::GetHeight(FVector2D(Next.X, Next.Y), Seconds);
	Next.Z = FMath::Max(Next.Z, Surface + 200.f);
	SetActorLocationAndRotation(Next, Heading.Rotation());

	const bool bNearTarget = Target != nullptr && FVector::Dist(Next, Target->GetRaceLocation()) < 450.f;
	const bool bAtDecoy = bDecoyed && FVector::Dist(Next, Decoy) < 600.f;
	const bool bIntoLand = BayTerrain::GetHeight(FVector2D(Next.X, Next.Y)) > Next.Z - 100.f;
	if (bNearTarget || bAtDecoy || bIntoLand || Age > 7.f)
	{
		Explode();
	}
}

void ABoatMissile::Explode()
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	BoatBlast::Detonate(GetWorld(), GetActorLocation(), BlastRadius, bDecoyed ? 0.f : Damage, 900000.f, FiredBy);
	Destroy();
}

// ===========================================================================
// Flares
// ===========================================================================
AFlareBurst::AFlareBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	InitialLifeSpan = 3.5f;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetLightColor(FLinearColor(1.f, 0.85f, 0.6f));
	Light->SetAttenuationRadius(4000.f);
	Light->SetIntensity(80000.f);
	Light->SetCastShadows(false);
}

void AFlareBurst::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	BoatAudio::PlayAt(this, EBoatSound::Flare, GetActorLocation(), 0.8f, 1.6f);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	FRandomStream Random(GetUniqueID());
	for (int32 Index = 0; Index < 8; ++Index)
	{
		UStaticMeshComponent* Ball = NewObject<UStaticMeshComponent>(this);
		Ball->SetStaticMesh(Sphere);
		Ball->SetupAttachment(GetRootComponent());
		Ball->SetUsingAbsoluteLocation(true);
		Ball->SetWorldScale3D(FVector(0.35f));
		Ball->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Ball->SetCastShadow(false);
		Ball->RegisterComponent();
		Ball->SetMaterial(0, BoatArt::GlowMaterial(Ball, FLinearColor(30.f, 22.f, 12.f)));
		Ball->SetWorldLocation(GetActorLocation());
		Balls.Add(Ball);
		const float Angle = Random.FRandRange(-1.2f, 1.2f) + PI;
		Velocities.Add(GetActorRotation().RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f)) * Random.FRandRange(900.f, 1600.f)
			+ FVector(0.f, 0.f, Random.FRandRange(900.f, 1500.f)));
	}
}

void AFlareBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	for (int32 Index = 0; Index < Balls.Num(); ++Index)
	{
		Velocities[Index].Z -= 600.f * DeltaSeconds; // they float down slowly, burning
		Balls[Index]->SetWorldLocation(Balls[Index]->GetComponentLocation() + Velocities[Index] * DeltaSeconds);
		Balls[Index]->SetWorldScale3D(FVector(0.35f * (1.f - Age / 3.5f) + 0.05f));
	}
	Light->SetIntensity(80000.f * FMath::Max(1.f - Age / 3.5f, 0.f) * (0.8f + 0.2f * FMath::FRand()));
	if (Balls.Num() > 0)
	{
		Light->SetWorldLocation(Balls[0]->GetComponentLocation());
	}
}

// ===========================================================================
// Grenade
// ===========================================================================
ABoatGrenade::ABoatGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 5.f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(Sphere.Object);
	Body->SetRelativeScale3D(FVector(0.32f, 0.32f, 0.4f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fuse = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Fuse"));
	Fuse->SetupAttachment(Body);
	Fuse->SetStaticMesh(Sphere.Object);
	Fuse->SetRelativeLocation(FVector(0.f, 0.f, 55.f));
	Fuse->SetRelativeScale3D(FVector(0.35f));
	Fuse->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABoatGrenade::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Basic = BoatArt::Material(TEXT("M_Surface")))
	{
		UMaterialInstanceDynamic* Olive = UMaterialInstanceDynamic::Create(Basic, Body);
		Olive->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.18f, 0.06f));
		Olive->SetScalarParameterValue(TEXT("Roughness"), 0.6f);
		Body->SetMaterial(0, Olive);
	}
	Fuse->SetMaterial(0, BoatArt::GlowMaterial(Fuse, FLinearColor(12.f, 2.f, 0.4f)));
	BoatAudio::PlayAt(this, EBoatSound::Grenade, GetActorLocation(), 0.8f);
}

void ABoatGrenade::Throw(ABoatPawn* Thrower)
{
	FiredBy = Thrower;
	// The co-driver lobs it forward and out to one side, on top of the boat's own speed.
	const float Side = FMath::RandBool() ? 1.f : -1.f;
	const FVector BoatVelocity = Thrower->GetActorForwardVector() * Thrower->GetForwardSpeed();
	Velocity = BoatVelocity + Thrower->GetActorForwardVector() * 1700.f + Thrower->GetActorRightVector() * (Side * 500.f)
		+ FVector(0.f, 0.f, 1100.f);
}

void ABoatGrenade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	AddActorWorldRotation(FRotator(400.f * DeltaSeconds, 250.f * DeltaSeconds, 0.f));
	if (!HasAuthority() || bExploded)
	{
		return;
	}
	Velocity.Z -= 980.f * DeltaSeconds;
	const FVector Next = GetActorLocation() + Velocity * DeltaSeconds;
	SetActorLocation(Next);
	const float Surface = OceanWaves::GetHeight(FVector2D(Next.X, Next.Y), WeaponServerNow(GetWorld()));
	if ((Next.Z < Surface + 30.f && Age > 0.3f) || Age > 3.f)
	{
		bExploded = true;
		BoatBlast::Detonate(GetWorld(), Next, BlastRadius, Damage, 900000.f, FiredBy);
		Destroy();
	}
}
