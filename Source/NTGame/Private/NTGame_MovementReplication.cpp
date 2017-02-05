// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_MovementTypes.h"
#include "GameFramework/GameNetworkManager.h"

#include "NTGame_Pawn.h"
#include "NTGame_MovementComponent.h"

///////////////////
///// Statics /////
///////////////////

const int32 FNetworkPredictionData_Client_Physics::MaxSavedMoves = 96;
const int32 FNetworkPredictionData_Client_Physics::MaxFreeMoves = 32;
const float FNetworkPredictionData_Client_Physics::MaxMoveDeltaTime = 0.125f;	// AGameNetworkManager::MaxMoveDeltaTime

///////////////////////////////////
///// Simplified Network Data /////
///////////////////////////////////

void FSavedPhysicsMove::Clear()
{
	StartMoveData = FRepPawnMoveData();
	EndMoveData = FRepPawnMoveData();
	MoveInput = FRepPlayerInput();
	MoveTimestamp = 0.f;
	MoveDeltaTime = 0.f;
	bForceNoCombine = false;
	bHasInvalidTimeStampWhenStampsReset = false;
}

void FSavedPhysicsMove::PreUpdate(const UNTGame_MovementComponent* InComponent)
{
	StartMoveData.Location = InComponent->UpdatedPrimitive->GetComponentLocation();
	StartMoveData.Rotation = InComponent->UpdatedPrimitive->GetComponentQuat();
	StartMoveData.LinearVelocity = InComponent->UpdatedPrimitive->GetPhysicsLinearVelocity();
	StartMoveData.AngularVelocity = InComponent->UpdatedPrimitive->GetPhysicsAngularVelocity();
}

void FSavedPhysicsMove::PostUpdate(const UNTGame_MovementComponent* InComponent)
{
	EndMoveData.Location = InComponent->UpdatedPrimitive->GetComponentLocation();
	EndMoveData.Rotation = InComponent->UpdatedPrimitive->GetComponentQuat();
	EndMoveData.LinearVelocity = InComponent->UpdatedPrimitive->GetPhysicsLinearVelocity();
	EndMoveData.AngularVelocity = InComponent->UpdatedPrimitive->GetPhysicsAngularVelocity();
}

bool FSavedPhysicsMove::IsImportantMove(const FSavedPhysicsMovePtr& LastAckedMove) const
{
	return MoveInput != LastAckedMove->MoveInput;
}

bool FSavedPhysicsMove::CanCombineWith(const FSavedPhysicsMovePtr& OtherPendingMove) const
{
	if (bForceNoCombine || OtherPendingMove->bForceNoCombine) { return false; }

	return MoveInput == OtherPendingMove->MoveInput;
}

//////////////////////////////////
///// Simplified Client Data /////
//////////////////////////////////

FNetworkPredictionData_Client_Physics::FNetworkPredictionData_Client_Physics()
	: ClientUpdateTime(0.f)
	, CurrentTimeStamp(0.f)
	, PendingMove(NULL)
	, LastAckedMove(NULL)
{}

int32 FNetworkPredictionData_Client_Physics::GetMoveIndexFromTimeStamp(const float TimeStamp) const
{
	if (SavedMoves.Num() > 0)
	{
		if (LastAckedMove.IsValid() && !LastAckedMove->bHasInvalidTimeStampWhenStampsReset && (TimeStamp <= LastAckedMove->MoveTimestamp))
		{
			return INDEX_NONE;
		}

		// Otherwise try to find the move
		for (int32 Idx = 0; Idx < SavedMoves.Num(); Idx++)
		{
			const FSavedPhysicsMovePtr& MoveItr = SavedMoves[Idx];
			if (MoveItr->MoveTimestamp == TimeStamp)
			{
				return Idx;
			}
		}
	}

	return INDEX_NONE;
}

void FNetworkPredictionData_Client_Physics::AcknowledgeMove(const int32 AckMoveIndex)
{
	if (AckMoveIndex != INDEX_NONE)
	{
		const FSavedPhysicsMovePtr& AckedMove = SavedMoves[AckMoveIndex];
		if (LastAckedMove.IsValid())
		{
			FreeMove(LastAckedMove);
		}
		LastAckedMove = AckedMove;

		// Free Expired Moves
		for (int32 Idx = 0; Idx < AckMoveIndex; Idx++)
		{
			const FSavedPhysicsMovePtr& MoveItr = SavedMoves[Idx];
			FreeMove(MoveItr);
		}

		SavedMoves.RemoveAt(0, AckMoveIndex + 1);
	}
}

void FNetworkPredictionData_Client_Physics::FreeMove(const FSavedPhysicsMovePtr& FreedMove)
{
	if (FreedMove.IsValid())
	{
		// Only keep a pool of a limited number of moves
		if (FreeMoves.Num() < MaxFreeMoves)
		{
			FreeMoves.Push(FreedMove);
		}

		// Shouldn't keep a reference to the move on the free list
		if (PendingMove == FreedMove)
		{
			PendingMove = NULL;
		}

		if (LastAckedMove == FreedMove)
		{
			LastAckedMove = NULL;
		}
	}
}

