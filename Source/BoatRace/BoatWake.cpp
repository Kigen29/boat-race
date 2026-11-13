#include "BoatWake.h"

#include "BoatArt.h"
#include "OceanWaves.h"

#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const float SampleSpacing = 150.f;  // cm between samples
	const float Life = 5.5f;            // seconds a patch of foam lasts
	const int32 MaxSamples = 70;
	/** tan(19.47 degrees): the half-angle of a Kelvin wake. */
	const float KelvinSpread = 0.354f;
}

void FBoatWakeTrail::Init(AActor* Owner)
{
	if (Owner == nullptr || Mesh != nullptr)
	{
		return;
	}
	Mesh = NewObject<UProceduralMeshComponent>(Owner);
	Mesh->SetupAttachment(Owner->GetRootComponent());
	Mesh->SetUsingAbsoluteLocation(true);
	Mesh->SetUsingAbsoluteRotation(true);
	Mesh->SetUsingAbsoluteScale(true);
	Mesh->SetWorldLocation(FVector::ZeroVector);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = true;
	Mesh->RegisterComponent();

	UMaterialInterface* Foam = BoatArt::Material(TEXT("M_Foam"));
	Material = Foam ? UMaterialInstanceDynamic::Create(Foam, Mesh) : BoatArt::SoftMaterial(Mesh, FLinearColor::White, 0.5f);
	if (Material != nullptr)
	{
		Material->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
		Mesh->SetMaterial(0, Material);
	}
}

void FBoatWakeTrail::Update(const FVector& Stern, const FVector& Forward, float Speed, bool bInWater, float WaveTime,
	float Now, float Brightness, float Scale)
{
	if (Mesh == nullptr)
	{
		return;
	}
	if (Material != nullptr && !FMath::IsNearlyEqual(Brightness, LastBrightness, 0.01f))
	{
		LastBrightness = Brightness;
		Material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.9f, 0.95f, 1.f) * Brightness);
	}

	const FVector2D Here(Stern.X, Stern.Y);
	const FVector2D Ahead = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	const FVector2D Right(-Ahead.Y, Ahead.X);
	// White water only once the hull is moving properly, and only in the water.
	const float Strength = bInWater ? FMath::Clamp((Speed - 250.f) / 900.f, 0.f, 1.f) : 0.f;

	if (Samples.Num() == 0 || FVector2D::Distance(Samples[0].Position, Here) > SampleSpacing)
	{
		if (Samples.Num() > 0 && FVector2D::Distance(Samples[0].Position, Here) > SampleSpacing * 8.f)
		{
			Samples.Reset(); // teleported: do not draw a wake across the bay
		}
		FSample Sample;
		Sample.Position = Here;
		Sample.Right = Right;
		Sample.Speed = Speed;
		Sample.Time = Now;
		Sample.Strength = Strength;
		Samples.Insert(Sample, 0);
	}
	while (Samples.Num() > MaxSamples || (Samples.Num() > 0 && Now - Samples.Last().Time > Life))
	{
		Samples.Pop();
	}

	// The live point at the transom, then the laid-down samples behind it.
	TArray<FSample> Points;
	Points.Reserve(Samples.Num() + 1);
	FSample Live;
	Live.Position = Here;
	Live.Right = Right;
	Live.Speed = Speed;
	Live.Time = Now;
	Live.Strength = Strength;
	Points.Add(Live);
	Points.Append(Samples);

	if (Points.Num() < 2)
	{
		Mesh->ClearAllMeshSections();
		return;
	}

	TArray<FVector> Vertices;
	TArray<FLinearColor> Colours;
	TArray<int32> Triangles;
	const int32 PerPoint = 9; // three strips of three: arm, centre, arm
	Vertices.Reserve(Points.Num() * PerPoint);
	Colours.Reserve(Points.Num() * PerPoint);

	auto Put = [&](const FVector2D& P, float Alpha)
	{
		const float Surface = OceanWaves::GetHeight(P, WaveTime);
		Vertices.Add(FVector(P.X, P.Y, Surface + 6.f));
		Colours.Add(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Alpha, 0.f, 1.f)));
	};

	for (const FSample& Point : Points)
	{
		const float Age = Now - Point.Time;
		const float Fade = FMath::Pow(FMath::Clamp(1.f - Age / Life, 0.f, 1.f), 1.4f) * Point.Strength;
		// Churned water right behind the boat spreads as it ages.
		const float Half = (80.f + Age * 110.f) * Scale;
		// The Kelvin arms run outwards with the distance travelled since.
		const float Travelled = FMath::Max(Age * Point.Speed, 0.f);
		const float Arm = (60.f + Travelled * KelvinSpread) * Scale;
		const float ArmHalf = (45.f + Age * 35.f) * Scale;
		const float ArmAlpha = Fade * 0.55f * FMath::Clamp(Age * 3.f, 0.f, 1.f);

		Put(Point.Position - Point.Right * (Arm + ArmHalf), 0.f);
		Put(Point.Position - Point.Right * Arm, ArmAlpha);
		Put(Point.Position - Point.Right * FMath::Max(Arm - ArmHalf, Half), 0.f);
		Put(Point.Position - Point.Right * Half, 0.f);
		Put(Point.Position, Fade * FMath::Clamp(1.2f - Age * 0.15f, 0.f, 1.f));
		Put(Point.Position + Point.Right * Half, 0.f);
		Put(Point.Position + Point.Right * FMath::Max(Arm - ArmHalf, Half), 0.f);
		Put(Point.Position + Point.Right * Arm, ArmAlpha);
		Put(Point.Position + Point.Right * (Arm + ArmHalf), 0.f);
	}

	for (int32 Row = 0; Row + 1 < Points.Num(); ++Row)
	{
		const int32 A = Row * PerPoint;
		const int32 B = (Row + 1) * PerPoint;
		for (int32 Column : { 0, 1, 3, 4, 6, 7 })
		{
			Triangles.Append({ A + Column, B + Column, A + Column + 1, A + Column + 1, B + Column, B + Column + 1 });
		}
	}

	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, TArray<FVector>(), TArray<FVector2D>(), Colours,
		TArray<FProcMeshTangent>(), false);
	if (Material != nullptr && Mesh->GetMaterial(0) != Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}
