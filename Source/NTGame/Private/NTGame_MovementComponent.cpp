// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_MovementComponent.h"

#include "GameFramework/GameNetworkManager.h"
#include "NTGame_Pawn.h"

// PhysX
#include "PhysicsPublic.h"
#include "Runtime/Engine/Classes/PhysicsEngine/PhysicsSettings.h"
#include "ThirdParty/PhysX/PhysX_3.4/include/PxScene.h"

///////////////////
///// Statics /////
///////////////////

const float UNTGame_MovementComponent::MinMoveTickTime = 0.0002f;
const float UNTGame_MovementComponent::MinTimeBetweenTimeStampResets = 4.f * 60.f;
const float UNTGame_MovementComponent::MaxSimulationTimeStep = 0.1f;					 // TODO: Use UPhysicsSettings::MaxSimulationTimeStep

////////////////////////
///// Construction /////
////////////////////////

UNTGame_MovementComponent::UNTGame_MovementComponent(const FObjectInitializer& OI)
	: Super(OI)
{
	SetNetAddressable();
	SetIsReplicated(true);

	// Post Physics Tick Function
	PostPhysicsTickFunction.bCanEverTick = true;
	PostPhysicsTickFunction.bStartWithTickEnabled = true;
	PostPhysicsTickFunction.bAllowTickOnDedicatedServer = true;
	PostPhysicsTickFunction.TickGroup = ETickingGroup::TG_PostPhysics;

	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_PrePhysics;

	// Physics properties
	Velocity = FVector::ZeroVector;
	Omega = FVector::ZeroVector;
	Accel = FVector::ZeroVector;
	Alpha = FVector::ZeroVector;

	// Movement Properties
	ForwardSpeed = 200.f;
	StrafeSpeed = 125.f;
	SteerSpeed = 30.f;
	PitchSpeed = 25.f;
	PitchLimits = FVector2D(-35.f, 35.f);

	// Debugging
	bDrawDebug = false;
	bEnableHoverSpring = false;
}

/////////////////////////////
///// Post Physics Tick /////
/////////////////////////////

void UNTGame_MovementComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&PostPhysicsTickFunction))
		{
			PostPhysicsTickFunction.MoveComponent = this;
		}
	}
	else
	{
		if (PostPhysicsTickFunction.IsTickFunctionRegistered())
		{
			PostPhysicsTickFunction.UnRegisterTickFunction();
		}
	}
}

void FPawnMovementPostPhysicsTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(MoveComponent, false, DeltaTime, TickType, [this](float DilatedTime)
	{
		MoveComponent->PostPhysicsTickComponent(DilatedTime, *this);
	});
}

void UNTGame_MovementComponent::PostPhysicsTickComponent(float DeltaTime, FPawnMovementPostPhysicsTickFunction& ThisTickFunction)
{
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	ASSERTV(OwningPawn != nullptr, TEXT("Invalid Owner"));

	Omega = UpdatedPrimitive->GetPhysicsLinearVelocity();
	Velocity = UpdatedPrimitive->GetPhysicsLinearVelocity();

	if (GetOwner()->Role > ROLE_SimulatedProxy)
	{
		const bool bIsClient = (GetOwner()->Role == ROLE_AutonomousProxy && GetNetMode() == NM_Client);
		if (OwningPawn->IsLocallyControlled() && bIsClient)
		{
			// Client sends update to server
			ClientPrepareMove_PostSim(DeltaTime);
		}
		else if (OwningPawn->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() < NM_Client)
		{
			// Server sends updates to remote clients
			ServerMove_PostSim();
		}
	}

	UpdateComponentVelocity();
}

////////////////////////////
///// Pre Physics Tick /////
////////////////////////////

