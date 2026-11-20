#include "JetScenery.h"
#include "JetTerrain.h"
#include "JetGame.h"
#include "JetFighter.h"
#include "OceanWaves.h"
#include "BoatArt.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AJetScenery::AJetScenery()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.2f;
    Terrain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Islands"));
    SetRootComponent(Terrain);
    // Contact is analytic, against these exact triangles. Cooking a second
    // physics mesh costs memory and is unnecessary for a kinematic aircraft.
    Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Terrain->SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto Make = [this](const TCHAR* Name, UStaticMesh* Mesh)
    {
        auto* C = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
        C->SetupAttachment(Terrain);
        C->SetStaticMesh(Mesh);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetCastShadow(false);
        return C;
    };
    Runway = Make(TEXT("Runway"), Cube.Object);
    Markings = Make(TEXT("RunwayPaint"), Cube.Object);
    Beacons = Make(TEXT("NavigationBeacons"), Cube.Object);
    Clouds = Make(TEXT("CloudPuffs"), Sphere.Object);
    Beacons->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Beacons->SetCollisionObjectType(ECC_WorldStatic);
    Beacons->SetCollisionResponseToAllChannels(ECR_Block);
    Beacons->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
}

void AJetScenery::BeginPlay()
{
    Super::BeginPlay();
    BuildTerrain();
    BuildAirstrip();
    BuildClouds();
    UE_LOG(LogJetFighter, Log, TEXT("Jet scenery: %d terrain triangles, %d cloud puffs, %d landmarks"),
        JetTerrain::Cells*JetTerrain::Cells*2, CloudStarts.Num(), JetTerrain::Landmarks().Num());
}

void AJetScenery::BuildTerrain()
{
    const int32 Side = JetTerrain::Cells+1;
    TArray<FVector> Verts, Normals;
    TArray<int32> Indices;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    Verts.Reserve(Side*Side);
    for (int32 Y = 0; Y < Side; ++Y)
        for (int32 X = 0; X < Side; ++X)
        {
            const double MX = X*JetTerrain::Cell-JetTerrain::HalfWidth;
            const double MY = Y*JetTerrain::Cell-JetTerrain::HalfWidth;
            const double H = JetTerrain::VertexHeightMetres(MX, MY);
            const double DX = (JetTerrain::VertexHeightMetres(MX+1, MY)-JetTerrain::VertexHeightMetres(MX-1, MY))*0.5;
            const double DY = (JetTerrain::VertexHeightMetres(MX, MY+1)-JetTerrain::VertexHeightMetres(MX, MY-1))*0.5;
            const FVector Normal = FVector(-DX, -DY, 1).GetSafeNormal();
            Verts.Add(FVector(MX*100., MY*100., H*100.));
            Normals.Add(Normal);
            UVs.Add(FVector2D(MX/100., MY/100.));
            Tangents.Add(FProcMeshTangent(FVector(1, 0, DX).GetSafeNormal(), false));
            const float Rock = FMath::Clamp((1.-Normal.Z)*2.5 + (H-450.)/650., 0., 1.);
            const FLinearColor Grass(0.13f, 0.24f, 0.07f), Stone(0.32f, 0.28f, 0.22f), Sand(0.6f, 0.47f, 0.26f);
            const FLinearColor Inland = FMath::Lerp(Grass, Stone, Rock);
            Colors.Add(FMath::Lerp(Sand, Inland, FMath::Clamp(H/55., 0., 1.)));
            if (X < Side-1 && Y < Side-1)
            {
                const int32 I = Y*Side+X;
                Indices.Append({I, I+Side, I+1, I+1, I+Side, I+Side+1});
            }
        }
    Terrain->CreateMeshSection_LinearColor(0, Verts, Indices, Normals, UVs, Colors, Tangents, false);
    UMaterialInterface* Material = BoatArt::Material(TEXT("M_VertexColor"));
    Terrain->SetMaterial(0, Material ? Material : BoatArt::BasicMaterial());
}

