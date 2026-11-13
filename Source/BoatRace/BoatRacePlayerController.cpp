#include "BoatRacePlayerController.h"

#include "BayTerrain.h"
#include "BoatAudio.h"
#include "BoatPawn.h"
#include "BoatProjectile.h"
#include "BoatRace.h"
#include "BoatRaceGameMode.h"
#include "BoatRaceGameState.h"
#include "BoatTrack.h"
#include "LanDiscovery.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "UnrealClient.h"

namespace
{
	// The world is built in code, so every "level" is the engine's empty map.
	const TCHAR* const RaceMap = TEXT("/Engine/Maps/Entry");
	const int32 MaxAddressLength = 60;

	struct FTypedKey
	{
		FKey Key;
		TCHAR Character;
	};

	/** Keys accepted while typing a host address (IP, host name or host:port). */
	const TArray<FTypedKey>& GetAddressKeys()
	{
		static TArray<FTypedKey> Keys;
		if (Keys.Num() == 0)
		{
			Keys.Add(FTypedKey{ EKeys::Zero, TEXT('0') });
			Keys.Add(FTypedKey{ EKeys::One, TEXT('1') });
			Keys.Add(FTypedKey{ EKeys::Two, TEXT('2') });
			Keys.Add(FTypedKey{ EKeys::Three, TEXT('3') });
			Keys.Add(FTypedKey{ EKeys::Four, TEXT('4') });
			Keys.Add(FTypedKey{ EKeys::Five, TEXT('5') });
			Keys.Add(FTypedKey{ EKeys::Six, TEXT('6') });
			Keys.Add(FTypedKey{ EKeys::Seven, TEXT('7') });
			Keys.Add(FTypedKey{ EKeys::Eight, TEXT('8') });
			Keys.Add(FTypedKey{ EKeys::Nine, TEXT('9') });
			Keys.Add(FTypedKey{ EKeys::NumPadZero, TEXT('0') });
			Keys.Add(FTypedKey{ EKeys::NumPadOne, TEXT('1') });
			Keys.Add(FTypedKey{ EKeys::NumPadTwo, TEXT('2') });
			Keys.Add(FTypedKey{ EKeys::NumPadThree, TEXT('3') });
			Keys.Add(FTypedKey{ EKeys::NumPadFour, TEXT('4') });
			Keys.Add(FTypedKey{ EKeys::NumPadFive, TEXT('5') });
			Keys.Add(FTypedKey{ EKeys::NumPadSix, TEXT('6') });
			Keys.Add(FTypedKey{ EKeys::NumPadSeven, TEXT('7') });
			Keys.Add(FTypedKey{ EKeys::NumPadEight, TEXT('8') });
			Keys.Add(FTypedKey{ EKeys::NumPadNine, TEXT('9') });
			Keys.Add(FTypedKey{ EKeys::Period, TEXT('.') });
			Keys.Add(FTypedKey{ EKeys::Decimal, TEXT('.') });
			Keys.Add(FTypedKey{ EKeys::Hyphen, TEXT('-') });
			Keys.Add(FTypedKey{ EKeys::Subtract, TEXT('-') });
			Keys.Add(FTypedKey{ EKeys::Semicolon, TEXT(':') }); // for host:port

			const FKey Letters[] =
			{
				EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F, EKeys::G,
				EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L, EKeys::M, EKeys::N,
				EKeys::O, EKeys::P, EKeys::Q, EKeys::R, EKeys::S, EKeys::T, EKeys::U,
				EKeys::V, EKeys::W, EKeys::X, EKeys::Y, EKeys::Z
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Letters); ++Index)
			{
				Keys.Add(FTypedKey{ Letters[Index], static_cast<TCHAR>(TEXT('a') + Index) });
			}
		}
		return Keys;
	}
}

void ABoatRacePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
	{
		return;
	}

	if (bTypingAddress)
	{
		HandleAddressTyping();
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("PhotoTour")))
	{
		RunPhotoTour(DeltaTime);
	}

	// TAB: the host picks the weather between races (SHIFT+TAB goes back).
	if (WasInputKeyJustPressed(EKeys::Tab))
	{
		ServerRequestWeather(IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift) ? -1 : 1);
	}
	if (WasInputKeyJustPressed(EKeys::M))
	{
		if (UBoatAudioSubsystem* Audio = UBoatAudioSubsystem::Get(this))
		{
			Audio->ToggleMusic();
		}
	}

	if (WasInputKeyJustPressed(EKeys::Enter) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
	{
		ServerRequestStart();
	}
	if (WasInputKeyJustPressed(EKeys::R) || WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
	{
		ServerRequestReset();
	}

	if (GetNetMode() == NM_Standalone)
	{
		// Main menu: not hosting and not connected to anyone.
		if (WasInputKeyJustPressed(EKeys::H))
		{
			HostRace();
		}
		else if (WasInputKeyJustPressed(EKeys::J))
		{
			bTypingAddress = true;
		}
		else
		{
			JoinFoundRaceOnKeyPress();
		}
	}
	else if (WasInputKeyJustPressed(EKeys::L))
	{
		LeaveRace();
	}
}

void ABoatRacePlayerController::JoinFoundRaceOnKeyPress()
{
	const ULanDiscovery* Discovery = GetGameInstance() ? GetGameInstance()->GetSubsystem<ULanDiscovery>() : nullptr;
	if (Discovery == nullptr)
	{
		return;
	}

	const FKey NumberKeys[][2] =
	{
		{ EKeys::One, EKeys::NumPadOne },
		{ EKeys::Two, EKeys::NumPadTwo },
		{ EKeys::Three, EKeys::NumPadThree },
		{ EKeys::Four, EKeys::NumPadFour },
	};
	const TArray<FFoundRace>& Races = Discovery->GetFoundRaces();
	for (int32 Index = 0; Index < FMath::Min(Races.Num(), BoatTrack::MaxPlayers); ++Index)
	{
		if (WasInputKeyJustPressed(NumberKeys[Index][0]) || WasInputKeyJustPressed(NumberKeys[Index][1]))
		{
			JoinRace(Races[Index].Address);
			return;
		}
	}
}

void ABoatRacePlayerController::HandleAddressTyping()
{
	if (WasInputKeyJustPressed(EKeys::Escape))
	{
		bTypingAddress = false;
		return;
	}
	if (WasInputKeyJustPressed(EKeys::BackSpace))
	{
		if (TypedAddress.Len() > 0)
		{
			TypedAddress.LeftChopInline(1);
		}
		return;
	}
	if (WasInputKeyJustPressed(EKeys::Enter))
	{
		if (TypedAddress.Len() > 0)
		{
			bTypingAddress = false;
			JoinRace(TypedAddress);
		}
		return;
	}

	for (const FTypedKey& Typed : GetAddressKeys())
	{
		if (WasInputKeyJustPressed(Typed.Key) && TypedAddress.Len() < MaxAddressLength)
		{
			TypedAddress.AppendChar(Typed.Character);
		}
	}
}

void ABoatRacePlayerController::HostRace()
{
	// Reload the (empty) map as a listen server. This machine now plays and
	// accepts up to three other players on UDP port 7777.
	UE_LOG(LogBoatRace, Log, TEXT("Hosting a race."));
	UGameplayStatics::OpenLevel(this, FName(RaceMap), true, TEXT("listen"));
}

void ABoatRacePlayerController::JoinRace(const FString& Address)
{
	UE_LOG(LogBoatRace, Log, TEXT("Joining %s"), *Address);
	ClientTravel(Address, TRAVEL_Absolute);
}

void ABoatRacePlayerController::LeaveRace()
{
	// Back to the stand-alone menu. If this machine was the host, the race ends for everyone.
	ClientTravel(RaceMap, TRAVEL_Absolute);
}

void ABoatRacePlayerController::ServerRequestStart_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestStartRace(this);
	}
}

