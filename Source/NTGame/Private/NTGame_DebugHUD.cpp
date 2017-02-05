// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_DebugHUD.h"

// Widgets
#include "NTGame_DebugWidget.h"

ANTGame_DebugHUD::ANTGame_DebugHUD(const FObjectInitializer& OI)
	: Super(OI)
{
	WidgetAsset_Debug = nullptr;
	ActiveWidget_Debug = nullptr;
}

void ANTGame_DebugHUD::BeginPlay()
{
	Super::BeginPlay();

	ToggleDebugWidget(true);
}

////////////////////////
///// Debug Widget /////
////////////////////////

void ANTGame_DebugHUD::ToggleDebugWidget(const bool bNewVisible)
{
	if ((bNewVisible && ActiveWidget_Debug) || (!bNewVisible && !ActiveWidget_Debug)) { return; }

	if (bNewVisible)
	{
		CreateDebugWidget();
	}
	else
	{
		ActiveWidget_Debug->RemoveFromParent();
		ActiveWidget_Debug = nullptr;
	}
}

void ANTGame_DebugHUD::CreateDebugWidget()
{
	ASSERTV(ActiveWidget_Debug == nullptr, TEXT("Debug Widget Already Initialized"));

	ActiveWidget_Debug = CreateWidget<UNTGame_DebugWidget>(GetOwningPlayerController(), WidgetAsset_Debug);
	ASSERTV(ActiveWidget_Debug != nullptr, TEXT("Unable To Create Debug Widget"));

	ActiveWidget_Debug->AddToViewport(0);
	//ActiveWidget_Debug->SetAnchorsInViewport(FAnchors(0.f, 0.f, 1.f, 1.f));
	//ActiveWidget_Debug->SetPositionInViewport(FVector2D::ZeroVector, true);
	//ActiveWidget_Debug->SetAlignmentInViewport(FVector2D::ZeroVector);
}