#include "LanDiscovery.h"

#include "BoatRace.h"
#include "BoatRaceGameState.h"
#include "BoatTrack.h"

#include "Common/UdpSocketBuilder.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformProcess.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

namespace
{
	// Versioned so that a future change to the format is ignored by old copies
	// instead of being misread.
	const TCHAR* const Signature = TEXT("BOATRACE/1");
	const double AnnounceInterval = 1.0;
	// Three missed announcements and the race is taken off the list: the host
	// quit, or the race moved to another network.
	const double ForgetAfter = 3.5;
	const int32 MaxPacket = 512;

	FString GetHostDisplayName()
	{
		FString User = FPlatformProcess::UserName(false);
		if (User.IsEmpty())
		{
			User = FPlatformProcess::ComputerName();
		}
		if (User.Len() > 0)
		{
			User[0] = FChar::ToUpper(User[0]);
		}
		return User.Left(24) + TEXT("'s race");
	}
}

void ULanDiscovery::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Four times a second is quick enough for a menu list and costs nothing.
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ULanDiscovery::Tick), 0.25f);
}

void ULanDiscovery::Deinitialize()
{
	FTSTicker::RemoveTicker(TickHandle);
	CloseSockets();
	Super::Deinitialize();
}

bool ULanDiscovery::Tick(float DeltaTime)
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return true;
	}

	switch (World->GetNetMode())
	{
	case NM_ListenServer:
		Announce();
		break;
	case NM_Standalone:
		Listen();
		break;
	default:
		// Connected to someone else's race: nothing to find or announce.
		FoundRaces.Reset();
		break;
	}
	return true;
}

void ULanDiscovery::Announce()
{
	const double Now = FPlatformTime::Seconds();
	if (Now < NextAnnounceTime)
	{
		return;
	}
	NextAnnounceTime = Now + AnnounceInterval;

	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Sockets == nullptr)
	{
		return;
	}
	if (SendSocket == nullptr)
	{
		SendSocket = FUdpSocketBuilder(TEXT("BoatRaceAnnounce")).AsNonBlocking().AsReusable().WithBroadcast();
		if (SendSocket == nullptr)
		{
			UE_LOG(LogBoatRace, Warning, TEXT("Could not open a socket to announce the race; others will have to join by address."));
			NextAnnounceTime = Now + 30.0;
			return;
		}
	}

	const UWorld* World = GetGameInstance()->GetWorld();
	const ABoatRaceGameState* RaceState = World->GetGameState<ABoatRaceGameState>();
	// People only: computer rivals step aside when someone joins.
	int32 Players = 1;
	if (RaceState != nullptr)
	{
		Players = 0;
		for (const APlayerState* Player : RaceState->PlayerArray)
		{
			Players += (Player != nullptr && !Player->IsABot()) ? 1 : 0;
		}
	}
	const bool bRacing = RaceState && RaceState->RaceState != ERaceState::Lobby;

	// Name goes last so that nothing in it can upset the fields before it.
	const FString Message = FString::Printf(TEXT("%s|%d|%d|%d|%d|%s"),
		Signature, World->URL.Port, Players, BoatTrack::MaxPlayers, bRacing ? 1 : 0, *GetHostDisplayName());
	const FTCHARToUTF8 Utf8(*Message);

	TSharedRef<FInternetAddr> Everyone = Sockets->CreateInternetAddr();
	Everyone->SetBroadcastAddress();
	Everyone->SetPort(DiscoveryPort);

	int32 BytesSent = 0;
	SendSocket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), BytesSent, *Everyone);
}

void ULanDiscovery::Listen()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (Sockets == nullptr)
	{
		return;
	}
	if (ListenSocket == nullptr)
	{
		// Reusable, so that several copies on one machine (test_4_players.sh)
		// can all listen; Linux hands a broadcast to every one of them.
		ListenSocket = FUdpSocketBuilder(TEXT("BoatRaceListen")).AsNonBlocking().AsReusable()
			.WithBroadcast().BoundToPort(DiscoveryPort);
		if (ListenSocket == nullptr)
		{
			return;
		}
	}

	const double Now = FPlatformTime::Seconds();
	TSharedRef<FInternetAddr> Sender = Sockets->CreateInternetAddr();
	uint8 Buffer[MaxPacket + 1];
	uint32 Pending = 0;
	while (ListenSocket->HasPendingData(Pending))
	{
		int32 BytesRead = 0;
		if (!ListenSocket->RecvFrom(Buffer, MaxPacket, BytesRead, *Sender) || BytesRead <= 0)
		{
			break;
		}
		Buffer[BytesRead] = 0;
		const FString Message = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Buffer)));

		TArray<FString> Fields;
		Message.ParseIntoArray(Fields, TEXT("|"), false);
		if (Fields.Num() < 6 || Fields[0] != Signature)
		{
			continue;
		}

		const int32 Port = FCString::Atoi(*Fields[1]);
		const FString Address = FString::Printf(TEXT("%s:%d"), *Sender->ToString(false), Port > 0 ? Port : 7777);

		FFoundRace* Race = FoundRaces.FindByPredicate([&Address](const FFoundRace& Known) { return Known.Address == Address; });
		if (Race == nullptr)
		{
			Race = &FoundRaces.AddDefaulted_GetRef();
			Race->Address = Address;
			UE_LOG(LogBoatRace, Log, TEXT("Found a race on the network at %s"), *Address);
		}
		Race->Players = FCString::Atoi(*Fields[2]);
		Race->MaxPlayers = FCString::Atoi(*Fields[3]);
		Race->bRacing = Fields[4] == TEXT("1");
		// Rejoin the name in case it contained the separator.
		Race->Name = FString::Join(TArrayView<const FString>(Fields).RightChop(5), TEXT("|"));
		Race->LastHeard = Now;
	}

	FoundRaces.RemoveAll([Now](const FFoundRace& Race) { return Now - Race.LastHeard > ForgetAfter; });
	FoundRaces.Sort([](const FFoundRace& A, const FFoundRace& B) { return A.Address < B.Address; });
}

void ULanDiscovery::CloseSockets()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	for (FSocket** Socket : { &SendSocket, &ListenSocket })
	{
		if (*Socket != nullptr)
		{
			(*Socket)->Close();
			if (Sockets != nullptr)
			{
				Sockets->DestroySocket(*Socket);
			}
			*Socket = nullptr;
		}
	}
}
