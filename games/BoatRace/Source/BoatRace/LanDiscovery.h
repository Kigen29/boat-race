#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LanDiscovery.generated.h"

class FSocket;

/** A race heard on the local network. */
struct FFoundRace
{
	FString Name;
	/** "ip:port", ready to hand to ClientTravel. */
	FString Address;
	int32 Players = 0;
	int32 MaxPlayers = 0;
	bool bRacing = false;
	double LastHeard = 0.0;
};

/**
 * Finds races on the same Wi-Fi so nobody has to look up or type an address.
 *
 * A host shouts a one-line announcement to the whole network once a second;
 * every copy of the game sitting on the main menu listens for it. This is
 * plain UDP broadcast rather than an online subsystem, because the only job is
 * "who is hosting in this house" and broadcast needs no accounts, no server
 * and no plugin. It cannot cross the internet, which is the point: anything
 * it finds is close enough to join directly.
 *
 * A game-instance subsystem, because it has to keep running across the map
 * reload that turns the menu into a hosted race.
 */
UCLASS()
class ULanDiscovery : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Races heard in the last few seconds, in a stable order. */
	const TArray<FFoundRace>& GetFoundRaces() const { return FoundRaces; }

	/** UDP port the announcements use. The race itself stays on 7777. */
	static constexpr int32 DiscoveryPort = 7778;

private:
	bool Tick(float DeltaTime);
	void Announce();
	void Listen();
	void CloseSockets();

	FSocket* SendSocket = nullptr;
	FSocket* ListenSocket = nullptr;
	FTSTicker::FDelegateHandle TickHandle;
	double NextAnnounceTime = 0.0;
	TArray<FFoundRace> FoundRaces;
};