void UNTGame_MovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	const FRepPlayerInput InputData = ComputeAndConsumeInput();
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	ASSERTV(OwningPawn != nullptr, TEXT("Invalid Owner"));

	if (GetOwner()->Role > ROLE_SimulatedProxy)
	{
		const bool bIsClient = (GetOwner()->Role == ROLE_AutonomousProxy && GetNetMode() == NM_Client);
		if (bIsClient)
		{
			Client_DrawMoveBuffer(DeltaTime, FColor::Red);			// Uncorrected Buffer in Red
			ClientConditionalReplayBadMoves();
			Client_DrawMoveBuffer(DeltaTime, FColor::Green);		// Corrected buffer in green
		}

		if (OwningPawn->IsLocallyControlled())
		{
			if (OwningPawn->Role == ROLE_Authority)
			{
				// Local Server Pawn and AI Pawns?
				PerformMovement(DeltaTime, InputData);
			}
			else if (bIsClient)
			{
				// Local Client Updates it's pawn
				ClientPrepareMove_PreSim();
				PerformMovement(DeltaTime, InputData);
			}
		}
		else
		{
			if (GetOwner()->Role == ROLE_Authority)
			{
				// TODO: Servers need to smooth client updates here maybe?

				if (OwningPawn->GetController() == nullptr)
				{
					PerformMovement(DeltaTime, InputData);
				}
			}			
		}
	}
	else if (GetOwner()->Role == ROLE_SimulatedProxy)
	{
		// Any object that isn't local on the client
		// Don't bother using input here
		PerformMovement(DeltaTime, InputData);
	}
}

void UNTGame_MovementComponent::PerformMovement(const float DeltaTime, const FRepPlayerInput& InInput)
{
	CalculateInputAcceleration(DeltaTime, InInput);

	UpdatedPrimitive->SetPhysicsLinearVelocity(Accel * DeltaTime, true);
	UpdatedPrimitive->SetPhysicsAngularVelocity(Alpha * DeltaTime, true);
}

void UNTGame_MovementComponent::CalculateInputAcceleration(const float InDeltaTime, const FRepPlayerInput& InInput)
{
	// Bit gross but ah well!
	const ANTGame_Pawn* OwningNTPawn = Cast<ANTGame_Pawn>(GetOwner());
	ASSERTV(OwningNTPawn != nullptr, TEXT("Invalid Owner Pawn"));

	const FVector Thrust = OwningNTPawn->GetViewCamera()->GetForwardVector() * InInput.ForwardAxis * ForwardSpeed;
	const FVector Strafe = OwningNTPawn->GetViewCamera()->GetRightVector() * InInput.StrafeAxis * StrafeSpeed;
	const FVector SteerAngRot = OwningNTPawn->GetViewCamera()->GetUpVector() * InInput.SteerAxis * SteerSpeed;
	const FVector PitchAngRot = OwningNTPawn->GetViewCamera()->GetRightVector() * InInput.PitchAxis * PitchSpeed;

	// Trace below for hovering
	FHitResult HoverHit = FHitResult();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	FVector HoverAccel = FVector::ZeroVector;

	if (bEnableHoverSpring)
	{
		FCollisionQueryParams Params = FCollisionQueryParams();
		Params.AddIgnoredActor(GetOwner());
		Params.bTraceComplex = true;

		if (GetWorld()->LineTraceSingleByChannel(HoverHit, Location, Location + FVector(0.f, 0.f, -HoverSpring_Length), ECC_Visibility, Params))
		{
			const float CompressionRatio = FMath::GetMappedRangeValueClamped(FVector2D(0.f, HoverSpring_Length), FVector2D(1.f, 0.f), Location.Z - HoverHit.Location.Z);
			const float HoverAccelZ = (CompressionRatio * HoverSpring_Tension) + (-HoverSpring_Damping * Velocity.Z);

			HoverAccel = FVector(0.f, 0.f, HoverAccelZ);
		}
	}

	Accel = Thrust + Strafe + HoverAccel;
	Alpha = SteerAngRot + PitchAngRot;
}

/////////////////////////
///// Replay Buffer /////
/////////////////////////

void UNTGame_MovementComponent::ClientPrepareMove_PreSim()
{
	// Create New Move In Buffer
	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	if (!ClientData) { return; }

	// Update Current Move
	ClientData->CurrentMove = ClientData->CreateSavedMove();
	if (!ClientData->CurrentMove.IsValid())
	{
		return;
	}

	// Save pre-transform and physics state
	ClientData->CurrentMove->PreUpdate(this);
}

