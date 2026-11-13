#include "RaceWeather.h"

namespace
{
	FLinearColor WeatherSRGB(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	TArray<FWeatherLook> MakeLooks()
	{
		TArray<FWeatherLook> Looks;
		Looks.SetNum(static_cast<int32>(ERaceWeather::Count));

		FWeatherLook& Sunny = Looks[static_cast<int32>(ERaceWeather::Sunny)];
		Sunny.Name = TEXT("Sunny");
		Sunny.SunPitch = -48.f; Sunny.SunYaw = 145.f; Sunny.SunIntensity = 10.f;
		Sunny.SunColor = FLinearColor(1.f, 0.96f, 0.88f);
		Sunny.SkyIntensity = 1.2f; Sunny.FogDensity = 0.006f; Sunny.FogColor = WeatherSRGB(150, 196, 240);
		// The bright turquoise of the reference video.
		Sunny.WaterTint = FLinearColor(0.004f, 0.16f, 0.32f);
		Sunny.Waves = 0.6f; Sunny.Wind = 3.f; Sunny.Gustiness = 0.15f;
		Sunny.Cumulus = 0.6f; Sunny.bGulls = true;

		FWeatherLook& Cloudy = Looks[static_cast<int32>(ERaceWeather::Cloudy)];
		Cloudy = Sunny;
		Cloudy.Name = TEXT("Cloudy");
		Cloudy.SunPitch = -40.f; Cloudy.SunIntensity = 5.f; Cloudy.SunColor = FLinearColor(0.92f, 0.93f, 0.96f);
		Cloudy.SkyIntensity = 1.3f; Cloudy.FogDensity = 0.018f; Cloudy.FogColor = WeatherSRGB(160, 172, 186);
		Cloudy.WaterTint = FLinearColor(0.006f, 0.07f, 0.11f);
		Cloudy.Waves = 0.9f; Cloudy.Wind = 5.f; Cloudy.Overcast = 0.55f; Cloudy.CloudColor = WeatherSRGB(200, 205, 214);
		Cloudy.Cumulus = 0.9f; Cloudy.bGulls = true;

		FWeatherLook& Rainy = Looks[static_cast<int32>(ERaceWeather::Rainy)];
		Rainy = Cloudy;
		Rainy.Name = TEXT("Rainy");
		Rainy.SunIntensity = 2.6f; Rainy.SunColor = FLinearColor(0.8f, 0.84f, 0.92f);
		Rainy.SkyIntensity = 1.2f; Rainy.FogDensity = 0.04f; Rainy.FogColor = WeatherSRGB(128, 138, 150);
		Rainy.WaterTint = FLinearColor(0.006f, 0.035f, 0.05f);
		Rainy.Waves = 1.1f; Rainy.Wind = 6.f; Rainy.Gustiness = 0.35f;
		Rainy.Precipitation = EPrecipitation::Rain; Rainy.Overcast = 0.85f; Rainy.CloudColor = WeatherSRGB(120, 126, 136);
		Rainy.Cumulus = 0.f; Rainy.bGulls = false;

		FWeatherLook& Stormy = Looks[static_cast<int32>(ERaceWeather::Stormy)];
		Stormy = Rainy;
		Stormy.Name = TEXT("Stormy");
		Stormy.SunPitch = -14.f; Stormy.SunYaw = 140.f; Stormy.SunIntensity = 1.6f; Stormy.SunColor = FLinearColor(0.72f, 0.78f, 0.92f);
		Stormy.SkyIntensity = 0.9f; Stormy.FogDensity = 0.06f; Stormy.FogColor = WeatherSRGB(74, 82, 92);
		Stormy.WaterTint = FLinearColor(0.004f, 0.02f, 0.024f);
		Stormy.Waves = 1.45f; Stormy.Wind = 11.f; Stormy.Gustiness = 0.6f;
		Stormy.Overcast = 0.95f; Stormy.CloudColor = WeatherSRGB(62, 66, 74); Stormy.bLightning = true;

		FWeatherLook& Windy = Looks[static_cast<int32>(ERaceWeather::Windy)];
		Windy = Sunny;
		Windy.Name = TEXT("Windy");
		Windy.SunPitch = -35.f; Windy.SunYaw = 120.f; Windy.SunIntensity = 7.f;
		Windy.FogDensity = 0.014f; Windy.WaterTint = FLinearColor(0.004f, 0.09f, 0.2f);
		Windy.Waves = 1.35f; Windy.Wind = 14.f; Windy.Gustiness = 0.7f;
		Windy.Precipitation = EPrecipitation::Gale; Windy.Overcast = 0.25f; Windy.Cumulus = 1.f;

		FWeatherLook& Snowy = Looks[static_cast<int32>(ERaceWeather::Snowy)];
		Snowy = Cloudy;
		Snowy.Name = TEXT("Snowy");
		Snowy.SunPitch = -22.f; Snowy.SunIntensity = 3.5f; Snowy.SunColor = FLinearColor(0.9f, 0.94f, 1.f);
		Snowy.SkyIntensity = 1.5f; Snowy.FogDensity = 0.035f; Snowy.FogColor = WeatherSRGB(206, 214, 228);
		Snowy.WaterTint = FLinearColor(0.008f, 0.04f, 0.07f);
		Snowy.Waves = 0.8f; Snowy.Wind = 4.f; Snowy.Precipitation = EPrecipitation::Snow;
		Snowy.Overcast = 0.75f; Snowy.CloudColor = WeatherSRGB(214, 220, 230); Snowy.Cumulus = 0.f;
		Snowy.Snow = 1.f; Snowy.bGulls = false;

		FWeatherLook& Foggy = Looks[static_cast<int32>(ERaceWeather::Foggy)];
		Foggy = Cloudy;
		Foggy.Name = TEXT("Foggy");
		Foggy.SunPitch = -30.f; Foggy.SunIntensity = 3.f;
		Foggy.SkyIntensity = 1.4f; Foggy.FogDensity = 0.35f; Foggy.FogColor = WeatherSRGB(196, 204, 210);
		Foggy.WaterTint = FLinearColor(0.008f, 0.05f, 0.06f);
		Foggy.Waves = 0.4f; Foggy.Wind = 1.f; Foggy.Overcast = 0.6f; Foggy.Cumulus = 0.f; Foggy.bGulls = true;

		FWeatherLook& Sunset = Looks[static_cast<int32>(ERaceWeather::Sunset)];
		Sunset = Sunny;
		Sunset.Name = TEXT("Sunset");
		Sunset.SunPitch = -4.f; Sunset.SunYaw = 200.f; Sunset.SunIntensity = 7.f; Sunset.SunColor = FLinearColor(1.f, 0.55f, 0.25f);
		Sunset.SkyIntensity = 1.1f; Sunset.FogDensity = 0.012f; Sunset.FogColor = WeatherSRGB(240, 160, 110);
		Sunset.WaterTint = FLinearColor(0.012f, 0.05f, 0.12f);
		Sunset.Waves = 0.7f; Sunset.Cumulus = 0.7f; Sunset.CloudColor = WeatherSRGB(255, 190, 150);

		FWeatherLook& Night = Looks[static_cast<int32>(ERaceWeather::Night)];
		Night = Sunny;
		Night.Name = TEXT("Night");
		// Moonlight: the sky atmosphere goes dark blue with a dim, cold light.
		Night.SunPitch = -35.f; Night.SunYaw = 60.f; Night.SunIntensity = 0.35f; Night.SunColor = FLinearColor(0.55f, 0.65f, 1.f);
		Night.SkyIntensity = 0.6f; Night.FogDensity = 0.01f; Night.FogColor = WeatherSRGB(20, 28, 48);
		Night.WaterTint = FLinearColor(0.002f, 0.01f, 0.025f);
		Night.Waves = 0.8f; Night.Cumulus = 0.f; Night.Overcast = 0.2f; Night.CloudColor = WeatherSRGB(30, 36, 52);
		Night.bNight = true; Night.bGulls = false;
		return Looks;
	}

	uint32 Mix(uint32 Value)
	{
		Value ^= Value >> 16;
		Value *= 0x7feb352d;
		Value ^= Value >> 15;
		Value *= 0x846ca68b;
		Value ^= Value >> 16;
		return Value;
	}

	float Unit(uint32 Seed)
	{
		return static_cast<float>(Mix(Seed) & 0xFFFF) / 65535.f;
	}
}

const FWeatherLook& RaceWeather::Get(ERaceWeather Weather)
{
	static const TArray<FWeatherLook> Looks = MakeLooks();
	return Looks[FMath::Clamp(static_cast<int32>(Weather), 0, Looks.Num() - 1)];
}

ERaceWeather RaceWeather::FromIndex(int32 Index)
{
	const int32 Count = static_cast<int32>(ERaceWeather::Count);
	return static_cast<ERaceWeather>(((Index % Count) + Count) % Count);
}

ERaceWeather RaceWeather::Parse(const FString& Text)
{
	const FString Trimmed = Text.TrimStartAndEnd();
	if (Trimmed.IsNumeric())
	{
		return FromIndex(FCString::Atoi(*Trimmed));
	}
	for (int32 Index = 0; Index < static_cast<int32>(ERaceWeather::Count); ++Index)
	{
		if (Trimmed.Equals(Get(FromIndex(Index)).Name, ESearchCase::IgnoreCase))
		{
			return FromIndex(Index);
		}
	}
	return ERaceWeather::Sunny;
}

FVector2D RaceWeather::GetWind(ERaceWeather Weather, float ServerSeconds)
{
	const FWeatherLook& Look = Get(Weather);
	// A slowly veering direction and a few overlapping gust rhythms.
	const float Heading = FMath::DegreesToRadians(35.f + 25.f * FMath::Sin(ServerSeconds * 0.021f));
	const float Gust = 1.f + Look.Gustiness * (0.55f * FMath::Sin(ServerSeconds * 0.37f)
		+ 0.3f * FMath::Sin(ServerSeconds * 1.13f + 1.f) + 0.15f * FMath::Sin(ServerSeconds * 2.9f + 2.f));
	const float Speed = Look.Wind * 100.f * FMath::Max(Gust, 0.f);
	return FVector2D(FMath::Cos(Heading), FMath::Sin(Heading)) * Speed;
}

float RaceWeather::GetLightningAge(ERaceWeather Weather, float ServerSeconds, FVector2D* OutStrikeMetres)
{
	if (!Get(Weather).bLightning)
	{
		return -1.f;
	}
	// Six-second windows; most of them hold one strike at a random moment.
	const float Window = 6.f;
	const int32 Slot = FMath::FloorToInt(ServerSeconds / Window);
	if (Unit(static_cast<uint32>(Slot) * 3u + 1u) > 0.7f)
	{
		return -1.f;
	}
	const float StrikeAt = Slot * Window + Unit(static_cast<uint32>(Slot) * 3u + 2u) * (Window - 1.f);
	if (OutStrikeMetres != nullptr)
	{
		const float Angle = Unit(static_cast<uint32>(Slot) * 7u + 5u) * 2.f * PI;
		const float Distance = 900.f + 900.f * Unit(static_cast<uint32>(Slot) * 7u + 6u);
		*OutStrikeMetres = FVector2D(30.f + FMath::Cos(Angle) * Distance, -75.f + FMath::Sin(Angle) * Distance);
	}
	return ServerSeconds - StrikeAt;
}
