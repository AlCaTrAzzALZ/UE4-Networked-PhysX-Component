// Copyright (C) James Baxter 2017. All Rights Reserved.

// This is basically a carbon copy of the engines CharacterMovement timestamp verification, minus support for time dilation.

#include "NTGame.h"
#include "NTGame_MovementTypes.h"
#include "NTGame_MovementComponent.h"
#include "GameFramework/GameNetworkManager.h"

//////////////////////////////////
///// Timestamp Verification /////
//////////////////////////////////

bool UNTGame_MovementComponent::VerifyClientTimeStamp(const float TimeStamp, FNetworkPredictionData_Server_Physics& ServerData)
{
	bool bTimeStampResetDetected = false;
	const bool bIsValid = IsClientTimeStampValid(TimeStamp, ServerData, bTimeStampResetDetected);
	if (bIsValid)
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNTGameMovement, Log, TEXT("TimeStamp reset detected. CurrentTimeStamp: %f, new TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
			OnClientTimeStampResetDetected();
			ServerData.CurrentClientTimeStamp = 0.f;
		}
		else
		{
			UE_LOG(LogNTGameMovement, VeryVerbose, TEXT("TimeStamp %f Accepted! CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
			ProcessClientTimeStampForTimeDiscrepancy(TimeStamp, ServerData);
		}
		return true;
	}
	else
	{
		if (bTimeStampResetDetected)
		{
			UE_LOG(LogNTGameMovement, Log, TEXT("TimeStamp expired. Before TimeStamp Reset. CurrentTimeStamp: %f, TimeStamp: %f"), ServerData.CurrentClientTimeStamp, TimeStamp);
		}
		else
		{
			UE_LOG(LogNTGameMovement, Log, TEXT("TimeStamp expired. %f, CurrentTimeStamp: %f"), TimeStamp, ServerData.CurrentClientTimeStamp);
		}
		return false;
	}
}

bool UNTGame_MovementComponent::IsClientTimeStampValid(const float TimeStamp, FNetworkPredictionData_Server_Physics& ServerData, bool& bTimeStampResetDetected) const
{
	// Very large deltas happen around a timestamp reset
	const float DeltaTimeStamp = (TimeStamp - ServerData.CurrentClientTimeStamp);
	if (FMath::Abs(DeltaTimeStamp) > (UNTGame_MovementComponent::MinTimeBetweenTimeStampResets * 0.5f))
	{
		// Client is resetting timestamp to increase accuracy
		bTimeStampResetDetected = true;
		if (DeltaTimeStamp < 0.f)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if (TimeStamp <= ServerData.CurrentClientTimeStamp)
	{
		return false;
	}

	return true;
}

void UNTGame_MovementComponent::OnClientTimeStampResetDetected()
{}

//////////////////////////////////
///// Discrepancy Resolution /////
//////////////////////////////////

// NB: This only affects the Accelerations that we calculate, not the final velocity (PhysX engine delta is semi-fixed).
void UNTGame_MovementComponent::ProcessClientTimeStampForTimeDiscrepancy(float ClientTimeStamp, FNetworkPredictionData_Server_Physics& ServerData)
{
	// Should only be called on server in networked games
	const AActor* ActorOwner = GetOwner();
	ASSERTV(ActorOwner != nullptr, TEXT("ProcessClientTimeStampForTimeDiscrepancy: Invalid Vehicle Owner"));
	ASSERTV(ActorOwner->Role == ROLE_Authority && GetNetMode() < NM_Client, TEXT("ProcessClientTimeStampForTimeDiscrepancy: Should only be called Server-Side!"));

	// Movement time Discrepancy detection and resolution (potentially caused by client speed hacks, time manipulation)
	// Track client reported time deltas through ServerMove RPC's vs actual Server time, when error accumulates enough
	// trigger prevention measures where client must "pay back" the time difference.
	const bool bServerMoveHasOccurred = ServerData.ServerTimeStampLastServerMove != 0.f;
	AGameNetworkManager* GameNetworkManager = AGameNetworkManager::StaticClass()->GetDefaultObject<AGameNetworkManager>();
	if (GameNetworkManager != nullptr && GameNetworkManager->bMovementTimeDiscrepancyDetection && bServerMoveHasOccurred)
	{
		const float WorldTimeSeconds = GetWorld()->GetTimeSeconds();
		const float ServerDelta = (WorldTimeSeconds - ServerData.ServerTimeStampLastServerMove);
		const float ClientDelta = ClientTimeStamp - ServerData.CurrentClientTimeStamp;
		const float ClientError = ClientDelta - ServerDelta; // Difference between how much time client has ticked since last move vs server

															 // Accumulate raw total discrepancy, unfiltered/unbound (for tracking more long-term trends over the lifetime of the CharacterMovementComponent)
		ServerData.LifetimeRawTimeDiscrepancy += ClientError;

		//
		// 1. Determine total effective discrepancy 
		//
		// NewTimeDiscrepancy is bounded and has a DriftAllowance to limit momentary burst packet loss or 
		// low frame rate from having significant impacts, which could cause needing multiple seconds worth of 
		// slow-down/speed-up even though it wasn't intentional time manipulation
		float NewTimeDiscrepancy = ServerData.TimeDiscrepancy + ClientError;
		{
			// Apply drift allowance - forgiving percent difference per time for error
			const float DriftAllowance = GameNetworkManager->MovementTimeDiscrepancyDriftAllowance;
			if (DriftAllowance > 0.f)
			{
				if (NewTimeDiscrepancy > 0.f)
				{
					NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy - ServerDelta*DriftAllowance, 0.f);
				}
				else
				{
					NewTimeDiscrepancy = FMath::Min(NewTimeDiscrepancy + ServerDelta*DriftAllowance, 0.f);
				}
			}

			// Enforce bounds
			// Never go below MinTimeMargin - ClientError being negative means the client is BEHIND
			// the server (they are going slower).
			NewTimeDiscrepancy = FMath::Max(NewTimeDiscrepancy, GameNetworkManager->MovementTimeDiscrepancyMinTimeMargin);
		}

		// Determine EffectiveClientError, which is error for the currently-being-processed move after 
		// drift allowances/clamping/resolution mode modifications.
		// We need to know how much the current move contributed towards actionable error so that we don't
		// count error that the server never allowed to impact movement to matter
		float EffectiveClientError = ClientError;
		{
			const float NewTimeDiscrepancyRaw = ServerData.TimeDiscrepancy + ClientError;
			if (NewTimeDiscrepancyRaw != 0.f)
			{
				EffectiveClientError = ClientError * (NewTimeDiscrepancy / NewTimeDiscrepancyRaw);
			}
		}

		//
		// 2. If we were in resolution mode, determine if we still need to be
		//
		ServerData.bResolvingTimeDiscrepancy = ServerData.bResolvingTimeDiscrepancy && (ServerData.TimeDiscrepancy > 0.f);

		//
		// 3. Determine if NewTimeDiscrepancy is significant enough to trigger detection, and if so, trigger resolution if enabled
		//
		if (!ServerData.bResolvingTimeDiscrepancy)
		{
			if (NewTimeDiscrepancy > GameNetworkManager->MovementTimeDiscrepancyMaxTimeMargin)
			{
				// Time discrepancy detected - client timestamp ahead of where the server thinks it should be!

				// Trigger logic for resolving time discrepancies
				if (GameNetworkManager->bMovementTimeDiscrepancyResolution)
				{
					// Trigger Resolution
					ServerData.bResolvingTimeDiscrepancy = true;

					// Transfer calculated error to official TimeDiscrepancy value, which is the time that will be resolved down
					// in this and subsequent moves until it reaches 0 (meaning we equalize the error)
					// Don't include contribution to error for this move, since we are now going to be in resolution mode
					// and the expected client error (though it did help trigger resolution) won't be allowed
					// to increase error this frame
					ServerData.TimeDiscrepancy = (NewTimeDiscrepancy - EffectiveClientError);
				}
				else
				{
					// We're detecting discrepancy but not handling resolving that through movement component.
					// Clear time stamp error accumulated that triggered detection so we start fresh (maybe it was triggered
					// during severe hitches/packet loss/other non-goodness)
					ServerData.TimeDiscrepancy = 0.f;
				}

				// Project-specific resolution (reporting/recording/analytics)
				OnTimeDiscrepancyDetected(NewTimeDiscrepancy, ServerData.LifetimeRawTimeDiscrepancy, WorldTimeSeconds - ServerData.WorldCreationTime, ClientError);
			}
			else
			{
				// When not in resolution mode and still within error tolerances, accrue total discrepancy
				ServerData.TimeDiscrepancy = NewTimeDiscrepancy;
			}
		}

		//
		// 4. If we are actively resolving time discrepancy, we do so by altering the DeltaTime for the current ServerMove
		//
		if (ServerData.bResolvingTimeDiscrepancy)
		{
			// Optionally force client corrections during time discrepancy resolution
			// This is useful when default project movement error checking is lenient or ClientAuthorativePosition is enabled
			// to ensure time discrepancy resolution is enforced
			if (GameNetworkManager->bMovementTimeDiscrepancyForceCorrectionsDuringResolution)
			{
				ServerData.bForceClientUpdate = true;
			}

			// Movement time discrepancy resolution
			// When the server has detected a significant time difference between what the client ServerMove RPCs are reporting
			// and the actual time that has passed on the server (pointing to potential speed hacks/time manipulation by client),
			// we enter a resolution mode where the usual "base delta's off of client's reported timestamps" is clamped down
			// to the server delta since last movement update, so that during resolution we're not allowing further advantage.
			// Out of that ServerDelta-based move delta, we also need the client to "pay back" the time stolen from initial 
			// time discrepancy detection (held in TimeDiscrepancy) at a specified rate (AGameNetworkManager::TimeDiscrepancyResolutionRate) 
			// to equalize movement time passed on client and server before we can consider the discrepancy "resolved"
			const float ServerCurrentTimeStamp = WorldTimeSeconds;
			const float ServerDeltaSinceLastMovementUpdate = (ServerCurrentTimeStamp - ServerData.ServerTimeStamp);
			const bool bIsFirstServerMoveThisServerTick = ServerDeltaSinceLastMovementUpdate > 0.f;

			// Restrict ServerMoves to server deltas during time discrepancy resolution 
			// (basing moves off of trusted server time, not client timestamp deltas)
			const float BaseDeltaTime = ServerData.GetBaseServerMoveDeltaTime(ClientTimeStamp);

			if (!bIsFirstServerMoveThisServerTick)
			{
				// Accumulate client deltas for multiple ServerMoves per server tick so that the next server tick
				// can pay back the full amount of that tick and not be bounded by a single small Move delta
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick += BaseDeltaTime;
			}

			float ServerBoundDeltaTime = FMath::Min(BaseDeltaTime + ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick, ServerDeltaSinceLastMovementUpdate);
			ServerBoundDeltaTime = FMath::Max(ServerBoundDeltaTime, 0.f); // No negative deltas allowed

			if (bIsFirstServerMoveThisServerTick)
			{
				// The first ServerMove for a server tick has used the accumulated client delta in the ServerBoundDeltaTime
				// calculation above, clear it out for next frame where we have multiple ServerMoves
				ServerData.TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick = 0.f;
			}

			// Calculate current move DeltaTime and PayBack time based on resolution rate
			const float ResolutionRate = FMath::Clamp(GameNetworkManager->MovementTimeDiscrepancyResolutionRate, 0.f, 1.f);
			float TimeToPayBack = FMath::Min(ServerBoundDeltaTime * ResolutionRate, ServerData.TimeDiscrepancy); // Make sure we only pay back the time we need to
			float DeltaTimeAfterPayback = ServerBoundDeltaTime - TimeToPayBack;

			// Adjust deltas so current move DeltaTime adheres to minimum tick time
			DeltaTimeAfterPayback = FMath::Max(DeltaTimeAfterPayback, UNTGame_MovementComponent::MinMoveTickTime);
			TimeToPayBack = ServerBoundDeltaTime - DeltaTimeAfterPayback;

			// Output of resolution: an overridden delta time that will be picked up for this ServerMove, and removing the time
			// we paid back by overriding the DeltaTime to TimeDiscrepancy (time needing resolved)
			ServerData.TimeDiscrepancyResolutionMoveDeltaOverride = DeltaTimeAfterPayback;
			ServerData.TimeDiscrepancy -= TimeToPayBack;
		}
	}
}

void UNTGame_MovementComponent::OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, const float CurrentMoveError)
{
	// Log Issue
}