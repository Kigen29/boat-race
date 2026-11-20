#include "JetTrainingTarget.h"
#include "JetHealthComponent.h"
#include "JetFighter.h"
#include "BoatArt.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"

AJetTrainingTarget::AJetTrainingTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    HitVolume = CreateDefaultSubobject<USphereComponent>(TEXT("DroneHitVolume"));
    SetRootComponent(HitVolume);
    HitVolume->InitSphereRadius(600.f);
    HitVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    HitVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    HitVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Model = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneModel"));
    Model->SetupAttachment(RootComponent);
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Cone.Succeeded()) Model->SetStaticMesh(Cone.Object);
    Model->SetRelativeRotation(FRotator(-90,0,0));
    Model->SetRelativeScale3D(FVector(3,3,10));
    Blast = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DestructionFlash"));
    Blast->SetupAttachment(RootComponent);
    if (Sphere.Succeeded()) Blast->SetStaticMesh(Sphere.Object);
    Blast->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Blast->SetVisibility(false);
    for (int32 I=0; I<8; ++I)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Debris%d"),I));
        Part->SetupAttachment(RootComponent);
        if (Cone.Succeeded()) Part->SetStaticMesh(Cone.Object);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetVisibility(false);
        Debris.Add(Part);
    }
    Health = CreateDefaultSubobject<UJetHealthComponent>(TEXT("Hull"));
}

void AJetTrainingTarget::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Mesh = LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Aircraft/SM_JetRook.SM_JetRook"),nullptr,LOAD_NoWarn|LOAD_Quiet))
    {
        Model->SetStaticMesh(Mesh);
        Model->SetRelativeRotation(FRotator::ZeroRotator);
        Model->SetRelativeScale3D(FVector(1200.f/FMath::Max(1.f,float(Mesh->GetBoundingBox().GetSize().X))));
    }
    auto Paint = [](UStaticMeshComponent* Part, const FLinearColor& Color)
    {
        for (int32 I=0; I<Part->GetNumMaterials(); ++I)
            if (auto* M=Part->CreateDynamicMaterialInstance(I,BoatArt::BasicMaterial())) M->SetVectorParameterValue(TEXT("Color"),Color);
    };
    Paint(Model,FLinearColor(0.85,0.22,0.035));
    Blast->SetMaterial(0,BoatArt::GlowMaterial(Blast,FLinearColor(4,1.5,0.08)));
    Blast->SetCastShadow(false);
    for (const auto& Part:Debris) Paint(Part,FLinearColor(0.15,0.1,0.07));
}

bool AJetTrainingTarget::ReceiveShot(float Damage, int32 SourceTeam, uint64 ShotId, const FVector& Impact)
{
    if (!Health->ApplyHit(Damage,SourceTeam,ShotId)) return false;
    HitFlashTime=0.12f;
    Blast->SetWorldLocation(Impact);
    Blast->SetRelativeScale3D(FVector(1.4));
    Blast->SetVisibility(true);
    if (Health->IsDestroyed())
    {
        Blast->SetRelativeLocation(FVector::ZeroVector);
        HitVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Model->SetVisibility(false);
        Blast->SetVisibility(true);
        for (const auto& Part:Debris) Part->SetVisibility(true);
        UE_LOG(LogJetFighter,Log,TEXT("Jet training: drone destroyed by shot %llu"),ShotId);
    }
    return true;
}

void AJetTrainingTarget::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!Health->IsDestroyed())
    {
        HitFlashTime=FMath::Max(0.f,HitFlashTime-Dt);
        Blast->SetVisibility(HitFlashTime>0.f);
        return;
    }
    DestructionTime += Dt;
    Blast->SetVisibility(DestructionTime < 0.65f);
    Blast->SetRelativeScale3D(FVector(2.f+DestructionTime*18.f));
    for (int32 I=0; I<Debris.Num(); ++I)
    {
        const float A=I*2.f*PI/Debris.Num();
        auto* Part=Debris[I].Get();
        Part->SetVisibility(DestructionTime<3.f);
        Part->SetRelativeLocation(FVector(FMath::Cos(A)*2200.f*DestructionTime,
            FMath::Sin(A)*2200.f*DestructionTime,700.f*DestructionTime-490.f*DestructionTime*DestructionTime));
        Part->SetRelativeRotation(FRotator(DestructionTime*130.f,I*45.f+DestructionTime*80.f,DestructionTime*170.f));
        Part->SetRelativeScale3D(FVector(0.4,0.8,1.8));
    }
}