void AJetScenery::BuildAirstrip()
{
    auto Paint = [this](UInstancedStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        auto* Mat = UMaterialInstanceDynamic::Create(BoatArt::BasicMaterial(), this);
        Mat->SetVectorParameterValue(TEXT("Color"), Color);
        Mesh->SetMaterial(0, Mat);
    };
    Paint(Runway, FLinearColor(0.025f, 0.035f, 0.045f));
    Paint(Markings, FLinearColor(0.9f, 0.9f, 0.8f));
    Beacons->SetMaterial(0, BoatArt::GlowMaterial(this, FLinearColor(1.f, 0.35f, 0.04f)));
    auto Add = [](UInstancedStaticMeshComponent* Mesh, const FVector& Metres, const FVector& Scale)
    {
        Mesh->AddInstance(FTransform(FQuat::Identity, Metres*100., Scale));
    };
    Add(Runway, FVector(0, -2800, 220.1), FVector(1000, 70, 0.2));
    for (int32 I = -8; I <= 8; ++I) Add(Markings, FVector(I*50, -2800, 220.25), FVector(24, 2, 0.05));
    for (int32 End : {-1, 1})
        for (int32 Side : {-1, 1})
        {
            Add(Markings, FVector(End*460, -2800+Side*22, 220.25), FVector(25, 8, 0.05));
            Add(Beacons, FVector(End*490, -2800+Side*40, 222), FVector(3, 3, 4));
        }
    for (const auto& Point : JetTerrain::Landmarks())
    {
        if (Point.PositionMetres.Z < 300) continue;
        FVector P = Point.PositionMetres;
        P.Z = JetTerrain::HeightMetres(P.X, P.Y)+10.;
        Add(Beacons, P, FVector(5, 5, 20));
    }
}

void AJetScenery::BuildClouds()
{
    auto* Mat = UMaterialInstanceDynamic::Create(BoatArt::BasicMaterial(), this);
    Mat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.92f, 0.95f, 1.f));
    Clouds->SetMaterial(0, Mat);
    FRandomStream Random(CloudSeed);
    for (int32 Group = 0; Group < FMath::Clamp(CloudGroups, 0, 32); ++Group)
    {
        const FVector Centre(Random.FRandRange(-8500, 8500), Random.FRandRange(-8500, 8500), Random.FRandRange(1700, 2600));
        for (int32 Puff = 0; Puff < 3; ++Puff)
        {
            const FVector P = Centre+FVector(Puff*150, Random.FRandRange(-80, 80), Random.FRandRange(-20, 20));
            const FVector Scale(Random.FRandRange(350, 550), Random.FRandRange(200, 350), Random.FRandRange(100, 180));
            const FTransform T(FQuat::Identity, P*100., Scale);
            CloudStarts.Add(T);
            Clouds->AddInstance(T);
        }
    }
}

void AJetScenery::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* State = GetWorld()->GetGameState<AJetGameState>();
    const double Seconds = State ? State->GetServerNow() : GetWorld()->GetTimeSeconds();
    const double Shift = FMath::Fmod(Seconds*CloudWindMetres*100., 1800000.);
    for (int32 I = 0; I < CloudStarts.Num(); ++I)
    {
        FTransform T = CloudStarts[I];
        FVector P = T.GetLocation();
        P.X = FMath::Fmod(P.X+Shift+2700000., 1800000.)-900000.;
        T.SetLocation(P);
        Clouds->UpdateInstanceTransform(I, T, false, I == CloudStarts.Num()-1, true);
    }
}

float AJetWaterSurface::GridScaleForAltitude(float HeightCm, float BaseCellCm)
{
    const float Ratio = FMath::Max(1.f, HeightCm*0.08f/FMath::Max(BaseCellCm, 1.f));
    return FMath::Pow(2.f, FMath::Clamp(FMath::CeilToFloat(FMath::Log2(Ratio)), 0.f, 6.f));
}