void ABoatRacePlayerController::ServerRequestWeather_Implementation(int32 Step)
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestWeatherChange(this, FMath::Clamp(Step, -1, 1));
	}
}

void ABoatRacePlayerController::RunPhotoTour(float DeltaTime)
{
	// Starts a race with rivals, photographs the grid under the start lights,
	// then visits a different stretch of the course in each kind of weather,
	// and quits. Screenshots land in Saved/Screenshots.
	UWorld* World = GetWorld();
	ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr;
	ABoatRaceGameState* RaceState = World ? World->GetGameState<ABoatRaceGameState>() : nullptr;
	ABoatPawn* Boat = Cast<ABoatPawn>(GetPawn());
	if (RaceMode == nullptr || RaceState == nullptr || Boat == nullptr)
	{
		return;
	}
	if (TourClock < 0.f)
	{
		TourClock = 0.f;
		RaceMode->RequestStartRace(this);
	}
	TourClock += DeltaTime;

	const float StepSeconds = 7.f;
	const int32 Weathers = static_cast<int32>(ERaceWeather::Count);
	const int32 Step = FMath::FloorToInt((TourClock - 2.f) / StepSeconds);
	if (Step != TourStep && TourClock > 2.f)
	{
		TourStep = Step;
		if (Step == 0)
		{
			FScreenshotRequest::RequestScreenshot(TEXT("Tour_00_grid"), false, false);
			return;
		}
		if (Step > Weathers + 2)
		{
			ConsoleCommand(TEXT("quit"));
			return;
		}
		// Weather can only change between races, so do it as the server would.
		RaceState->SetWeather(RaceWeather::FromIndex((Step - 1) % Weathers));
		const int32 Gates = BoatTrack::NumGates();
		const FTransform Spot = BoatTrack::GetGateResetTransform((Step * 5) % Gates);
		Boat->TeleportBoat(Spot.GetLocation(), static_cast<float>(Spot.Rotator().Yaw));
	}
	// The last two stops set off a blast ahead of the boat to show the explosion.
	static int32 BlastStep = -1;
	const float IntoStep = TourClock - 2.f - TourStep * StepSeconds;
	if (TourStep > Weathers && TourStep != BlastStep && IntoStep > (TourStep == Weathers + 1 ? 3.5f : 3.f))
	{
		BlastStep = TourStep;
		BoatBlast::Detonate(World, Boat->GetActorLocation() + Boat->GetActorForwardVector() * 3500.f, 900.f, 0.f, 0.f, Boat);
	}

	// Shoot halfway through each step, once the boat has settled and moved off.
	static int32 ShotStep = -1;
	if (TourStep > 0 && TourStep != ShotStep && TourClock - 2.f - TourStep * StepSeconds > 4.f)
	{
		ShotStep = TourStep;
		const FString Name = TourStep <= Weathers
			? FString::Printf(TEXT("Tour_%02d_%s"), TourStep, RaceWeather::Get(RaceState->GetWeather()).Name)
			: FString::Printf(TEXT("Tour_%02d_extra"), TourStep);
		FScreenshotRequest::RequestScreenshot(Name, false, false);
		const FVector Where = Boat->GetActorLocation();
		UE_LOG(LogBoatRace, Log, TEXT("Photo tour: %s, %.0f fps; boat %.0f km/h, throttle %.2f, ground %.0f cm, at %.0f %.0f %.0f"),
			*Name, 1.f / FMath::Max(DeltaTime, 0.001f), Boat->GetSpeedKmh(), Boat->GetThrottleInput(),
			BayTerrain::GetHeight(FVector2D(Where.X, Where.Y)), Where.X, Where.Y, Where.Z);
	}
}

void ABoatRacePlayerController::ServerRequestReset_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestResetBoat(this);
	}
}
