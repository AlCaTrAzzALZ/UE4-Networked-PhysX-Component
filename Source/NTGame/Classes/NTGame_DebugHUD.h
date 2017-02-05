// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "GameFramework/HUD.h"
#include "NTGame_DebugHUD.generated.h"

// Declarations
class UNTGame_DebugWidget;

UCLASS()
class NTGAME_API ANTGame_DebugHUD : public AHUD
{
	GENERATED_BODY()
		
public:
	//////////////////////////
	///// Initialization /////
	//////////////////////////

	ANTGame_DebugHUD(const FObjectInitializer& OI);

	virtual void BeginPlay() override;

	////////////////////////
	///// Debug Widget /////
	////////////////////////
public:
	FORCEINLINE UNTGame_DebugWidget* GetDebugWidget() const { return ActiveWidget_Debug; }
	void ToggleDebugWidget(const bool bNewVisible);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Widgets")
	TSubclassOf<UNTGame_DebugWidget> WidgetAsset_Debug;

	UPROPERTY(Transient)
	UNTGame_DebugWidget* ActiveWidget_Debug;

	void CreateDebugWidget();
};