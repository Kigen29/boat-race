#include "BoatAudio.h"

#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "RaceWeather.h"

#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "Sound/SoundAttenuation.h"

// ===========================================================================
// Synthesis
//
// Everything is built once, the first time it is asked for, at 22,050 Hz mono.
// That is plenty for engines, water and explosions and keeps the music loop
// to about a megabyte. Each sound is designed rather than recorded: layers of
// oscillators, filtered noise and envelopes, mixed and soft-clipped.
// ===========================================================================
namespace
{
	const int32 Rate = 22050;
	const float TwoPi = 2.f * PI;

	using FBuffer = TArray<float>;

	FBuffer Silence(float Seconds)
	{
		FBuffer Out;
		Out.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * Rate)));
		return Out;
	}

	float T(int32 Index) { return static_cast<float>(Index) / Rate; }

	/** One-pole filters, the workhorse of all the noise-based sounds. */
	struct FLowPass
	{
		float State = 0.f;
		float Coefficient = 1.f;
		explicit FLowPass(float Cutoff) { Set(Cutoff); }
		void Set(float Cutoff) { Coefficient = 1.f - FMath::Exp(-TwoPi * FMath::Max(Cutoff, 1.f) / Rate); }
		float operator()(float In) { State += Coefficient * (In - State); return State; }
	};

	struct FHighPass
	{
		FLowPass Low;
		explicit FHighPass(float Cutoff) : Low(Cutoff) {}
		float operator()(float In) { return In - Low(In); }
	};

	float Saw(float Phase) { return 2.f * (Phase - FMath::FloorToFloat(Phase)) - 1.f; }
	float Square(float Phase) { return (Phase - FMath::FloorToFloat(Phase)) < 0.5f ? 1.f : -1.f; }
	float Tri(float Phase) { return 1.f - 4.f * FMath::Abs((Phase - FMath::FloorToFloat(Phase)) - 0.5f); }
	float Note(float Midi) { return 440.f * FMath::Pow(2.f, (Midi - 69.f) / 12.f); }

	void Normalise(FBuffer& Buffer, float Peak)
	{
		float Max = 1.e-6f;
		for (float Value : Buffer)
		{
			Max = FMath::Max(Max, FMath::Abs(Value));
		}
		for (float& Value : Buffer)
		{
			Value *= Peak / Max;
		}
	}

	void SoftClip(FBuffer& Buffer, float Drive)
	{
		for (float& Value : Buffer)
		{
			Value = FMath::Tanh(Value * Drive);
		}
	}

	/** Makes a loop seamless by folding the tail back over the head. */
	void MakeLoop(FBuffer& Buffer, float FadeSeconds)
	{
		const int32 Fade = FMath::Min(FMath::RoundToInt(FadeSeconds * Rate), Buffer.Num() / 3);
		const int32 Keep = Buffer.Num() - Fade;
		for (int32 Index = 0; Index < Fade; ++Index)
		{
			const float Mix = static_cast<float>(Index) / Fade;
			Buffer[Index] = Buffer[Index] * Mix + Buffer[Keep + Index] * (1.f - Mix);
		}
		Buffer.SetNum(Keep);
	}

	/** A decaying, slightly inharmonic bell: chimes and pickups. */
	void AddBell(FBuffer& Buffer, float Start, float Frequency, float Volume, float Decay)
	{
		const float Partials[][2] = { { 1.f, 1.f }, { 2.01f, 0.5f }, { 2.76f, 0.3f }, { 5.4f, 0.12f } };
		const int32 First = FMath::RoundToInt(Start * Rate);
		for (int32 Index = FMath::Max(First, 0); Index < Buffer.Num(); ++Index)
		{
			const float Time = T(Index - First);
			float Sample = 0.f;
			for (const float* Partial : Partials)
			{
				Sample += Partial[1] * FMath::Sin(TwoPi * Frequency * Partial[0] * Time) * FMath::Exp(-Time / (Decay / Partial[0]));
			}
			Buffer[Index] += Sample * Volume * FMath::Min(Time * 400.f, 1.f);
		}
	}

	// --- The sounds ---------------------------------------------------------

	FBuffer MakeEngine()
	{
		// One second holding exactly 60 firing cycles, so it loops without a
		// click and the game can pitch it from idle (~40 Hz) to flat out.
		FBuffer Out = Silence(1.f);
		FRandomStream Random(7);
		FLowPass Rumble(380.f), Grit(2200.f);
		const float F0 = 60.f;
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			float Tone = 0.f;
			for (int32 Harmonic = 1; Harmonic <= 14; ++Harmonic)
			{
				const float Weight = (Harmonic % 2 ? 1.f : 0.65f) / FMath::Pow(static_cast<float>(Harmonic), 0.85f);
				Tone += Weight * FMath::Sin(TwoPi * F0 * Harmonic * Time + Harmonic * 1.3f);
			}
			// Combustion: each firing is a pulse, and every other one a little
			// stronger, which is the lumpy two-stroke sound of an outboard.
			const float Firing = FMath::Pow(0.5f + 0.5f * FMath::Cos(TwoPi * F0 * Time), 6.f);
			const float Lope = 1.f + 0.25f * FMath::Sin(TwoPi * F0 * 0.5f * Time);
			const float Noise = Grit(Random.FRandRange(-1.f, 1.f));
			Out[Index] = Rumble(Tone * 0.35f * Lope) + Firing * (0.55f + Noise * 0.9f) * 0.6f + Tone * 0.08f;
		}
		SoftClip(Out, 1.8f);
		Normalise(Out, 0.8f);
		return Out;
	}

	FBuffer MakeWaterRush()
	{
		FBuffer Out = Silence(3.4f);
		FRandomStream Random(11);
		FLowPass Body(900.f), Fizz(5000.f);
		FHighPass Hiss(2500.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float White = Random.FRandRange(-1.f, 1.f);
			// Slaps of the hull meeting chop, a few a second.
			const float Slap = FMath::Pow(FMath::Max(FMath::Sin(TwoPi * 2.3f * Time + FMath::Sin(Time * 5.f)), 0.f), 8.f);
			Out[Index] = Body(White) * (0.7f + 0.8f * Slap) + Hiss(Fizz(White)) * 0.35f;
		}
		MakeLoop(Out, 0.4f);
		Normalise(Out, 0.7f);
		return Out;
	}

	FBuffer MakeExplosion()
	{
		FBuffer Out = Silence(3.2f);
		FRandomStream Random(13);
		FLowPass Body(1400.f), Deep(160.f), Crackle(3000.f);
		float Phase = 0.f;
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float White = Random.FRandRange(-1.f, 1.f);
			// Crack: a few milliseconds of bright noise.
			const float Crack = White * FMath::Exp(-Time / 0.012f) * 1.4f;
			// Body: the roar, darkening as it dies away.
			Body.Set(1400.f * FMath::Exp(-Time / 0.5f) + 180.f);
			const float Roar = Body(White) * FMath::Exp(-Time / 0.55f) * 2.2f;
			// Sub boom: a falling sine you feel more than hear.
			const float Pitch = 28.f + 52.f * FMath::Exp(-Time / 0.18f);
			Phase += Pitch / Rate;
			const float Boom = FMath::Sin(TwoPi * Phase) * FMath::Exp(-Time / 0.9f) * 1.3f;
			// Rumble and falling debris: sparse clicks in the tail.
			const float Rumble = Deep(White) * 3.f * FMath::Exp(-Time / 1.3f);
			const float Debris = (Random.FRand() < 0.004f * FMath::Exp(-Time / 0.8f)) ? Random.FRandRange(-1.f, 1.f) : 0.f;
			Out[Index] = Crack + Roar + Boom + Rumble + Crackle(Debris) * 3.f;
			// The water it throws up comes down as a hiss.
			if (Time > 0.35f)
			{
				Out[Index] += White * 0.18f * FMath::Exp(-(Time - 0.35f) / 0.9f) * FMath::Min((Time - 0.35f) * 4.f, 1.f);
			}
		}
		SoftClip(Out, 1.3f);
		Normalise(Out, 0.95f);
		return Out;
	}

	FBuffer MakeTorpedo()
	{
		FBuffer Out = Silence(1.3f);
		FRandomStream Random(17);
		FLowPass Sweep(400.f), Thump(120.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float White = Random.FRandRange(-1.f, 1.f);
			Sweep.Set(400.f + 3200.f * FMath::Sin(FMath::Min(Time / 1.3f, 1.f) * PI));
			const float Whoosh = Sweep(White) * FMath::Sin(FMath::Min(Time / 1.3f, 1.f) * PI);
			const float Launch = Thump(White) * 6.f * FMath::Exp(-Time / 0.06f) + FMath::Sin(TwoPi * 70.f * Time) * FMath::Exp(-Time / 0.08f);
			Out[Index] = Whoosh * 1.4f + Launch;
		}
		Normalise(Out, 0.8f);
		return Out;
	}

	FBuffer MakeMineDrop()
	{
		FBuffer Out = Silence(0.7f);
		FRandomStream Random(19);
		FLowPass Bubbles(1800.f);
		float Phase = 0.f;
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float Pitch = 120.f + 380.f * FMath::Exp(-Time / 0.05f);
			Phase += Pitch / Rate;
			const float Plop = FMath::Sin(TwoPi * Phase) * FMath::Exp(-Time / 0.12f);
			const float Burble = Bubbles(Random.FRandRange(-1.f, 1.f)) * FMath::Exp(-Time / 0.25f)
				* (0.5f + 0.5f * FMath::Sin(TwoPi * 23.f * Time));
			Out[Index] = Plop + Burble * 1.5f;
		}
		Normalise(Out, 0.7f);
		return Out;
	}

	FBuffer MakeBeeps(int32 Count, float Frequency, float Length, float Gap)
	{
		FBuffer Out = Silence(Count * (Length + Gap));
		for (int32 Beep = 0; Beep < Count; ++Beep)
		{
			const int32 Start = FMath::RoundToInt(Beep * (Length + Gap) * Rate);
			const int32 Samples = FMath::RoundToInt(Length * Rate);
			for (int32 Index = 0; Index < Samples && Start + Index < Out.Num(); ++Index)
			{
				const float Time = T(Index);
				const float Envelope = FMath::Min(Time * 200.f, 1.f) * FMath::Min((Length - Time) * 60.f, 1.f);
				Out[Start + Index] += (FMath::Sin(TwoPi * Frequency * Time) + 0.3f * Square(Frequency * Time) * 0.4f) * Envelope;
			}
		}
		Normalise(Out, 0.6f);
		return Out;
	}

	FBuffer MakeSplash()
	{
		FBuffer Out = Silence(1.1f);
		FRandomStream Random(23);
		FLowPass Low(2500.f);
		FHighPass High(400.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float Envelope = FMath::Min(Time * 60.f, 1.f) * FMath::Exp(-Time / 0.28f);
			Low.Set(4000.f * FMath::Exp(-Time / 0.4f) + 600.f);
			Out[Index] = High(Low(Random.FRandRange(-1.f, 1.f))) * Envelope;
		}
		Normalise(Out, 0.75f);
		return Out;
	}

	FBuffer MakeCrash()
	{
		FBuffer Out = Silence(1.0f);
		FRandomStream Random(29);
		FLowPass Thud(220.f), Scrape(3000.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float White = Random.FRandRange(-1.f, 1.f);
			const float Body = Thud(White) * 5.f * FMath::Exp(-Time / 0.09f) + FMath::Sin(TwoPi * 85.f * Time) * FMath::Exp(-Time / 0.12f) * 0.9f;
			// Fibreglass and steel ring with a few unrelated partials.
			float Ring = 0.f;
			for (float Partial : { 523.f, 811.f, 1307.f, 1990.f })
			{
				Ring += FMath::Sin(TwoPi * Partial * Time) * FMath::Exp(-Time / (0.25f * 523.f / Partial));
			}
			Out[Index] = Body + Ring * 0.25f + Scrape(White) * 0.4f * FMath::Exp(-Time / 0.2f);
		}
		SoftClip(Out, 1.5f);
		Normalise(Out, 0.85f);
		return Out;
	}

	FBuffer MakePickup()
	{
		FBuffer Out = Silence(0.75f);
		const float Notes[] = { 72.f, 76.f, 79.f, 84.f };
		for (int32 Step = 0; Step < 4; ++Step)
		{
			AddBell(Out, Step * 0.07f, Note(Notes[Step]), 0.5f, 0.35f);
		}
		Normalise(Out, 0.6f);
		return Out;
	}

	FBuffer MakeLapChime()
	{
		FBuffer Out = Silence(1.4f);
		AddBell(Out, 0.f, Note(79.f), 0.6f, 0.6f);
		AddBell(Out, 0.16f, Note(84.f), 0.7f, 0.8f);
		Normalise(Out, 0.65f);
		return Out;
	}

	FBuffer MakeHorn()
	{
		// An air horn: three detuned reedy saws with a wobble, two blasts.
		FBuffer Out = Silence(1.7f);
		FLowPass Tone(2400.f);
		const float Chord[] = { 233.f, 277.f, 349.f };
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const bool bBlast = Time < 0.55f || (Time > 0.7f && Time < 1.65f);
			if (!bBlast)
			{
				continue;
			}
			const float Local = Time < 0.55f ? Time : Time - 0.7f;
			const float Envelope = FMath::Min(Local * 40.f, 1.f) * FMath::Min(((Time < 0.55f ? 0.55f : 0.95f) - Local) * 30.f, 1.f);
			float Sample = 0.f;
			for (float Frequency : Chord)
			{
				Sample += Saw(Frequency * Time * (1.f + 0.004f * FMath::Sin(TwoPi * 5.f * Time)));
			}
			Out[Index] = Tone(Sample) * Envelope;
		}
		SoftClip(Out, 1.4f);
		Normalise(Out, 0.7f);
		return Out;
	}

	FBuffer MakeFanfare()
	{
		FBuffer Out = Silence(3.0f);
		FLowPass Brass(3000.f);
		struct FHit { float Start; float Length; float Notes[3]; };
		const FHit Hits[] =
		{
			{ 0.00f, 0.16f, { 67.f, 0.f, 0.f } }, { 0.18f, 0.16f, { 67.f, 0.f, 0.f } },
			{ 0.36f, 0.16f, { 67.f, 0.f, 0.f } }, { 0.54f, 0.5f, { 72.f, 76.f, 79.f } },
			{ 1.10f, 0.25f, { 70.f, 74.f, 77.f } }, { 1.40f, 1.4f, { 72.f, 76.f, 84.f } },
		};
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			float Sample = 0.f;
			for (const FHit& Hit : Hits)
			{
				const float Local = Time - Hit.Start;
				if (Local < 0.f || Local > Hit.Length)
				{
					continue;
				}
				const float Envelope = FMath::Min(Local * 30.f, 1.f) * FMath::Min((Hit.Length - Local) * 12.f, 1.f);
				for (float Midi : Hit.Notes)
				{
					if (Midi > 0.f)
					{
						const float Frequency = Note(Midi) * (1.f + 0.003f * FMath::Sin(TwoPi * 6.f * Time));
						Sample += (Saw(Frequency * Time) * 0.6f + Square(Frequency * Time * 0.5f) * 0.2f) * Envelope;
					}
				}
			}
			Out[Index] = Brass(Sample);
		}
		Normalise(Out, 0.7f);
		return Out;
	}

	FBuffer MakeThunder()
	{
		FBuffer Out = Silence(6.f);
		FRandomStream Random(31);
		FLowPass Rumble(140.f), Crack(2500.f), Swell(3.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			const float White = Random.FRandRange(-1.f, 1.f);
			// A slowly wandering loudness, so it rolls rather than just fades.
			const float Roll = 0.4f + FMath::Abs(Swell(Random.FRandRange(-1.f, 1.f))) * 18.f;
			const float Envelope = FMath::Min(Time * 3.f, 1.f) * FMath::Exp(-Time / 2.2f);
			Out[Index] = Rumble(White) * 6.f * Roll * Envelope + Crack(White) * FMath::Exp(-Time / 0.15f) * 0.6f;
		}
		SoftClip(Out, 1.2f);
		Normalise(Out, 0.95f);
		return Out;
	}

	FBuffer MakeRain()
	{
		FBuffer Out = Silence(4.4f);
		FRandomStream Random(37);
		FHighPass Hiss(1200.f);
		FLowPass Soft(7000.f), Drops(3500.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float White = Random.FRandRange(-1.f, 1.f);
			const float Drop = (Random.FRand() < 0.0025f) ? Random.FRandRange(-1.f, 1.f) * 3.f : 0.f;
			Out[Index] = Soft(Hiss(White)) * 0.6f + Drops(Drop);
		}
		MakeLoop(Out, 0.4f);
		Normalise(Out, 0.6f);
		return Out;
	}

	FBuffer MakeWind()
	{
		FBuffer Out = Silence(6.4f);
		FRandomStream Random(41);
		FLowPass Band(500.f), Body(260.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			// The whistle wanders, at a whole number of cycles per loop.
			Band.Set(450.f + 380.f * FMath::Sin(TwoPi * Time / 6.f) + 160.f * FMath::Sin(TwoPi * Time * 3.f / 6.f));
			const float White = Random.FRandRange(-1.f, 1.f);
			const float Gust = 0.6f + 0.4f * FMath::Sin(TwoPi * Time * 2.f / 6.f);
			Out[Index] = (Band(White) * 2.f - Body(White) * 0.8f) * Gust;
		}
		MakeLoop(Out, 0.4f);
		Normalise(Out, 0.6f);
		return Out;
	}

	FBuffer MakeSea()
	{
		FBuffer Out = Silence(8.4f);
		FRandomStream Random(43);
		FLowPass Wash(600.f), Lap(2000.f);
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			// Waves breaking on the shore, two per loop.
			const float Swell = FMath::Pow(0.5f + 0.5f * FMath::Sin(TwoPi * Time * 2.f / 8.f - 1.2f), 3.f);
			const float White = Random.FRandRange(-1.f, 1.f);
			Out[Index] = Wash(White) * (0.3f + Swell) + Lap(White) * 0.15f * Swell;
		}
		MakeLoop(Out, 0.4f);
		Normalise(Out, 0.5f);
		return Out;
	}

	FBuffer MakeGull()
	{
		FBuffer Out = Silence(1.5f);
		FRandomStream Random(47);
		float Phase = 0.f;
		const float Calls[] = { 0.f, 0.32f, 0.6f };
		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const float Time = T(Index);
			float Sample = 0.f;
			for (float Start : Calls)
			{
				const float Local = Time - Start;
				const float Length = Start > 0.5f ? 0.45f : 0.22f;
				if (Local < 0.f || Local > Length)
				{
					continue;
				}
				const float U = Local / Length;
				const float Pitch = 1250.f + 650.f * FMath::Sin(U * PI) - 300.f * U;
				Phase += Pitch / Rate;
				const float Envelope = FMath::Sin(U * PI);
				// A rough, nasal tone: a few harmonics with a buzz.
				Sample += (FMath::Sin(TwoPi * Phase) + 0.5f * FMath::Sin(TwoPi * Phase * 2.f) + 0.3f * FMath::Sin(TwoPi * Phase * 3.f)
					+ 0.15f * Random.FRandRange(-1.f, 1.f)) * Envelope;
			}
			Out[Index] = Sample;
		}
		Normalise(Out, 0.4f);
		return Out;
	}

	FBuffer MakeMusic()
	{
		// 16 bars at 120 bpm in A minor: Am F C G, driving four-on-the-floor,
		// a pumping bass, a sixteenth-note arpeggio and a hook in the second half.
		const int32 Sixteenth = 2756; // 22050 * 60 / 120 / 4, rounded
		const int32 Steps = 16 * 16;
		FBuffer Out;
		Out.SetNumZeroed(Steps * Sixteenth);
		FRandomStream Random(53);

		const float Roots[] = { 57.f, 53.f, 48.f, 55.f };              // A2 F2 C2 G2
		const float Chords[][3] = { { 69.f, 72.f, 76.f }, { 65.f, 69.f, 72.f }, { 64.f, 67.f, 72.f }, { 67.f, 71.f, 74.f } };
		const float Hook[] = { 76.f, 0.f, 74.f, 72.f, 74.f, 0.f, 76.f, 0.f, 79.f, 0.f, 76.f, 74.f, 72.f, 0.f, 69.f, 0.f };

		FLowPass BassFilter(500.f), PadFilter(1300.f), ArpFilter(3200.f), LeadFilter(2600.f);
		FHighPass HatFilter(6000.f);
		float BassPhase = 0.f, LeadPhase = 0.f;
		float PadPhase[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };

		for (int32 Index = 0; Index < Out.Num(); ++Index)
		{
			const int32 Step = Index / Sixteenth;
			const int32 InStep = Index % Sixteenth;
			const float StepTime = static_cast<float>(InStep) / Rate;
			const int32 Bar = Step / 16;
			const int32 Beat16 = Step % 16;
			const int32 ChordIndex = (Bar / 2) % 4;
			const bool bSecondHalf = Bar >= 8;
			float Sample = 0.f;

			// Kick on every beat: a fast falling sine.
			const int32 BeatSample = Index % (Sixteenth * 4);
			const float BeatTime = static_cast<float>(BeatSample) / Rate;
			{
				const float Pitch = 45.f + 110.f * FMath::Exp(-BeatTime / 0.03f);
				Sample += FMath::Sin(TwoPi * Pitch * BeatTime) * FMath::Exp(-BeatTime / 0.16f) * 0.9f;
			}
			const float White = Random.FRandRange(-1.f, 1.f);
			// Clap on two and four.
			if (Beat16 == 4 || Beat16 == 12)
			{
				Sample += White * FMath::Exp(-StepTime / 0.07f) * 0.45f + FMath::Sin(TwoPi * 190.f * StepTime) * FMath::Exp(-StepTime / 0.05f) * 0.2f;
			}
			// Hats on the off-beats, ghosted sixteenths, open on the last.
			{
				const bool bOff = (Beat16 % 4) == 2;
				const float Level = bOff ? 0.22f : ((Beat16 % 2) ? 0.07f : 0.f);
				const float Decay = (Beat16 == 14) ? 0.12f : 0.025f;
				Sample += HatFilter(White) * Level * FMath::Exp(-StepTime / Decay) * 1.6f;
			}
			// Bass: octave-jumping eighths, ducking under the kick.
			{
				const float Midi = Roots[ChordIndex] - 12.f + ((Beat16 / 2) % 2 ? 12.f : 0.f);
				BassPhase += Note(Midi) / Rate;
				const float Duck = 0.35f + 0.65f * FMath::Min(BeatTime / 0.12f, 1.f);
				const float Pluck = FMath::Exp(-static_cast<float>(Index % (Sixteenth * 2)) / Rate / 0.18f);
				BassFilter.Set(250.f + 900.f * Pluck);
				Sample += BassFilter(Saw(BassPhase)) * 0.55f * Duck;
			}
			// Pad: the chord on detuned saws, very soft.
			{
				float Pad = 0.f;
				for (int32 Voice = 0; Voice < 6; ++Voice)
				{
					const float Midi = Chords[ChordIndex][Voice % 3] - 12.f;
					PadPhase[Voice] += Note(Midi) * (Voice < 3 ? 1.003f : 0.997f) / Rate;
					Pad += Saw(PadPhase[Voice]);
				}
				const float BarTime = static_cast<float>(Index % (Sixteenth * 32)) / Rate;
				Sample += PadFilter(Pad) * 0.06f * FMath::Min(BarTime / 0.6f, 1.f);
			}
			// Arpeggio: up through the chord, two octaves, every sixteenth.
			{
				const int32 Order[] = { 0, 1, 2, 1, 0, 1, 2, 0 };
				const float Midi = Chords[ChordIndex][Order[Beat16 % 8]] + (Beat16 >= 8 ? 12.f : 0.f);
				const float Frequency = Note(Midi);
				Sample += ArpFilter(Square(Frequency * StepTime) * 0.5f + Tri(Frequency * StepTime) * 0.5f)
					* FMath::Exp(-StepTime / 0.09f) * (bSecondHalf ? 0.13f : 0.18f);
			}
			// Hook in the second half.
			if (bSecondHalf)
			{
				const float Midi = Hook[Beat16] > 0.f ? Hook[Beat16] + ((Bar % 2) && Beat16 >= 8 ? -2.f : 0.f) : 0.f;
				if (Midi > 0.f)
				{
					LeadPhase += Note(Midi) * (1.f + 0.004f * FMath::Sin(TwoPi * 5.5f * T(Index))) / Rate;
					const float Envelope = FMath::Min(StepTime * 80.f, 1.f) * FMath::Exp(-StepTime / 0.25f);
					Sample += LeadFilter(Saw(LeadPhase) * 0.6f + Square(LeadPhase) * 0.4f) * 0.22f * Envelope;
				}
			}
			Out[Index] = Sample;
		}
		SoftClip(Out, 1.2f);
		Normalise(Out, 0.8f);
		return Out;
	}

	using FSamples = TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe>;

	FSamples ToPcm(const FBuffer& Buffer)
	{
		TSharedPtr<TArray<int16>, ESPMode::ThreadSafe> Pcm = MakeShared<TArray<int16>, ESPMode::ThreadSafe>();
		Pcm->SetNumUninitialized(Buffer.Num());
		for (int32 Index = 0; Index < Buffer.Num(); ++Index)
		{
			(*Pcm)[Index] = static_cast<int16>(FMath::Clamp(Buffer[Index], -1.f, 1.f) * 32000.f);
		}
		return Pcm;
	}

	FSamples GetSamples(EBoatSound Sound)
	{
		static FSamples Cache[static_cast<int32>(EBoatSound::Count)];
		FSamples& Slot = Cache[static_cast<int32>(Sound)];
		if (Slot.IsValid())
		{
			return Slot;
		}
		FBuffer Buffer;
		switch (Sound)
		{
		case EBoatSound::Engine:    Buffer = MakeEngine(); break;
		case EBoatSound::WaterRush: Buffer = MakeWaterRush(); break;
		case EBoatSound::Explosion: Buffer = MakeExplosion(); break;
		case EBoatSound::Torpedo:   Buffer = MakeTorpedo(); break;
		case EBoatSound::MineDrop:  Buffer = MakeMineDrop(); break;
		case EBoatSound::MineArm:   Buffer = MakeBeeps(2, 1250.f, 0.07f, 0.06f); break;
		case EBoatSound::Splash:    Buffer = MakeSplash(); break;
		case EBoatSound::Crash:     Buffer = MakeCrash(); break;
		case EBoatSound::Pickup:    Buffer = MakePickup(); break;
		case EBoatSound::Beep:      Buffer = MakeBeeps(1, 660.f, 0.28f, 0.05f); break;
		case EBoatSound::Go:        Buffer = MakeBeeps(1, 1320.f, 0.75f, 0.05f); break;
		case EBoatSound::LapChime:  Buffer = MakeLapChime(); break;
		case EBoatSound::FinalLap:  Buffer = MakeHorn(); break;
		case EBoatSound::Fanfare:   Buffer = MakeFanfare(); break;
		case EBoatSound::Thunder:   Buffer = MakeThunder(); break;
		case EBoatSound::Rain:      Buffer = MakeRain(); break;
		case EBoatSound::Wind:      Buffer = MakeWind(); break;
		case EBoatSound::Sea:       Buffer = MakeSea(); break;
		case EBoatSound::Gull:      Buffer = MakeGull(); break;
		case EBoatSound::Music:     Buffer = MakeMusic(); break;
		default:                    Buffer = Silence(0.1f); break;
		}
		Slot = ToPcm(Buffer);
		return Slot;
	}
}

