// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_PlayerController.h"

ANTGame_PlayerController::ANTGame_PlayerController(const FObjectInitializer& OI)
	: Super(OI)
{}

/////////////////
///// Input /////
/////////////////

void ANTGame_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	ASSERTV(InputComponent != nullptr, TEXT("Invalid Player Input Component"));

	InputComponent->BindAction(TEXT("PacketLag_Up"), IE_Pressed, this, &ANTGame_PlayerController::IncreasePacketLag);
	InputComponent->BindAction(TEXT("PacketLag_Down"), IE_Pressed, this, &ANTGame_PlayerController::DecreasePacketLag);
	InputComponent->BindAction(TEXT("PacketLagVar_Up"), IE_Pressed, this, &ANTGame_PlayerController::IncreasePacketLagVariance);
	InputComponent->BindAction(TEXT("PacketLagVar_Down"), IE_Pressed, this, &ANTGame_PlayerController::DecreasePacketLagVariance);
	InputComponent->BindAction(TEXT("PacketLoss_Up"), IE_Pressed, this, &ANTGame_PlayerController::IncreasePacketLoss);
	InputComponent->BindAction(TEXT("PacketLoss_Down"), IE_Pressed, this, &ANTGame_PlayerController::DecreasePacketLoss);
	InputComponent->BindAction(TEXT("PacketDupe_Up"), IE_Pressed, this, &ANTGame_PlayerController::IncreasePacketDupe);
	InputComponent->BindAction(TEXT("PacketDupe_Down"), IE_Pressed, this, &ANTGame_PlayerController::DecreasePacketDupe);
	InputComponent->BindAction(TEXT("PacketOrder_Toggle"), IE_Pressed, this, &ANTGame_PlayerController::TogglePacketOrder);
	InputComponent->BindAction(TEXT("PacketReset"), IE_Pressed, this, &ANTGame_PlayerController::ResetPacketSettings);

	InputComponent->BindAction(TEXT("ToggleDebug"), IE_Pressed, this, &ANTGame_PlayerController::ToggleDebugDrawing);

	InputComponent->BindAction(TEXT("UseBoxPawn"), IE_Pressed, this, &ANTGame_PlayerController::UseBoxPawn);
	InputComponent->BindAction(TEXT("UseSpherePawn"), IE_Pressed, this, &ANTGame_PlayerController::UseSpherePawn);
}

/////////////////////////////
///// Packet Loss Input /////
/////////////////////////////

void ANTGame_PlayerController::IncreasePacketLag()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLag + 25, 0, 500));		
	ConsoleCommand(TEXT("Net PktLag=") + CommandValue, false);
}

void ANTGame_PlayerController::DecreasePacketLag()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLag - 25, 0, 500));
	ConsoleCommand(TEXT("Net PktLag=") + CommandValue, false);
}

void ANTGame_PlayerController::IncreasePacketLagVariance()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLagVariance + 10, 0, 100));
	ConsoleCommand(TEXT("Net PktLagVariance=") + CommandValue, false);
}

void ANTGame_PlayerController::DecreasePacketLagVariance()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLagVariance - 10, 0, 100));
	ConsoleCommand(TEXT("Net PktLagVariance=") + CommandValue, false);
}

void ANTGame_PlayerController::IncreasePacketLoss()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLoss + 10, 0, 100));
	ConsoleCommand(TEXT("Net PktLoss=") + CommandValue, false);
}

void ANTGame_PlayerController::DecreasePacketLoss()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktLoss - 10, 0, 100));
	ConsoleCommand(TEXT("Net PktLoss=") + CommandValue, false);
}

void ANTGame_PlayerController::IncreasePacketDupe()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktDup + 10, 0, 100));
	ConsoleCommand(TEXT("Net PktDup=") + CommandValue, false);
}

void ANTGame_PlayerController::DecreasePacketDupe()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const FString CommandValue = FString::FromInt(FMath::Clamp(WorldNetDriver->PacketSimulationSettings.PktDup - 10, 0, 100));
	ConsoleCommand(TEXT("Net PktDup=") + CommandValue, false);
}

void ANTGame_PlayerController::TogglePacketOrder()
{
	const UNetDriver* WorldNetDriver = GetWorld()->GetNetDriver();
	ASSERTV(WorldNetDriver != nullptr, TEXT("Invalid Network Driver"));

	const int32 CurrentValue = WorldNetDriver->PacketSimulationSettings.PktOrder;
	const FString CommandValue = FString::FromInt(CurrentValue > 0 ? 0 : 1);

	ConsoleCommand(TEXT("Net PktOrder=") + CommandValue, false);
}

void ANTGame_PlayerController::ResetPacketSettings()
{
	ConsoleCommand(TEXT("Net PktLag=0"), false);
	ConsoleCommand(TEXT("Net PktLagVariance=0"), false);
	ConsoleCommand(TEXT("Net PktLoss=0"), false);
	ConsoleCommand(TEXT("Net PktDup=0"), false);
	ConsoleCommand(TEXT("Net PktOrder=0"), false);
}

///////////////////////////////
///// Debug Drawing Input /////
///////////////////////////////

void ANTGame_PlayerController::ToggleDebugDrawing()
{
	ANTGame_Pawn* MyNTPawn = Cast<ANTGame_Pawn>(GetPawn());
	if (MyNTPawn)
	{
		MyNTPawn->GetPhysicsMovement()->bDrawDebug = !MyNTPawn->GetPhysicsMovement()->bDrawDebug;
	}
}

void ANTGame_PlayerController::RequestPawnChange(const uint8 PawnID)
{
	if (CurrentPawnIndex != PawnID)
	{
		Server_RequestPawnChange(PawnID);
	}
}

void ANTGame_PlayerController::Server_RequestPawnChange_Implementation(const uint8 PawnID)
{
	CurrentPawnIndex = PawnID;

	ANTGame_GameMode* WorldGM = Cast<ANTGame_GameMode>(GetWorld()->GetAuthGameMode());
	ASSERTV(WorldGM != nullptr, TEXT("Invalid Game Mode"));

	if (GetPawn())
	{
		// Will unpossess and properly detach from pawn
		GetPawn()->Destroy();
	}
	WorldGM->RestartPlayer(this);
}

///////////////////////
///// Replication /////
///////////////////////

void ANTGame_PlayerController::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ANTGame_PlayerController, CurrentPawnIndex, COND_OwnerOnly);
}