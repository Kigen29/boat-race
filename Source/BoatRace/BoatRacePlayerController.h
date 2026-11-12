#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoatRacePlayerController.generated.h"

/**
 * Handles the keys that are not about driving: hosting, joining by address,
 * starting a race, resetting the boat and leaving.
 */
UCLASS()
class ABoatRacePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;

	/** True while the player is typing a host address on the menu. */
	bool IsTypingAddress() const { return bTypingAddress; }
	const FString& GetTypedAddress() const { return TypedAddress; }

protected:
	UFUNCTION(Server, Reliable)
	void ServerRequestStart();

	UFUNCTION(Server, Reliable)
	void ServerRequestReset();

private:
	void HandleAddressTyping();
	void HostRace();
	void JoinRace(const FString& Address);
	void LeaveRace();

	bool bTypingAddress = false;
	FString TypedAddress;
};