float BoatAudio::GetDuration(EBoatSound Sound)
{
	return static_cast<float>(GetSamples(Sound)->Num()) / Rate;
}

// ===========================================================================
// The procedural wave
// ===========================================================================
void UBoatSynthWave::Start(TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples, bool bInLoop)
{
	Samples = InSamples;
	bLoop = bInLoop;
	Cursor = 0;
	SetSampleRate(Rate);
	NumChannels = 1;
	Duration = bLoop ? INDEFINITELY_LOOPING_DURATION : static_cast<float>(Samples->Num()) / Rate;
	SoundGroup = SOUNDGROUP_Default;
	bLooping = false; // the looping is done here, in the generator
}

int32 UBoatSynthWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
	OutAudio.Reset();
	OutAudio.AddZeroed(NumSamples * sizeof(int16));
	int16* Out = reinterpret_cast<int16*>(OutAudio.GetData());
	if (!Samples.IsValid() || Samples->Num() == 0)
	{
		return NumSamples;
	}
	const TArray<int16>& Source = *Samples;
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		if (Cursor >= Source.Num())
		{
			if (!bLoop)
			{
				break; // the rest stays silent until the subsystem stops us
			}
			Cursor = 0;
		}
		Out[Index] = Source[Cursor++];
	}
	return NumSamples;
}