void UNTGame_MovementComponent::ClientPrepareMove_PostSim(const float DeltaTime)
{
	// Retrieve Client Data
	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	if (!ClientData) { return; }

	// Update Client Timestamp, with the delta of the PhysX frame
	const float NewSimDelta = ClientData->UpdateTimeStampAndDeltaTime(DeltaTime, UNTGame_MovementComponent::MinTimeBetweenTimeStampResets);
	
	if (!ClientData->CurrentMove.IsValid())
	{
		return;
	}

	// Save Transform & Physics State
	ClientData->CurrentMove->PostUpdate(this);
	ClientData->SavedMoves.Push(ClientData->CurrentMove);
	ClientData->ClientUpdateTime = GetWorld()->GetTimeSeconds();

	ClientData->CurrentMove->MoveTimestamp = ClientData->CurrentTimeStamp;
	ClientData->CurrentMove->MoveInput = LastControlInput;
	ClientData->CurrentMove->MoveDeltaTime = DeltaTime;

	// Send Move To Server
	ServerMove(ClientData->CurrentMove->MoveTimestamp, ClientData->CurrentMove->MoveInput, ClientData->CurrentMove->EndMoveData);

	// Reset Current Move
	ClientData->CurrentMove = NULL;
}

void UNTGame_MovementComponent::ServerMove_Implementation(const float MoveTimeStamp, const FRepPlayerInput ClientInput, const FRepPawnMoveData& EndMoveData)
{
	// This runs on the Server
	FNetworkPredictionData_Server_Physics* ServerData = GetPredictionData_Server_Physics();
	ASSERTV(ServerData != nullptr, TEXT("Invalid Server Data"));

	// Nullify client move if we receive a new move before the existing move has been processed.
	// This probably isn't possibly anyway.
	ServerData->CurrentlyProcessingClientMove = NULL;

	// If this move is super old, we don't bother re simulating
	// ForceUpdatePosition will be called at this point from PlayerController, and the server will hard-set the client position.
	if (!VerifyClientTimeStamp(MoveTimeStamp, *ServerData))
	{
		return;
	}

	FRepPlayerInput ClientInputCopy = ClientInput;
	bool bServerReadyForClient = true;
	
	APawn* MyPawn = Cast<APawn>(GetOwner());
	ASSERTV(MyPawn != nullptr, TEXT("Invalid Owner for Movement Component"));

	APlayerController* MyPC = Cast<APlayerController>(MyPawn->GetController());
	if (MyPC)
	{
		bServerReadyForClient = MyPC->NotifyServerReceivedClientData(MyPawn, MoveTimeStamp);
		if (!bServerReadyForClient)
		{
			ClientInputCopy = FRepPlayerInput();
		}
	}

	// Update Delta Time, given the last received client timestamp
	const float AccelDelta = ServerData->GetServerMoveDeltaTime(MoveTimeStamp);

	// Now update data
	ServerData->CurrentClientTimeStamp = MoveTimeStamp;
	ServerData->ServerTimeStamp = GetWorld()->GetTimeSeconds();
	ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;

	// Skip if we have invalid data
	if (!bServerReadyForClient) { return; }
	if (AccelDelta <= 0.f) { return; }

	ServerData->CreateProcessingMove(MoveTimeStamp, AccelDelta, ClientInputCopy, EndMoveData);

	// Run pre-sim movement code
	PerformMovement(AccelDelta, ClientInputCopy);
}