FSavedPhysicsMovePtr FNetworkPredictionData_Client_Physics::CreateSavedMove()
{
	if (SavedMoves.Num() >= MaxSavedMoves)
	{
		UE_LOG(LogNTGameMovement, Warning, TEXT("CreateSavedMove: Hit limit of %d saved moves (timing out or very bad ping?)"), SavedMoves.Num());
		// Free all saved moves
		for (int32 Idx = 0; Idx < SavedMoves.Num(); Idx++)
		{
			FreeMove(SavedMoves[Idx]);
		}

		SavedMoves.Reset();
	}

	if (FreeMoves.Num() == 0)
	{
		// No free moves, allocate a new one
		FSavedPhysicsMovePtr NewMove = AllocateNewMove();
		ASSERTV_WR(NewMove.IsValid(), NULL, TEXT("CreateSavedMove: AllocateNewMove unable to create a new move!"));
		NewMove->Clear();
		return NewMove;
	}
	else
	{
		// Pull from first free pool
		FSavedPhysicsMovePtr FirstFree = FreeMoves.Pop();
		FirstFree->Clear();
		return FirstFree;
	}
}

float FNetworkPredictionData_Client_Physics::UpdateTimeStampAndDeltaTime(const float InDeltaTime, const float MinTimeBetweenResets)
{
	if (CurrentTimeStamp > MinTimeBetweenResets)
	{
		UE_LOG(LogNTGameMovement, Log, TEXT("Resetting Client's TimeStamp %f"), CurrentTimeStamp);
		CurrentTimeStamp = 0.f;

		// Mark all buffered moves as having old time stamps, so we make sure not to resend them.
		// That would FUCK UP DA SERVER YO
		for (int32 Idx = 0; Idx < SavedMoves.Num(); Idx++)
		{
			const FSavedPhysicsMovePtr& MoveItr = SavedMoves[Idx];
			MoveItr->bHasInvalidTimeStampWhenStampsReset = true;
		}

		// And last acked move
		if (LastAckedMove.IsValid())
		{
			LastAckedMove->bHasInvalidTimeStampWhenStampsReset = true;
		}
	}

	// Update time stamp
	CurrentTimeStamp += InDeltaTime;
	float ClientDeltaTime = InDeltaTime;

	// Server uses timestamps to derive delta time, which introduces some rounding errors.
	// Make sure we do the same, so that MoveAutonomous uses the same input and is deterministic!
	if (SavedMoves.Num() > 0)
	{
		const FSavedPhysicsMovePtr& PreviousMove = SavedMoves.Last();
		if (!PreviousMove->bHasInvalidTimeStampWhenStampsReset)
		{
			// Clients use Servers' calculation / approximation of delta time
			const float ServerDeltaTime = CurrentTimeStamp - PreviousMove->MoveTimestamp;
			ClientDeltaTime = ServerDeltaTime;
		}
	}

	return FMath::Min(ClientDeltaTime, MaxMoveDeltaTime);
}

//////////////////////////////////
///// Simplified Server Data /////
//////////////////////////////////

FNetworkPredictionData_Server_Physics::FNetworkPredictionData_Server_Physics(const UWorld* InWorld)
	: CurrentClientTimeStamp(0.f)
	, LastUpdateTime(0.f)
	, ServerTimeStampLastServerMove(0.f)
	, bForceClientUpdate(false)
	, LifetimeRawTimeDiscrepancy(0.f)
	, TimeDiscrepancy(0.f)
	, bResolvingTimeDiscrepancy(false)
	, TimeDiscrepancyResolutionMoveDeltaOverride(0.f)
	, TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick(0.f)
	, WorldCreationTime(0.f)
	, CurrentlyProcessingClientMove(NULL)
{
	ASSERTV(InWorld != nullptr, TEXT("Invalid World For Server Data"))
	WorldCreationTime = InWorld->GetTimeSeconds();
	ServerTimeStamp = InWorld->GetTimeSeconds();		// Prevents 'Force Update' being called when initially respawned
}

float FNetworkPredictionData_Server_Physics::GetServerMoveDeltaTime(const float ClientTimeStamp) const
{
	if (bResolvingTimeDiscrepancy)
	{
		return TimeDiscrepancyResolutionMoveDeltaOverride;
	}
	else
	{
		return GetBaseServerMoveDeltaTime(ClientTimeStamp);
	}
}

float FNetworkPredictionData_Server_Physics::GetBaseServerMoveDeltaTime(const float ClientTimeStamp) const
{
	const float DeltaTime = FMath::Min(FNetworkPredictionData_Client_Physics::MaxMoveDeltaTime, ClientTimeStamp - CurrentClientTimeStamp);
	return DeltaTime;
}

FSavedPhysicsMovePtr FNetworkPredictionData_Server_Physics::CreateProcessingMove(const float MoveTimeStamp, const float AccelDelta, const FRepPlayerInput& ClientInput, const FRepPawnMoveData& ClientEndData)
{
	CurrentlyProcessingClientMove = FSavedPhysicsMovePtr(new FSavedPhysicsMove());
	ASSERTV_WR(CurrentlyProcessingClientMove.IsValid(), NULL, TEXT("Can't Create Processing Move"));
	
	CurrentlyProcessingClientMove->StartMoveData = ClientEndData;
	CurrentlyProcessingClientMove->MoveDeltaTime = AccelDelta;
	CurrentlyProcessingClientMove->MoveTimestamp = MoveTimeStamp;
	CurrentlyProcessingClientMove->MoveInput = ClientInput;

	return CurrentlyProcessingClientMove;
}