#pragma once

#include "CoreMinimal.h"

/** The weather the host picks in the lobby. Kept in step on every machine. */
enum class ERaceWeather : uint8
{
	Sunny,
	Cloudy,
	Rainy,
	Stormy,
	Windy,
	Snowy,
	Foggy,
	Sunset,
	Night,
	Count
};

enum class EPrecipitation : uint8
{
	None,
	Rain,
	Snow,
	/** Spray and leaves blown flat across the water. */
	Gale
};

/** Everything one kind of weather sets. */
struct FWeatherLook
{
	const TCHAR* Name = TEXT("");
	/** Sun pitch (negative is above the horizon) and heading, degrees. */
	float SunPitch = -40.f;
	float SunYaw = 160.f;
	float SunIntensity = 8.f;
	FLinearColor SunColor = FLinearColor::White;
	float SkyIntensity = 1.f;
	float FogDensity = 0.01f;
	FLinearColor FogColor = FLinearColor(0.5f, 0.6f, 0.75f);
	FLinearColor WaterTint = FLinearColor(0.01f, 0.1f, 0.15f);
	/** Multiplies the sea state from DefaultGame.ini. */
	float Waves = 1.f;
	/** Mean wind, m/s, and how gusty it is (0 steady .. 1 wild). */
	float Wind = 2.f;
	float Gustiness = 0.2f;
	EPrecipitation Precipitation = EPrecipitation::None;
	/** Grey cloud deck over the whole sky, 0 clear .. 1 solid. */
	float Overcast = 0.f;
	FLinearColor CloudColor = FLinearColor(0.9f, 0.92f, 0.95f);
	/** Fluffy white cumulus drifting over the hills. */
	float Cumulus = 0.5f;
	float Snow = 0.f;
	bool bLightning = false;
	bool bNight = false;
	/** Seagulls, birdsong. */
	bool bGulls = false;
};

namespace RaceWeather
{
	const FWeatherLook& Get(ERaceWeather Weather);
	ERaceWeather FromIndex(int32 Index);
	/** Accepts a name ("Snowy") or a number; Sunny when it makes no sense. */
	ERaceWeather Parse(const FString& Text);

	/**
	 * Wind on the water, cm/s, at a server time. Derived from the shared clock
	 * so every boat on every machine feels the same gusts.
	 */
	FVector2D GetWind(ERaceWeather Weather, float ServerSeconds);

	/** Seconds into a lightning flash at this time, or a negative number. */
	float GetLightningAge(ERaceWeather Weather, float ServerSeconds, FVector2D* OutStrikeMetres = nullptr);
}
