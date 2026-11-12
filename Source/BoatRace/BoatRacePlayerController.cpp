#include "BoatRacePlayerController.h"

#include "BoatRace.h"
#include "BoatRaceGameMode.h"

#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

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
	}
	else if (WasInputKeyJustPressed(EKeys::L))
	{
		LeaveRace();
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

void ABoatRacePlayerController::ServerRequestReset_Implementation()
{
	UWorld* World = GetWorld();
	if (ABoatRaceGameMode* RaceMode = World ? World->GetAuthGameMode<ABoatRaceGameMode>() : nullptr)
	{
		RaceMode->RequestResetBoat(this);
	}
}
