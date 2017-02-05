// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "GameFramework/Pawn.h"
#include "NTGame_Pawn.generated.h"

// Declarations
class UNTGame_MovementComponent;

UCLASS()
class NTGAME_API ANTGame_Pawn : public APawn
{
	GENERATED_BODY()
public:
	ANTGame_Pawn(const FObjectInitializer& OI);

	///////////////////////////
	///// APawn Interface /////
	///////////////////////////

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const override;
	virtual void PawnClientRestart() override;

	/////////////////////
	///// Accessors /////
	/////////////////////

	FORCEINLINE USkeletalMeshComponent* GetRootMesh() const { return RootMesh; }
	FORCEINLINE UNTGame_MovementComponent* GetPhysicsMovement() const { return PhysicsMovement; }
	FORCEINLINE USpringArmComponent* GetCameraSpringArm() const { return CameraSpringArm; }
	FORCEINLINE UCameraComponent* GetViewCamera() const { return ViewCamera; }

	///////////////////////
	///// Replication /////
	///////////////////////

	virtual void OnRep_ReplicatedMovement() override;
	virtual void PostNetReceiveLocationAndRotation() override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity);
	virtual void PostNetReceiveAngularVelocity(const FVector& NewAndVelocity);

protected:
	//////////////////////
	///// Components /////
	//////////////////////

	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USkeletalMeshComponent* RootMesh;
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UNTGame_MovementComponent* PhysicsMovement;
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USpringArmComponent* CameraSpringArm;
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UCameraComponent* ViewCamera;

	/////////////////
	///// Input /////
	/////////////////

	virtual void OnMoveForward(const float InVal);
	virtual void OnMoveRight(const float InVal);
	virtual void OnPitch(const float InVal);
	virtual void OnSteer(const float InVal);
};