// ===========================================================================
// The subsystem
// ===========================================================================
UBoatAudioSubsystem* UBoatAudioSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBoatAudioSubsystem>() : nullptr;
}

bool UBoatAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBoatAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBoatAudioSubsystem, STATGROUP_Tickables);
}

UBoatSynthWave* UBoatAudioSubsystem::MakeWave(EBoatSound Sound, bool bLoop) const
{
	UBoatSynthWave* Wave = NewObject<UBoatSynthWave>(const_cast<UBoatAudioSubsystem*>(this));
	Wave->Start(GetSamples(Sound), bLoop);
	return Wave;
}

UAudioComponent* UBoatAudioSubsystem::PlayAt(EBoatSound Sound, const FVector& Location, float Volume, float Pitch)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || EffectsVolume <= 0.f)
	{
		return nullptr;
	}
	if (FarAttenuation == nullptr)
	{
		FarAttenuation = NewObject<USoundAttenuation>(this);
		FarAttenuation->Attenuation.bAttenuate = true;
		FarAttenuation->Attenuation.bSpatialize = true;
		FarAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		FarAttenuation->Attenuation.AttenuationShapeExtents = FVector(2500.f, 0.f, 0.f);
		FarAttenuation->Attenuation.FalloffDistance = 40000.f;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSoundAtLocation(World, MakeWave(Sound, false), Location,
		FRotator::ZeroRotator, Volume * EffectsVolume, Pitch, 0.f, FarAttenuation, nullptr, true);
	if (Component != nullptr)
	{
		OneShots.Add({ Component, World->GetTimeSeconds() + BoatAudio::GetDuration(Sound) / FMath::Max(Pitch, 0.1f) + 0.15 });
	}
	return Component;
}