void UNTGame_MovementComponent::ServerMove_PostSim()
{
	// Check For Differences
	FNetworkPredictionData_Server_Physics* ServerData = GetPredictionData_Server_Physics();
	ASSERTV(ServerData != nullptr, TEXT("Invalid Server Data"));

	if (!ServerData->CurrentlyProcessingClientMove.IsValid()) { return; }

	// Update Post Move Data
	ServerData->CurrentlyProcessingClientMove->PostUpdate(this);

	// Now check if the client simulated incorrectly (client data is stored as 'Start Move Data')
	const bool bBadClientSim = ServerCheckClientError(ServerData->CurrentlyProcessingClientMove);
	if (ServerData->bForceClientUpdate || bBadClientSim)
	{
		// Move wasn't okay, send correct result of this move.
		// Client will have to re simulate moves created after this one (without re-sending them?)
		ClientAckBadMove(ServerData->CurrentlyProcessingClientMove->MoveTimestamp, ServerData->CurrentlyProcessingClientMove->EndMoveData);
	}
	else
	{
		// Move was okay, acknowledge it		
		ClientAckGoodMove(ServerData->CurrentlyProcessingClientMove->MoveTimestamp);
	}

	ServerData->CurrentlyProcessingClientMove = NULL;
	ServerData->bForceClientUpdate = false;
}

bool UNTGame_MovementComponent::ServerCheckClientError(const FSavedPhysicsMovePtr& CurrentlyProcessingClientMove) const
{
	const FVector LocDiff = CurrentlyProcessingClientMove->EndMoveData.Location - CurrentlyProcessingClientMove->StartMoveData.Location;
//	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::SanitizeFloat(FVector::DotProduct(LocDiff, LocDiff)));

	if (GetDefault<AGameNetworkManager>()->ExceedsAllowablePositionError(LocDiff))
	{
		return true;
	}

	return false;
}

void UNTGame_MovementComponent::ClientAckGoodMove_Implementation(const float MoveTimestamp)
{
	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	ASSERTV(ClientData != nullptr, TEXT("Invalid Client Data"));

	// Ack Move if it hasn't expired
	const int32 AckedMoveIndex = ClientData->GetMoveIndexFromTimeStamp(MoveTimestamp);
	if (AckedMoveIndex == INDEX_NONE)
	{
		if (ClientData->LastAckedMove.IsValid())
		{
			// Couldn't Ack Move
		}
		GEngine->AddOnScreenDebugMessage(-1, 0.25f, FColor::Blue, TEXT("Ack Good Move - NOTFOUND"));
		return;
	}

	ClientData->AcknowledgeMove(AckedMoveIndex);
	GEngine->AddOnScreenDebugMessage(-1, 0.03f, FColor::Green, TEXT("Ack Good Move"));
	// Tell Debug HUD
}

