#include "BoatTrack.h"

#include "BoatTrackData.h"

namespace
{
	const double MetresToUU = 100.0;

	double Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double Dot2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.X + A.Y * B.Y;
	}

	FVector2D FromMetres(double X, double Y)
	{
		return FVector2D(X * MetresToUU, Y * MetresToUU);
	}

	TArray<FBoatGate> MakeGateList()
	{
		const int32 Count = UE_ARRAY_COUNT(BoatTrackData::RacingLine);

		TArray<FVector2D> Points;
		Points.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Points.Add(FromMetres(BoatTrackData::RacingLine[Index][0], BoatTrackData::RacingLine[Index][1]));
		}

		const double HalfWidth = BoatTrackData::GateHalfWidthMetres * MetresToUU;

		TArray<FBoatGate> Gates;
		Gates.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D& Prev = Points[(Index + Count - 1) % Count];
			const FVector2D& Next = Points[(Index + 1) % Count];

			FBoatGate Gate;
			Gate.Center = Points[Index];
			// Averaging the neighbours puts the gate square to the racing line
			// through the corner rather than square to one straight.
			Gate.Forward = (Next - Prev).GetSafeNormal();
			// The racing line runs counter-clockwise, so this perpendicular
			// points at the inside of the circuit.
			const FVector2D ToInside(-Gate.Forward.Y, Gate.Forward.X);
			Gate.Inner = Gate.Center + ToInside * HalfWidth;
			Gate.Outer = Gate.Center - ToInside * HalfWidth;
			Gates.Add(Gate);
		}
		return Gates;
	}

}

const TArray<FBoatGate>& BoatTrack::GetGates()
{
	static const TArray<FBoatGate> Gates = MakeGateList();
	return Gates;
}

int32 BoatTrack::NumGates()
{
	return GetGates().Num();
}

float BoatTrack::GetGateHalfWidth()
{
	return static_cast<float>(BoatTrackData::GateHalfWidthMetres * MetresToUU);
}

float BoatTrack::GetLapLength()
{
	static const float Length = []()
	{
		const TArray<FBoatGate>& Gates = GetGates();
		double Total = 0.0;
		for (int32 Index = 0; Index < Gates.Num(); ++Index)
		{
			Total += (Gates[(Index + 1) % Gates.Num()].Center - Gates[Index].Center).Size();
		}
		return static_cast<float>(Total);
	}();
	return Length;
}

float BoatTrack::GetDistanceToGate(int32 GateIndex)
{
	// Cumulative distance from the start line, used to rank boats and to work
	// out how many seconds apart they are.
	static const TArray<float> Distances = []()
	{
		const TArray<FBoatGate>& Gates = GetGates();
		TArray<float> Result;
		Result.Reserve(Gates.Num());
		double Running = 0.0;
		for (int32 Index = 0; Index < Gates.Num(); ++Index)
		{
			Result.Add(static_cast<float>(Running));
			Running += (Gates[(Index + 1) % Gates.Num()].Center - Gates[Index].Center).Size();
		}
		return Result;
	}();

	const int32 Index = ((GateIndex % Distances.Num()) + Distances.Num()) % Distances.Num();
	return Distances[Index];
}

const TArray<FBoatCircle>& BoatTrack::GetRocks()
{
	static const TArray<FBoatCircle> Rocks = []()
	{
		TArray<FBoatCircle> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Rocks); ++Index)
		{
			FBoatCircle Rock;
			Rock.Center = FromMetres(BoatTrackData::Rocks[Index][0], BoatTrackData::Rocks[Index][1]);
			Rock.Radius = static_cast<float>(BoatTrackData::Rocks[Index][2] * MetresToUU);
			Result.Add(Rock);
		}
		return Result;
	}();
	return Rocks;
}

const TArray<FBoatRamp>& BoatTrack::GetRamps()
{
	static const TArray<FBoatRamp> Ramps = []()
	{
		TArray<FBoatRamp> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Ramps); ++Index)
		{
			FBoatRamp Ramp;
			Ramp.Center = FromMetres(BoatTrackData::Ramps[Index][0], BoatTrackData::Ramps[Index][1]);
			Ramp.Yaw = static_cast<float>(BoatTrackData::Ramps[Index][2]);
			Result.Add(Ramp);
		}
		return Result;
	}();
	return Ramps;
}

const TArray<FBoatWhirlpool>& BoatTrack::GetWhirlpools()
{
	static const TArray<FBoatWhirlpool> Pools = []()
	{
		TArray<FBoatWhirlpool> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Whirlpools); ++Index)
		{
			FBoatWhirlpool Pool;
			Pool.Center = FromMetres(BoatTrackData::Whirlpools[Index][0], BoatTrackData::Whirlpools[Index][1]);
			Pool.Radius = static_cast<float>(BoatTrackData::Whirlpools[Index][2] * MetresToUU);
			Result.Add(Pool);
		}
		return Result;
	}();
	return Pools;
}

const TArray<FVector2D>& BoatTrack::GetCratePoints()
{
	static const TArray<FVector2D> Points = []()
	{
		TArray<FVector2D> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Crates); ++Index)
		{
			Result.Add(FromMetres(BoatTrackData::Crates[Index][0], BoatTrackData::Crates[Index][1]));
		}
		return Result;
	}();
	return Points;
}

