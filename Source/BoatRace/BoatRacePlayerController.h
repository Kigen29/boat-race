#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BoatRacePlayerController.generated.h"

/**
 * Handles the keys that are not about driving: hosting, joining a race found
 * on the network or by address,
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
	/** Number keys on the menu join the races listed under "on your Wi-Fi". */
	void JoinFoundRaceOnKeyPress();
	void HostRace();
	void JoinRace(const FString& Address);
	void LeaveRace();

	bool bTypingAddress = false;
	FString TypedAddress;
};
