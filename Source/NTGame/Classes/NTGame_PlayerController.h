// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "NTGame_PlayerController.generated.h"

UCLASS()
class NTGAME_API ANTGame_PlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ANTGame_PlayerController(const FObjectInitializer& OI);

	// APlayerController Interface
	virtual void SetupInputComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const override;

	FORCEINLINE uint8 GetPawnTypeIndex() const { return CurrentPawnIndex; }

protected:
	/////////////////////////////
	///// Packet Loss Input /////
	/////////////////////////////

	UFUNCTION()	void IncreasePacketLag();
	UFUNCTION()	void DecreasePacketLag();

	UFUNCTION()	void IncreasePacketLagVariance();
	UFUNCTION()	void DecreasePacketLagVariance();

	UFUNCTION()	void IncreasePacketLoss();
	UFUNCTION()	void DecreasePacketLoss();

	UFUNCTION()	void IncreasePacketDupe();
	UFUNCTION()	void DecreasePacketDupe();

	UFUNCTION()	void TogglePacketOrder();
	UFUNCTION()	void ResetPacketSettings();

	///////////////////////////////
	///// Debug Drawing Input /////
	///////////////////////////////

	UFUNCTION()	void ToggleDebugDrawing();

	/////////////////////////
	///// Changing Pawn /////
	/////////////////////////

	UFUNCTION()	void UseBoxPawn() { RequestPawnChange(0); }
	UFUNCTION()	void UseSpherePawn() { RequestPawnChange(1); }

	UFUNCTION(BlueprintCallable, Category = "PawnType")
	void RequestPawnChange(const uint8 PawnID);

	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestPawnChange(const uint8 PawnID);
	void Server_RequestPawnChange_Implementation(const uint8 PawnID);
	bool Server_RequestPawnChange_Validate(const uint8 PawnID) { return true; }
	
	// So Client can't spam server with RPC's
	UPROPERTY(Replicated, Transient)
	uint8 CurrentPawnIndex;
};