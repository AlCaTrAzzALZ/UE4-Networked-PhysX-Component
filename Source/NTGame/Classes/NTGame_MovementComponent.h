// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "GameFramework/NavMovementComponent.h"
#include "NTGame_MovementTypes.h"
#include "NTGame_MovementComponent.generated.h"

UCLASS()
class NTGAME_API UNTGame_MovementComponent : public UPawnMovementComponent, public INetworkPredictionInterface
{
	GENERATED_BODY()

	//////////////////////////
	///// Initialization /////
	//////////////////////////
public:
	UNTGame_MovementComponent(const FObjectInitializer& OI);
	
	//////////////////////////////
	///// Ticking & Updating /////
	//////////////////////////////
public:
	UPROPERTY() FPawnMovementPostPhysicsTickFunction PostPhysicsTickFunction;
	virtual void PostPhysicsTickComponent(float DeltaTime, FPawnMovementPostPhysicsTickFunction& ThisTickFunction);
	virtual void RegisterComponentTickFunctions(bool bRegister) override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
		
	static const float MinMoveTickTime;
	static const float MinTimeBetweenTimeStampResets;
	static const float MaxSimulationTimeStep;
	static const ENetworkSmoothingMode SmoothingMode;

	///////////////////////////////////////
	///// Physics Movement Properties /////
	///////////////////////////////////////
public:
	// FVector Velocity; // Already Exists
	FVector Omega; // Angular Velocity
	FVector Accel; // Linear Acceleration
	FVector Alpha; // Angular Acceleration

protected:
	void PerformMovement(const float DeltaTime, const FRepPlayerInput& InInput);
	
	/////////////////
	///// Input /////
	/////////////////
public:
	FORCEINLINE void SetForwardInput(const float Val) { RawControlInput.ForwardAxis = FMath::Clamp(Val, -1.f, 1.f); }
	FORCEINLINE void SetStrafeInput(const float Val) { RawControlInput.StrafeAxis = FMath::Clamp(Val, -1.f, 1.f); }
	FORCEINLINE void SetSteerInput(const float Val) { RawControlInput.SteerAxis = FMath::Clamp(Val, -1.f, 1.f); }
	FORCEINLINE void SetPitchInput(const float Val) { RawControlInput.PitchAxis = FMath::Clamp(Val, -1.f, 1.f); }

	FORCEINLINE FRepPlayerInput GetLastControlInput() const { return LastControlInput; }

protected:
	UPROPERTY(Transient) FRepPlayerInput RawControlInput;
	UPROPERTY(Transient) FRepPlayerInput LastControlInput;

	virtual FRepPlayerInput ComputeAndConsumeInput();

	void CalculateInputAcceleration(const float InDeltaTime, const FRepPlayerInput& InInput);

	///////////////////////////////
	///// Movement Properties /////
	///////////////////////////////
public:
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float ForwardSpeed;
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float StrafeSpeed;
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float SteerSpeed;
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float PitchSpeed;
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	FVector2D PitchLimits;
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	uint8 bEnableHoverSpring;

	UPROPERTY(EditDefaultsOnly, Category = "Hovering")
	float HoverSpring_Tension;
	UPROPERTY(EditDefaultsOnly, Category = "Hovering")
	float HoverSpring_Length;
	UPROPERTY(EditDefaultsOnly, Category = "Hovering")
	float HoverSpring_Damping;

	////////////////////////////////////////
	///// Network Prediction Interface /////
	////////////////////////////////////////
public:
	virtual void SmoothCorrection(const FVector& OldLocation, const FQuat& OldRotation, const FVector& NewLocation, const FQuat& NewRotation) override;
	virtual void SendClientAdjustment() override;
	virtual void ForcePositionUpdate(float DeltaTime) override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual FNetworkPredictionData_Server* GetPredictionData_Server() const override;

	virtual void ResetPredictionData_Client() override;
	virtual void ResetPredictionData_Server() override;

	virtual bool HasPredictionData_Client() const { return ClientPredictionData != nullptr; }
	virtual bool HasPredictionData_Server() const { return ServerPredictionData != nullptr; }

protected:
	FNetworkPredictionData_Client_Physics* GetPredictionData_Client_Physics() const { return static_cast<FNetworkPredictionData_Client_Physics*>(GetPredictionData_Client()); }
	FNetworkPredictionData_Server_Physics* GetPredictionData_Server_Physics() const { return static_cast<FNetworkPredictionData_Server_Physics*>(GetPredictionData_Server()); };

	FNetworkPredictionData_Client_Physics* ClientPredictionData;
	FNetworkPredictionData_Server_Physics* ServerPredictionData;

	///////////////////////
	///// Replication /////
	///////////////////////
public:

protected:
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerMove(const float MoveTimeStamp, const FRepPlayerInput ClientInput, const FRepPawnMoveData& EndMoveData);
	void ServerMove_Implementation(const float MoveTimeStamp, const FRepPlayerInput ClientInput, const FRepPawnMoveData& EndMoveData);	
	bool ServerMove_Validate(const float MoveTimeStamp, const FRepPlayerInput ClientInput, const FRepPawnMoveData& EndMoveData) { return true; }

	UFUNCTION(Client, Unreliable)
	void ClientAckBadMove(const float MoveTimeStamp, const FRepPawnMoveData& ServerEndMoveData);
	void ClientAckBadMove_Implementation(const float MoveTimeStamp, const FRepPawnMoveData& ServerEndMoveData);

	UFUNCTION(Client, Unreliable)
	void ClientAckGoodMove(const float MoveTimestamp);
	void ClientAckGoodMove_Implementation(const float MoveTimestamp);
		
	void ServerMove_PostSim();
	bool ServerCheckClientError(const FSavedPhysicsMovePtr& CurrentlyProcessingClientMove) const;

	bool ClientConditionalReplayBadMoves();
	void ReplayMove(const FSavedPhysicsMovePtr& InMove);

	void ClientPrepareMove_PreSim();
	void ClientPrepareMove_PostSim(const float DeltaTime);	

	/////////////////////
	///// Debugging /////
	/////////////////////
public:
	UPROPERTY(EditDefaultsOnly, Category = "Debug") uint8 bDrawDebug : 1;

protected:
	void Client_DrawMoveBuffer(const float DeltaTime, const FColor& InColour);

	//////////////////////////////////
	///// TimeStamp Verification /////
	//////////////////////////////////
public:

protected:
	bool VerifyClientTimeStamp(const float TimeStamp, FNetworkPredictionData_Server_Physics& ServerData);
	bool IsClientTimeStampValid(const float TimeStamp, FNetworkPredictionData_Server_Physics& ServerData, bool& bTimeStampResetDetected) const;
	void ProcessClientTimeStampForTimeDiscrepancy(float ClientTimeStamp, FNetworkPredictionData_Server_Physics& ServerData);

	void OnClientTimeStampResetDetected();
	void OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float LifeTime, const float CurrentMoveError);
};