void UNTGame_MovementComponent::ClientAckBadMove_Implementation(const float MoveTimeStamp, const FRepPawnMoveData& ServerEndMoveData)
{
	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	ASSERTV(ClientData != nullptr, TEXT("Invalid Client Data"));

	// Ack Move if it hasn't expired
	const int32 AckedMoveIndex = ClientData->GetMoveIndexFromTimeStamp(MoveTimeStamp);
	if (AckedMoveIndex == INDEX_NONE)
	{
		if (ClientData->LastAckedMove.IsValid())
		{
			// Couldn't Ack Move
		}
		return;
	}
	
	ClientData->AcknowledgeMove(AckedMoveIndex);
		
	// We want to replay all moves from the acknowledged move onwards, before running physics next tick.
	// Set client to servers' physics state, ready for replay
	ClientData->bNeedsReplay = true;
	Velocity = ServerEndMoveData.LinearVelocity;
	Omega = ServerEndMoveData.AngularVelocity;

	UpdatedPrimitive->SetWorldLocationAndRotation(ServerEndMoveData.Location, ServerEndMoveData.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	UpdatedPrimitive->SetAllPhysicsLinearVelocity(Velocity);
	UpdatedPrimitive->SetAllPhysicsAngularVelocity(Omega);

	GEngine->AddOnScreenDebugMessage(-1, 0.03f, FColor::Red, TEXT("Ack Bad Move"));
}

//////////////////////////
///// Move Replaying /////
//////////////////////////

bool UNTGame_MovementComponent::ClientConditionalReplayBadMoves()
{
	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	ASSERTV_WR(ClientData != nullptr, false, TEXT("Invalid Client Data"));

	if (!ClientData->bNeedsReplay) { return false; }
	ClientData->bNeedsReplay = false;

	if (ClientData->SavedMoves.Num() == 0)
	{
		// No saved moves to replay
		return false;
	}

	// Replay non-acknowledged moves
	// The Server's 'End Move State' has already been set in 'ClientAckBadMove'

	for (int32 Idx = 0; Idx < ClientData->SavedMoves.Num(); Idx++)
	{
		const FSavedPhysicsMovePtr& CurrentMove = ClientData->SavedMoves[Idx];

		// Update this move, so it has our current physics state (either from the Server, or after we recalc a move)
		CurrentMove->PreUpdate(this);
		ReplayMove(CurrentMove);
		CurrentMove->PostUpdate(this);
	}

	// Now set physics state based on our latest move
	return ClientData->SavedMoves.Num() > 0;
}

void UNTGame_MovementComponent::ReplayMove(const FSavedPhysicsMovePtr& InMove)
{
	// Perform Movement First
	PerformMovement(InMove->MoveDeltaTime, InMove->MoveInput);

	// Now Simulate the PhysX Scene
	// This is kind of shit, because it simulates the entire scene at this rate and we haven't reset any other objects.
	// It also means that ALL other objects on the client will end up in the wrong position, until we get an update for them too.
	// In future, this re-simulation needs to be done in a different scene, possibly with duplicated static bodies to reduce desyncs.

	FPhysScene* Scene = GetWorld()->GetPhysicsScene();
	if (Scene)
	{
		PxScene* WorldPxScene = Scene->GetPhysXScene(PST_Sync);	// TODO, replace with consistent 'Replay Scene'

		// Scratch Buffers
		UPhysicsSettings* Settings = UPhysicsSettings::Get();
		ASSERTV(Settings != nullptr, TEXT("Invalid Physics Settings"));

		int32 SceneScratchBufferSize = Settings->SimulateScratchMemorySize;
		if (SceneScratchBufferSize > 0)
		{
			// Temporary Scratch Buffer
			// TODO: If it's safe to use this buffer over multiple frames, that would save a lot of memory allocations when replaying moves.. Need to test.
			// Could also create a permanent buffer with a custom scene
			uint8* Buffer = (uint8*)FMemory::Malloc(SceneScratchBufferSize, 16);

			WorldPxScene->simulate(InMove->MoveDeltaTime, nullptr, Buffer, SceneScratchBufferSize, true);
			WorldPxScene->fetchResults(true);

			// Free the Scratch Buffer
			FMemory::Free(Buffer);
		}
	}
	

	// TODO: Simulate in a separate scene to avoid moving non-local actors, causing jitter

// 		// Get the body from the PhysX scene
// 		const PxRigidActor* SyncPxActor = UpdatedPrimitive->GetBodyInstance()->GetPxRigidActor_AssumesLocked(PST_Sync);
// 		
// 		// Create a new Physics Scene
// 		FPhysScene* ReplayScene = new FPhysScene();
// 		ASSERTV(ReplayScene != nullptr, TEXT("Invalid Replay Scene"));
// 
// 		ReplayScene->SetOwningWorld(GetWorld());
// 		PxScene* PhysXScn = ReplayScene->GetPhysXScene(PST_Sync);
// 		if (PhysXScn)
// 		{
// 			const bool bScene;
// 		}
}

/////////////////
///// Input /////
/////////////////

FRepPlayerInput UNTGame_MovementComponent::ComputeAndConsumeInput()
{
	LastControlInput = RawControlInput;
	RawControlInput = FRepPlayerInput();

	return LastControlInput;
}

///////////////////////////////////
///// Network Prediction Data /////
///////////////////////////////////

FNetworkPredictionData_Client* UNTGame_MovementComponent::GetPredictionData_Client() const
{
	ASSERTV_WR(GetOwner() != nullptr, nullptr, TEXT("GetPredictionData_Client: Vehicle Owner Invalid"));
	ASSERTV_WR(GetOwner()->Role < ROLE_Authority || (GetOwner()->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() == NM_ListenServer), nullptr, TEXT("GetPredictionData_Client: Authority Check Failed"));
	ASSERTV_WR(GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer, nullptr, TEXT("GetPredictionData_Client: Client/Server Checks Failed"));

	if (!ClientPredictionData)
	{
		UNTGame_MovementComponent* MutableThis = const_cast<UNTGame_MovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Physics();
	}

	return ClientPredictionData;
}

FNetworkPredictionData_Server* UNTGame_MovementComponent::GetPredictionData_Server() const
{
	// Should only be called on server in network games
	ASSERTV_WR(GetOwner() != nullptr, nullptr, TEXT("GetPredictionData_Server: Vehicle Owner Invalid"));
	ASSERTV_WR(GetOwner()->Role == ROLE_Authority, nullptr, TEXT("GetPredictionData_Server: Authority Check Failed"));
	ASSERTV_WR(GetNetMode() < NM_Client, nullptr, TEXT("GetPredictionData_Server: Client Check Failed"));

	if (!ServerPredictionData)
	{
		UNTGame_MovementComponent* MutableThis = const_cast<UNTGame_MovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_Physics(GetWorld());
	}

	return ServerPredictionData;
}

void UNTGame_MovementComponent::ResetPredictionData_Client()
{
	if (ClientPredictionData)
	{
		delete ClientPredictionData;
		ClientPredictionData = nullptr;
	}
}

void UNTGame_MovementComponent::ResetPredictionData_Server()
{
	if (ServerPredictionData)
	{
		delete ServerPredictionData;
		ServerPredictionData = nullptr;
	}
}

////////////////////////////////////////
///// Network Prediction Interface /////
////////////////////////////////////////

void UNTGame_MovementComponent::SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation)
{
	// TODO
}

