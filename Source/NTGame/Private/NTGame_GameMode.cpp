// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_GameMode.h"

// For Default Classes
#include "NTGame_PlayerController.h"
#include "NTGame_DebugHUD.h"
#include "NTGame_Pawn.h"

ANTGame_GameMode::ANTGame_GameMode(const FObjectInitializer& OI)
	: Super(OI)
{
	PlayerControllerClass = ANTGame_PlayerController::StaticClass();
	HUDClass = ANTGame_DebugHUD::StaticClass();
	DefaultPawnClass = ANTGame_Pawn::StaticClass();
}

/////////////////////////
///// Pawn Spawning /////
/////////////////////////

APawn* ANTGame_GameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	// don't allow pawn to be spawned with any pitch or roll
	FRotator StartRotation(ForceInit);
	StartRotation.Yaw = StartSpot->GetActorRotation().Yaw;
	FVector StartLocation = StartSpot->GetActorLocation();

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save default player pawns into a map
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	APawn* ResultPawn = GetWorld()->SpawnActor<APawn>(PawnClass, StartLocation, StartRotation, SpawnInfo);
	return ResultPawn;
}

UClass* ANTGame_GameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	const ANTGame_PlayerController* MyPC = Cast<ANTGame_PlayerController>(InController);
	if (MyPC != nullptr)
	{
		const uint8 PawnID = MyPC->GetPawnTypeIndex();
		if (AvailablePawns.IsValidIndex(PawnID))
		{
			return AvailablePawns[PawnID];
		}
		else
		{
			return DefaultPawnClass;
		}
	}

	return DefaultPawnClass;
}