UAudioComponent* UBoatAudioSubsystem::Play2D(EBoatSound Sound, float Volume, float Pitch)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || EffectsVolume <= 0.f)
	{
		return nullptr;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSound2D(World, MakeWave(Sound, false), Volume * EffectsVolume, Pitch,
		0.f, nullptr, false, true);
	if (Component != nullptr)
	{
		OneShots.Add({ Component, World->GetTimeSeconds() + BoatAudio::GetDuration(Sound) / FMath::Max(Pitch, 0.1f) + 0.15 });
	}
	return Component;
}

UAudioComponent* UBoatAudioSubsystem::MakeLoop(USceneComponent* AttachTo, EBoatSound Sound, bool bPositional)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer || AttachTo == nullptr)
	{
		return nullptr;
	}
	if (NearAttenuation == nullptr)
	{
		NearAttenuation = NewObject<USoundAttenuation>(this);
		NearAttenuation->Attenuation.bAttenuate = true;
		NearAttenuation->Attenuation.bSpatialize = true;
		NearAttenuation->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		NearAttenuation->Attenuation.AttenuationShapeExtents = FVector(1800.f, 0.f, 0.f);
		NearAttenuation->Attenuation.FalloffDistance = 14000.f;
	}
	UAudioComponent* Component = NewObject<UAudioComponent>(AttachTo->GetOwner());
	Component->bAutoActivate = false;
	Component->bAutoDestroy = false;
	Component->bAllowSpatialization = bPositional;
	if (bPositional)
	{
		Component->AttenuationSettings = NearAttenuation;
	}
	Component->SetupAttachment(AttachTo);
	Component->SetSound(MakeWave(Sound, true));
	Component->RegisterComponent();
	return Component;
}