void UNTGame_MovementComponent::SendClientAdjustment()
{
	// Do Nothing
	// Traditionally this would be used to send updates to a client from the Server, but we do that on our own already.
	// Called from PlayerController, which ticks pre-physics, so we don't wanna do nuffin here.
}

void UNTGame_MovementComponent::ForcePositionUpdate(float DeltaTime)
{
	// This get's called on the Server if we haven't had an update from the client in a long time, to force movement
	ASSERTV(GetOwner()->Role == ROLE_Authority, TEXT("No Authority to Force Position Update"));
	ASSERTV(GetOwner()->GetRemoteRole() == ROLE_AutonomousProxy, TEXT("Invalid Remote Role"));

	UE_LOG(LogNTGameMovement, Warning, TEXT("Server called ForcePositionUpdate with %f DeltaTime. Max = %f, delta may be clamped."), DeltaTime, MaxSimulationTimeStep);

	// Clamp the maximum time step so we don't get huge deltas if we haven't had an update for a long time.
	// This can cause forces to go crazy otherwise
	const float ForcedSimAccelDelta = FMath::Min(DeltaTime, MaxSimulationTimeStep);
	PerformMovement(ForcedSimAccelDelta, FRepPlayerInput());
}

/////////////////////
///// Debugging /////
/////////////////////

void UNTGame_MovementComponent::Client_DrawMoveBuffer(const float DeltaTime, const FColor& InColour)
{
	if (!bDrawDebug) { return; }

	FNetworkPredictionData_Client_Physics* ClientData = GetPredictionData_Client_Physics();
	ASSERTV(ClientData != nullptr, TEXT("Invalid Client Data"));

	if (ClientData->SavedMoves.Num() == 0)
	{
		// No saved moves to draw
		return;
	}

	for (int32 Idx = 0; Idx < ClientData->SavedMoves.Num(); Idx++)
	{
		const FSavedPhysicsMovePtr& ThisMove = ClientData->SavedMoves[Idx];

		const FVector DrawOrigin = ThisMove->EndMoveData.Location;
		const FQuat DrawQuat = ThisMove->EndMoveData.Rotation;
		//const FVector VelocEnd = DrawOrigin + ThisMove->EndMoveData.LinearVelocity;

		DrawDebugBox(GetWorld(), DrawOrigin, FVector(64.f), DrawQuat, InColour, false, -1.f, 0, 4.f);
	}
}