void AJetWaterSurface::BeginPlay()
{
    BaseCellSize = CellSize;
    Super::BeginPlay();
    // Both surfaces must use the same tint, normal map and foam policy at
    // their join. The boat renderer deliberately darkens its far material.
    NearWater->SetMaterial(0, FarWater->GetMaterial(0));
}

float AJetWaterSurface::WaveDetailWeight(float HeightMetres, float GridRadius, float FadeHeightMetres)
{
    auto Smooth = [](float X) { X = FMath::Clamp(X, 0.f, 1.f); return X*X*(3.f-2.f*X); };
    const float HeightFade = Smooth((HeightMetres-100.f)/FMath::Max(FadeHeightMetres-100.f, 1.f));
    const float EdgeFade = Smooth((GridRadius-0.6f)/0.4f);
    return (1.f-HeightFade)*(1.f-EdgeFade);
}

void AJetWaterSurface::Tick(float DeltaSeconds)
{
    const auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) { Super::Tick(DeltaSeconds); return; }
    AActor::Tick(DeltaSeconds);
    SinceRefresh += DeltaSeconds;
    if (SinceRefresh < UpdateInterval) return;
    SinceRefresh = 0.f;
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    RefreshForView(Eye);
}

void AJetWaterSurface::RefreshForView(const FVector& Eye)
{
    CellSize = BaseCellSize*GridScaleForAltitude(Eye.Z, BaseCellSize);
    const FVector2D Centre(FMath::GridSnap(Eye.X, double(CellSize)), FMath::GridSnap(Eye.Y, double(CellSize)));
    FarWater->SetWorldLocation(FVector(Centre.X, Centre.Y, -140));
    const bool bDetailVisible = Eye.Z < WaveFadeHeightMetres*100.f;
    NearWater->SetVisibility(bDetailVisible);
    // A flat detail mesh on the same plane as the distant sea causes depth
    // fighting. Once waves are subpixel, draw only the distant surface.
    if (!bDetailVisible) return;
    const int32 Side = GridCells+1, Count = Side*Side;
    AirVertices.SetNumUninitialized(Count);
    AirNormals.SetNumUninitialized(Count);
    AirUVs.SetNumUninitialized(Count);
    AirTangents.SetNumUninitialized(Count);
    const double Half = GridCells*CellSize*0.5;
    const auto* State = GetWorld()->GetGameState<AJetGameState>();
    const double Seconds = State ? State->GetServerNow() : GetWorld()->GetTimeSeconds();
    for (int32 Y = 0; Y < Side; ++Y)
        for (int32 X = 0; X < Side; ++X)
        {
            const int32 I = Y*Side+X;
            const FVector2D XY(Centre.X-Half+X*CellSize, Centre.Y-Half+Y*CellSize);
            FVector2D Slope;
            const float Height = OceanWaves::GetHeightAndSlope(XY, Seconds, Slope);
            const float Radius = FMath::Max(FMath::Abs(float(X)*2.f/GridCells-1.f), FMath::Abs(float(Y)*2.f/GridCells-1.f));
            const float Weight = WaveDetailWeight(Eye.Z/100.f, Radius, WaveFadeHeightMetres);
            // Subpixel waves should not become hundred-metre patches after
            // sparse sampling. At the edge, settle just above the distant
            // plane; low, central flight retains the original wave function.
            AirVertices[I] = FVector(XY.X, XY.Y, FMath::Lerp(-100.f, Height, Weight))-GetActorLocation();
            AirNormals[I] = FVector(-Slope.X*Weight, -Slope.Y*Weight, 1).GetSafeNormal();
            AirUVs[I] = XY/1000.;
            AirTangents[I] = FProcMeshTangent(FVector(1, 0, Slope.X*Weight).GetSafeNormal(), false);
        }
    NearWater->UpdateMeshSection_LinearColor(0, AirVertices, AirNormals, AirUVs, TArray<FLinearColor>(), AirTangents);
}