const TArray<FVector2D>& BoatTrack::GetDebrisPoints()
{
	static const TArray<FVector2D> Points = []()
	{
		TArray<FVector2D> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Debris); ++Index)
		{
			Result.Add(FromMetres(BoatTrackData::Debris[Index][0], BoatTrackData::Debris[Index][1]));
		}
		return Result;
	}();
	return Points;
}

const TArray<FBoatBarge>& BoatTrack::GetBarges()
{
	static const TArray<FBoatBarge> Barges = []()
	{
		TArray<FBoatBarge> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Barges); ++Index)
		{
			FBoatBarge Barge;
			Barge.From = FromMetres(BoatTrackData::Barges[Index][0], BoatTrackData::Barges[Index][1]);
			Barge.To = FromMetres(BoatTrackData::Barges[Index][2], BoatTrackData::Barges[Index][3]);
			Barge.Period = FMath::Max(static_cast<float>(BoatTrackData::Barges[Index][4]), 1.f);
			Result.Add(Barge);
		}
		return Result;
	}();
	return Barges;
}

const TArray<FBoatLog>& BoatTrack::GetLogs()
{
	static const TArray<FBoatLog> Logs = []()
	{
		TArray<FBoatLog> Result;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoatTrackData::Logs); ++Index)
		{
			FBoatLog Log;
			Log.Center = FromMetres(BoatTrackData::Logs[Index][0], BoatTrackData::Logs[Index][1]);
			Log.Yaw = static_cast<float>(BoatTrackData::Logs[Index][2]);
			Result.Add(Log);
		}
		return Result;
	}();
	return Logs;
}

FVector2D BoatTrack::GetBargeLocation(const FBoatBarge& Barge, float Seconds)
{
	// A triangle wave over the period: out, then back, for ever. Derived from
	// the shared clock, so every machine puts the barge in the same place
	// without a single byte of network traffic.
	const float Cycle = FMath::Frac(FMath::Max(Seconds, 0.f) / Barge.Period);
	const float Alpha = (Cycle < 0.5f) ? (Cycle * 2.f) : (2.f - Cycle * 2.f);
	// Ease in and out so it does not snap direction at the ends of the run.
	const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
	return Barge.From + (Barge.To - Barge.From) * static_cast<double>(Smooth);
}

FTransform BoatTrack::GetGridTransform(int32 Slot)
{
	const FBoatGate& Start = GetGates()[0];
	const int32 SafeSlot = FMath::Clamp(Slot, 0, MaxPlayers - 1);
	const int32 Row = SafeSlot / 2;
	const int32 Column = SafeSlot % 2;

	const FVector2D ToInside(-Start.Forward.Y, Start.Forward.X);
	const double Back = 2500.0 + Row * 1800.0;
	const double Side = (Column == 0) ? -1400.0 : 1400.0;
	const FVector2D Position = Start.Center - Start.Forward * Back + ToInside * Side;

	const double YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Start.Forward.Y, Start.Forward.X));
	return FTransform(
		FRotator(0.0, YawDegrees, 0.0),
		FVector(Position.X, Position.Y, static_cast<double>(WaterZ + BoatRideHeight)));
}

FTransform BoatTrack::GetGateResetTransform(int32 GateIndex)
{
	const TArray<FBoatGate>& Gates = GetGates();
	const int32 Index = ((GateIndex % Gates.Num()) + Gates.Num()) % Gates.Num();
	const FBoatGate& Gate = Gates[Index];

	// Just past the gate, so it does not have to be crossed again, and aimed
	// at the next one.
	const FBoatGate& NextGate = Gates[(Index + 1) % Gates.Num()];
	const FVector2D Position = Gate.Center + Gate.Forward * 800.0;
	const FVector2D Aim = (NextGate.Center - Position).GetSafeNormal();

	const double YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Aim.Y, Aim.X));
	return FTransform(
		FRotator(0.0, YawDegrees, 0.0),
		FVector(Position.X, Position.Y, static_cast<double>(WaterZ + BoatRideHeight + 60.f)));
}

bool BoatTrack::CrossesGate(const FBoatGate& Gate, const FVector2D& From, const FVector2D& To)
{
	const FVector2D Move = To - From;
	if (Dot2D(Move, Gate.Forward) <= 0.0)
	{
		return false; // Stopped, or going the wrong way through the gate.
	}

	const FVector2D GateLine = Gate.Outer - Gate.Inner;
	const double Denominator = Cross2D(Move, GateLine);
	if (FMath::Abs(Denominator) < 1.e-6)
	{
		return false; // Travelling parallel to the gate line.
	}

	const FVector2D ToGate = Gate.Inner - From;
	const double AlongMove = Cross2D(ToGate, GateLine) / Denominator;
	const double AlongGate = Cross2D(ToGate, Move) / Denominator;
	return AlongMove >= 0.0 && AlongMove <= 1.0 && AlongGate >= 0.0 && AlongGate <= 1.0;
}

FLinearColor BoatTrack::GetPlayerColor(int32 Slot)
{
	switch (((Slot % MaxPlayers) + MaxPlayers) % MaxPlayers)
	{
	case 0:  return FLinearColor(0.85f, 0.05f, 0.05f); // red
	case 1:  return FLinearColor(0.05f, 0.25f, 0.90f); // blue
	case 2:  return FLinearColor(0.95f, 0.65f, 0.02f); // yellow
	default: return FLinearColor(0.05f, 0.65f, 0.15f); // green
	}
}
