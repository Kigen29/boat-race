#include "OceanWaves.h"

namespace OceanWaves
{
	float Roughness = 1.f;
}

namespace
{
	/**
	 * Amplitude (cm), wavelength (cm) and heading (degrees) of each component.
	 *
	 * Amplitudes add in quadrature, so these six give a standard deviation of
	 * about 48 cm and a significant wave height near 1.9 m: steep enough to
	 * throw a 7 m boat around without making the course unraceable. The spread
	 * of headings is what stops it looking like a rolling corduroy pattern.
	 */
	const double WaveTable[][3] =
	{
		{ 55.0, 9000.0,   15.0 },  // long swell
		{ 32.0, 5200.0,  -35.0 },
		{ 20.0, 3100.0,   62.0 },
		{ 12.0, 1700.0,  -78.0 },
		{  7.0,  900.0,  124.0 },
		{  4.0,  520.0, -151.0 }   // short chop
	};

	TArray<FOceanWave> MakeWaves()
	{
		TArray<FOceanWave> Waves;
		const int32 Count = UE_ARRAY_COUNT(WaveTable);
		Waves.Reserve(Count);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double HeadingRadians = FMath::DegreesToRadians(WaveTable[Index][2]);

			FOceanWave Wave;
			Wave.Amplitude = static_cast<float>(WaveTable[Index][0]);
			Wave.Wavelength = static_cast<float>(WaveTable[Index][1]);
			Wave.Direction = FVector2D(FMath::Cos(HeadingRadians), FMath::Sin(HeadingRadians));
			Wave.WaveNumber = static_cast<float>(2.0 * PI / WaveTable[Index][1]);
			// Deep-water dispersion: omega = sqrt(g k). Long waves move faster.
			Wave.AngularSpeed = FMath::Sqrt(OceanWaves::Gravity * Wave.WaveNumber);
			// An irrational spread so the components never line up into one big peak.
			Wave.Phase = static_cast<float>(Index) * 1.7f + 0.37f;
			Waves.Add(Wave);
		}
		return Waves;
	}
}

void OceanWaves::SetRoughness(float NewRoughness)
{
	Roughness = FMath::Clamp(NewRoughness, 0.f, 3.f);
}

const TArray<FOceanWave>& OceanWaves::GetWaves()
{
	static const TArray<FOceanWave> Waves = MakeWaves();
	return Waves;
}

float OceanWaves::GetHeight(const FVector2D& WorldXY, float Seconds)
{
	if (Roughness <= 0.f)
	{
		return MeanLevel;
	}

	double Height = 0.0;
	for (const FOceanWave& Wave : GetWaves())
	{
		const double Along = WorldXY.X * Wave.Direction.X + WorldXY.Y * Wave.Direction.Y;
		Height += Wave.Amplitude * FMath::Sin(Along * Wave.WaveNumber - Wave.AngularSpeed * Seconds + Wave.Phase);
	}
	return MeanLevel + static_cast<float>(Height) * Roughness;
}

float OceanWaves::GetHeightAndSlope(const FVector2D& WorldXY, float Seconds, FVector2D& OutSlope)
{
	OutSlope = FVector2D::ZeroVector;
	if (Roughness <= 0.f)
	{
		return MeanLevel;
	}

	double Height = 0.0;
	double SlopeX = 0.0;
	double SlopeY = 0.0;
	for (const FOceanWave& Wave : GetWaves())
	{
		const double Along = WorldXY.X * Wave.Direction.X + WorldXY.Y * Wave.Direction.Y;
		const double Angle = Along * Wave.WaveNumber - Wave.AngularSpeed * Seconds + Wave.Phase;
		Height += Wave.Amplitude * FMath::Sin(Angle);

		// d/dx of A sin(k (d . x) - wt) = A k dx cos(...)
		const double Gradient = Wave.Amplitude * Wave.WaveNumber * FMath::Cos(Angle);
		SlopeX += Gradient * Wave.Direction.X;
		SlopeY += Gradient * Wave.Direction.Y;
	}

	OutSlope = FVector2D(SlopeX * Roughness, SlopeY * Roughness);
	return MeanLevel + static_cast<float>(Height) * Roughness;
}

FVector OceanWaves::GetNormal(const FVector2D& WorldXY, float Seconds)
{
	FVector2D Slope;
	GetHeightAndSlope(WorldXY, Seconds, Slope);
	// The surface z = h(x, y) has normal (-dh/dx, -dh/dy, 1).
	return FVector(-Slope.X, -Slope.Y, 1.0).GetSafeNormal();
}

float OceanWaves::GetVerticalSpeed(const FVector2D& WorldXY, float Seconds)
{
	if (Roughness <= 0.f)
	{
		return 0.f;
	}

	double Speed = 0.0;
	for (const FOceanWave& Wave : GetWaves())
	{
		const double Along = WorldXY.X * Wave.Direction.X + WorldXY.Y * Wave.Direction.Y;
		const double Angle = Along * Wave.WaveNumber - Wave.AngularSpeed * Seconds + Wave.Phase;
		// d/dt of A sin(... - wt) = -A w cos(...)
		Speed += -Wave.Amplitude * Wave.AngularSpeed * FMath::Cos(Angle);
	}
	return static_cast<float>(Speed) * Roughness;
}

float OceanWaves::GetMaxHeight()
{
	double Total = 0.0;
	for (const FOceanWave& Wave : GetWaves())
	{
		Total += Wave.Amplitude;
	}
	return static_cast<float>(Total) * FMath::Max(Roughness, 1.f);
}
