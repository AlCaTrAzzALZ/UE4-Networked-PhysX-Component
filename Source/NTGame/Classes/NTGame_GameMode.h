// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "GameFramework/GameMode.h"
#include "NTGame_GameMode.generated.h"

// Declarations
class ANTGame_Pawn;

UCLASS()
class NTGAME_API ANTGame_GameMode : public AGameMode
{
	GENERATED_BODY()
public:
	ANTGame_GameMode(const FObjectInitializer& OI);

	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Pawns")
	TArray<TSubclassOf<ANTGame_Pawn>> AvailablePawns;
};