void UBoatAudioSubsystem::ToggleMusic()
{
	bMusicOn = !bMusicOn;
	if (MusicLoop != nullptr)
	{
		MusicLoop->SetVolumeMultiplier(bMusicOn ? MusicVolume : 0.f);
	}
}

void UBoatAudioSubsystem::QueueThunder(float DelaySeconds, float Volume)
{
	if (const UWorld* World = GetWorld())
	{
		PendingThunder.Add({ World->GetTimeSeconds() + DelaySeconds, Volume });
	}
}

void UBoatAudioSubsystem::StartBeds()
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	auto MakeBed = [this, World](EBoatSound Sound, float Volume)
	{
		UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, MakeWave(Sound, true), Volume, 1.f, 0.f, nullptr, false, false);
		if (Component != nullptr)
		{
			Component->Play();
		}
		return Component;
	};
	MusicLoop = MakeBed(EBoatSound::Music, bMusicOn ? MusicVolume : 0.f);
	SeaLoop = MakeBed(EBoatSound::Sea, 0.35f * EffectsVolume);
	RainLoop = MakeBed(EBoatSound::Rain, 0.f);
	WindLoop = MakeBed(EBoatSound::Wind, 0.f);
	bBedsStarted = true;
	UE_LOG(LogBoatRace, Log, TEXT("Audio started: music %.0f s loop, sea, rain and wind beds."), BoatAudio::GetDuration(EBoatSound::Music));
}

