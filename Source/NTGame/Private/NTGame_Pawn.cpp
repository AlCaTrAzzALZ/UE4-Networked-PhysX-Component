// Copyright (C) James Baxter 2017. All Rights Reserved.

#include "NTGame.h"
#include "NTGame_Pawn.h"

// Movement
#include "NTGame_MovementComponent.h"

ANTGame_Pawn::ANTGame_Pawn(const FObjectInitializer& OI) : Super(OI)
{
	RootMesh = OI.CreateDefaultSubobject<USkeletalMeshComponent>(this, TEXT("RootMesh"));
	RootMesh->SetCollisionObjectType(ECollisionChannel::ECC_Vehicle);
	RootMesh->SetCollisionProfileName(TEXT("Vehicle"));
	RootMesh->MeshComponentUpdateFlag = EMeshComponentUpdateFlag::OnlyTickPoseWhenRendered;
	RootMesh->SetSimulatePhysics(true);
	RootMesh->SetLinearDamping(0.f);
	RootMesh->SetAngularDamping(0.f);
	RootComponent = RootMesh;

	PhysicsMovement = OI.CreateDefaultSubobject<UNTGame_MovementComponent>(this, TEXT("PhysicsMovement"));
	PhysicsMovement->SetUpdatedComponent(RootMesh);

	CameraSpringArm = OI.CreateDefaultSubobject<USpringArmComponent>(this, TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootMesh, NAME_None);
	CameraSpringArm->TargetArmLength = 1000.f;
	CameraSpringArm->TargetOffset = FVector(0.f, 0.f, 192.f);
	CameraSpringArm->SetAbsolute(false, true, true);

	ViewCamera = OI.CreateDefaultSubobject<UCameraComponent>(this, TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(CameraSpringArm, USpringArmComponent::SocketName);
	ViewCamera->SetFieldOfView(80.f);

	// Replication Flags
	bAlwaysRelevant = true;
	bReplicateMovement = true;

	// Movement Replication

}

void ANTGame_Pawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FString RoleString = Role == ROLE_Authority ? TEXT("AUTHORITY") : Role == ROLE_SimulatedProxy ? TEXT("SIMULATED") : Role == ROLE_AutonomousProxy ? TEXT("AUTONOMOUS") : TEXT("NONE");
	const FString RemoteRoleString = GetRemoteRole() == ROLE_Authority ? TEXT("AUTHORITY") : GetRemoteRole() == ROLE_SimulatedProxy ? TEXT("SIMULATED") : GetRemoteRole() == ROLE_AutonomousProxy ? TEXT("AUTONOMOUS") : TEXT("NONE");
	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 128.f), TEXT("Role:") + RoleString, nullptr, FColor::Red, DeltaSeconds + 0.005f, true);
	DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 100.f), TEXT("Remote:") + RemoteRoleString, nullptr, FColor::Red, DeltaSeconds + 0.005f, true);
}

/////////////////
///// Input /////
/////////////////

void ANTGame_Pawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	ASSERTV(PlayerInputComponent != nullptr, TEXT("Invalid Input Component"));

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ANTGame_Pawn::OnMoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ANTGame_Pawn::OnMoveRight);
	PlayerInputComponent->BindAxis(TEXT("Pitch"), this, &ANTGame_Pawn::OnPitch);
	PlayerInputComponent->BindAxis(TEXT("Steer"), this, &ANTGame_Pawn::OnSteer);
}

void ANTGame_Pawn::OnMoveForward(const float InVal)
{
	PhysicsMovement->SetForwardInput(InVal);
}

void ANTGame_Pawn::OnMoveRight(const float InVal)
{
	PhysicsMovement->SetStrafeInput(InVal);
}

void ANTGame_Pawn::OnPitch(const float InVal)
{
	PhysicsMovement->SetPitchInput(InVal);
}

void ANTGame_Pawn::OnSteer(const float InVal)
{
	PhysicsMovement->SetSteerInput(InVal);
}

///////////////////////
///// Replication /////
///////////////////////

void ANTGame_Pawn::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty> & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CHANGE_CONDITION(ANTGame_Pawn, ReplicatedMovement, COND_SimulatedOrPhysicsNoReplay);
}

void ANTGame_Pawn::PawnClientRestart()
{
	GetPhysicsMovement()->StopMovementImmediately();
	GetPhysicsMovement()->ResetPredictionData_Client();

	Super::PawnClientRestart();
}

void ANTGame_Pawn::OnRep_ReplicatedMovement()
{
	// Note, we do nothing if we're the autonomous proxy (aka this local client)
	if (!RootComponent->GetAttachParent() && Role == ROLE_SimulatedProxy)
	{
		PostNetReceiveVelocity(ReplicatedMovement.LinearVelocity);
		PostNetReceiveAngularVelocity(ReplicatedMovement.AngularVelocity);
		PostNetReceiveLocationAndRotation();
	}
}

void ANTGame_Pawn::PostNetReceiveLocationAndRotation()
{
	if (Role == ROLE_SimulatedProxy)
	{
		// Don't change transform if using relative position
		/*if (true)*/
		{
			const FVector OldLocation = GetActorLocation();
			const FQuat OldRotation = GetActorQuat();

			GetPhysicsMovement()->UpdatedComponent->SetWorldLocationAndRotation(ReplicatedMovement.Location, ReplicatedMovement.Rotation.Quaternion(), false, nullptr, ETeleportType::TeleportPhysics);

			//GetVehicleMovement()->bNetworkSmoothingComplete = false;
			//GetVehicleMovement()->SmoothCorrection(OldLocation, OldRotation, ReplicatedMovement.Location, ReplicatedMovement.Rotation.Quaternion());
			//OnUpdateSimulatedPosition(OldLocation, OldRotation);
		}
	}
}

void ANTGame_Pawn::PostNetReceiveAngularVelocity(const FVector& InAngVelocity)
{
	if (Role == ROLE_SimulatedProxy)
	{
		GetPhysicsMovement()->Omega = InAngVelocity;
		GetPhysicsMovement()->UpdatedPrimitive->SetPhysicsAngularVelocity(InAngVelocity, false);
	}
}

void ANTGame_Pawn::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	if (Role == ROLE_SimulatedProxy)
	{
		GetPhysicsMovement()->Velocity = NewVelocity;
		GetPhysicsMovement()->UpdatedPrimitive->SetPhysicsLinearVelocity(NewVelocity, false);
	}
}