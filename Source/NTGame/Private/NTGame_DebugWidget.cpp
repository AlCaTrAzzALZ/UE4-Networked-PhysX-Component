// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_DebugWidget.h"

#include "NTGame_Pawn.h"
#include "NTGame_MovementComponent.h"

// Components
#include "TextBlock.h"

#define LOCTEXT_NAMESPACE "DebugWidgetNS"

UNTGame_DebugWidget::UNTGame_DebugWidget(const FObjectInitializer& OI)
	: Super(OI)
{}

void UNTGame_DebugWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateDynamicData();
}

void UNTGame_DebugWidget::UpdateDynamicData()
{
	const bool bIsServer = GetOwningPlayer()->GetNetMode() < NM_Client ? true : false;
	TitleText->SetText(bIsServer ? FText::FromString(TEXT("Server")) : FText::FromString(TEXT("Client")));

	const ANTGame_Pawn* OwningNTPawn = Cast<ANTGame_Pawn>(GetOwningPlayerPawn());
	if (OwningNTPawn)
	{
		const UNTGame_MovementComponent* OwningNTMovement = OwningNTPawn->GetPhysicsMovement();
		ASSERTV(OwningNTMovement != nullptr, TEXT("Invalid Pawn Movement"))

		InputText->SetText(FText::FromString(OwningNTMovement->GetLastControlInput().ToString()));

		VelocText->SetText(FText::FromString(TEXT("Veloc: ") + OwningNTMovement->Velocity.ToString()));
		OmegaText->SetText(FText::FromString(TEXT("Omega: ") + OwningNTMovement->Omega.ToString()));
		AccelText->SetText(FText::FromString(TEXT("Accel: ") + OwningNTMovement->Accel.ToString()));
		AlphaText->SetText(FText::FromString(TEXT("Alpha: ") + OwningNTMovement->Alpha.ToString()));
	}

	// If client, set lag text
	if (!bIsServer)
	{
		const UNetDriver* NetDriver = GetWorld()->GetNetDriver();
		ASSERTV(NetDriver != nullptr, TEXT("Invalid Net Driver"));
		
		const int32 BaseLag = NetDriver->PacketSimulationSettings.PktLag;
		const int32 VariLag = NetDriver->PacketSimulationSettings.PktLagVariance;
		const int32 PakLoss = NetDriver->PacketSimulationSettings.PktLoss;
		const int32 PakDupe = NetDriver->PacketSimulationSettings.PktDup;
		const int32 PakOrdr = NetDriver->PacketSimulationSettings.PktOrder;

		// Build Strings
		const FText LatencyVar = FText::Format(LOCTEXT("LatencyVar", "~+ {0}ms"), FText::AsNumber(VariLag));
		const FText LatencyText = FText::Format(LOCTEXT("Latency", "Latency: {0}ms {1}"), FText::AsNumber(BaseLag), VariLag > 0 ? LatencyVar : FText());

		const FText LossText = FText::Format(LOCTEXT("Loss", "Packet Loss: {0}"), FText::AsPercent((float)PakLoss / 100.f));
		const FText DupeText = FText::Format(LOCTEXT("Dupe", "Packet Dupe: {0}"), FText::AsPercent((float)PakDupe / 100.f));
		const FText OrdrText = FText::Format(LOCTEXT("Order", "Packet Order: {0}"), PakOrdr > 0 ? FText::FromString(TEXT("ON")) : FText::FromString(TEXT("OFF")));

		const FLinearColor LagColour = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, FMath::GetMappedRangeValueClamped(FVector2D(0.f, 150.f), FVector2D(0.f, 1.f), (float)BaseLag));
		const FLinearColor LossColour = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, FMath::GetMappedRangeValueClamped(FVector2D(0.f, 100.f), FVector2D(0.f, 1.f), (float)PakLoss));
		const FLinearColor DupeColour = FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, FMath::GetMappedRangeValueClamped(FVector2D(0.f, 100.f), FVector2D(0.f, 1.f), (float)PakDupe));

		PacketLag->SetText(LatencyText);
		PacketLoss->SetText(LossText);
		PacketDupe->SetText(DupeText);
		PacketOrder->SetText(OrdrText);

		PacketLag->SetColorAndOpacity(FSlateColor(LagColour));
		PacketLoss->SetColorAndOpacity(FSlateColor(LossColour));
		PacketDupe->SetColorAndOpacity(FSlateColor(DupeColour));
		PacketOrder->SetColorAndOpacity(PakOrdr > 0 ? FLinearColor::Red : FLinearColor::Gray);
	}
}

#undef LOCTEXT_NAMESPACE