void UBoatAudioSubsystem::UpdateBeds(float DeltaTime)
{
	const UWorld* World = GetWorld();
	const ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	const ERaceWeather Weather = RaceState ? RaceState->GetWeather() : ERaceWeather::Sunny;
	const FWeatherLook& Look = RaceWeather::Get(Weather);

	const float RainTarget = Look.Precipitation == EPrecipitation::Rain ? (Look.bLightning ? 0.9f : 0.6f) : 0.f;
	const float WindTarget = FMath::Clamp((Look.Wind - 3.f) / 11.f, 0.f, 1.f) * 0.7f;
	RainLevel = FMath::FInterpTo(RainLevel, RainTarget, DeltaTime, 0.8f);
	WindLevel = FMath::FInterpTo(WindLevel, WindTarget, DeltaTime, 0.8f);
	if (RainLoop != nullptr)
	{
		RainLoop->SetVolumeMultiplier(FMath::Max(RainLevel * EffectsVolume, 0.001f));
	}
	if (WindLoop != nullptr)
	{
		WindLoop->SetVolumeMultiplier(FMath::Max(WindLevel * EffectsVolume, 0.001f));
	}

	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Look.bGulls && Now > NextGullTime)
	{
		NextGullTime = Now + FMath::FRandRange(7.f, 16.f);
		Play2D(EBoatSound::Gull, FMath::FRandRange(0.15f, 0.3f), FMath::FRandRange(0.85f, 1.15f));
	}
}

void UBoatAudioSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!bBedsStarted && World->GetFirstPlayerController() != nullptr && World->HasBegunPlay())
	{
		StartBeds();
	}
	if (bBedsStarted)
	{
		UpdateBeds(DeltaTime);
	}

	const double Now = World->GetTimeSeconds();
	for (int32 Index = PendingThunder.Num() - 1; Index >= 0; --Index)
	{
		if (Now >= PendingThunder[Index].Key)
		{
			Play2D(EBoatSound::Thunder, PendingThunder[Index].Value, FMath::FRandRange(0.85f, 1.1f));
			PendingThunder.RemoveAtSwap(Index);
		}
	}

	for (int32 Index = OneShots.Num() - 1; Index >= 0; --Index)
	{
		FOneShot& Shot = OneShots[Index];
		if (!Shot.Component.IsValid())
		{
			OneShots.RemoveAtSwap(Index);
		}
		else if (Now >= Shot.StopAt)
		{
			Shot.Component->Stop();
			OneShots.RemoveAtSwap(Index);
		}
	}
}

void BoatAudio::PlayAt(const UObject* WorldContext, EBoatSound Sound, const FVector& Location, float Volume, float Pitch)
{
	if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(WorldContext))
	{
		Audio->PlayAt(Sound, Location, Volume, Pitch);
	}
}

void BoatAudio::Play2D(const UObject* WorldContext, EBoatSound Sound, float Volume, float Pitch)
{
	if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(WorldContext))
	{
		Audio->Play2D(Sound, Volume, Pitch);
	}
}
