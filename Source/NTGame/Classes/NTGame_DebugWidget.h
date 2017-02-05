// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "NTGame_DebugWidget.generated.h"

class UTextBlock;

UCLASS()
class NTGAME_API UNTGame_DebugWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UNTGame_DebugWidget(const FObjectInitializer& OI);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	///////////////////
	///// Updates /////
	///////////////////

	void UpdateDynamicData();

	////////////////////
	///// Pointers /////
	////////////////////

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TitleText;

	// Move Info
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InputText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* AccelText;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* AlphaText;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* VelocText;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* OmegaText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PacketLag;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PacketLoss;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PacketDupe;
	UPROPERTY(meta = (BindWidget))
	UTextBlock* PacketOrder;
};