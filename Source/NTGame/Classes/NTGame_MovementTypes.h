// Copyright (C) James Baxter 2017. All Rights Reserved.

#pragma once

#include "NTGame_MovementTypes.generated.h"

// Declarations
class ANTGame_Pawn;
class UNTGame_MovementComponent;
class UWorld;

///////////////////////////////////
///// Generic Input Structure /////
///////////////////////////////////

/* TODO: Make this more generic / overrideable, so that projects using this as a plugin can send custom input structs */
USTRUCT()
struct FRepPlayerInput
{
	GENERATED_BODY()

	/* Input Axis Values. MUST BE BETWEEN -1.f and 1.f! */
	/* These get quantized. You could bools and pack these into a single byte if you only care whether or not an axis is positive or negative (i.e, keyboard only) */
	UPROPERTY() float ForwardAxis;
	UPROPERTY() float StrafeAxis;
	UPROPERTY() float SteerAxis;
	UPROPERTY() float PitchAxis;
	
	/* Input States Compressed to byte (e.g, Jump, Boost etc.) */
	UPROPERTY() uint8 ControlFlags;

	/* Constructor */
	FRepPlayerInput()
		: ForwardAxis(0.f)
		, StrafeAxis(0.f)
		, SteerAxis(0.f)
		, PitchAxis(0.f)
		, ControlFlags(0)
	{}

	/* Binary Serialization */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		// Start As True
		// Write Packing
		uint8 ByteForward = CompressByte(ForwardAxis);
		uint8 ByteStrafe = CompressByte(StrafeAxis);
		uint8 ByteSteer = CompressByte(SteerAxis);
		uint8 BytePitch = CompressByte(PitchAxis);
		uint8 ByteControl = ControlFlags;

		uint8 B = (ByteForward != 0);
		Ar.SerializeBits(&B, 1);
		if (B) Ar << ByteForward; else ByteForward = 0;

		B = (ByteStrafe != 0);
		Ar.SerializeBits(&B, 1);
		if (B) Ar << ByteStrafe; else ByteStrafe = 0;

		B = (ByteSteer != 0);
		Ar.SerializeBits(&B, 1);
		if (B) Ar << ByteSteer; else ByteSteer = 0;

		B = (BytePitch != 0);
		Ar.SerializeBits(&B, 1);
		if (B) Ar << BytePitch; else BytePitch = 0;

		B = (ByteControl != 0);
		Ar.SerializeBits(&B, 1);
		if (B) Ar << ByteControl; else ByteControl = 0;

		if (Ar.IsLoading())
		{
			ForwardAxis = DecompressByte(ByteForward);
			StrafeAxis = DecompressByte(ByteStrafe);
			SteerAxis = DecompressByte(ByteSteer);
			SteerAxis = DecompressByte(ByteSteer);
			ControlFlags = ByteControl;
		}

		return true;
	}

	/* Compress / Decompress Axes to Bytes */
	FORCEINLINE uint8 CompressByte(const float X) const
	{
		// Need an ODD number of combinations so that we can quantize perfectly to zero (so we use 254 instead of 256)
		return (uint8)(((X * 0.5f) + 0.5f) * 254);
	}
	FORCEINLINE float DecompressByte(const uint8 X) const
	{
		return (((float)X / 254.f) * 2.f) - 1.f;
	}

	FORCEINLINE uint16 CompressShort(const float X) const
	{
		// Odd combinations so we can quantize perfectly on zero
		return (uint16)(((X * 0.5f) + 0.5f) * 65534);	//65535
	}
	FORCEINLINE float DecompressShort(const uint16 X) const
	{
		return (((float)X / 65534.f) * 2.f) - 1.f;
	}

	FString ToString() const
	{
		return TEXT("Fwrd: ") + FString::SanitizeFloat(ForwardAxis) + TEXT(" == ")
			+ TEXT("Strf: ") + FString::SanitizeFloat(StrafeAxis) + TEXT(" == ")
			+ TEXT("Stee: ") + FString::SanitizeFloat(SteerAxis) + TEXT(" == ")
			+ TEXT("Ptch: ") + FString::SanitizeFloat(PitchAxis) + TEXT(" == ");
	}

	// Operator Overloads
	bool operator==(const FRepPlayerInput& Other)const
	{
		return ForwardAxis == Other.ForwardAxis
			&& StrafeAxis == Other.StrafeAxis
			&& SteerAxis == Other.SteerAxis
			&& PitchAxis == Other.PitchAxis
			&& ControlFlags == Other.ControlFlags;
	}

	bool operator!=(const FRepPlayerInput& Other)const
	{
		return !(*this == Other);
	}
};

/* Enables Net Serialization of FRepPlayerInput */
template<>
struct TStructOpsTypeTraits<FRepPlayerInput> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true
	};
};

/////////////////////////
///// Pawn Movement /////
/////////////////////////

USTRUCT()
struct FRepPawnMoveData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()	FVector Location;
	UPROPERTY()	FQuat Rotation;
	UPROPERTY()	FVector LinearVelocity;
	UPROPERTY()	FVector AngularVelocity;

	FRepPawnMoveData()
		: Location(FVector::ZeroVector)
		, Rotation(FQuat::Identity)
		, LinearVelocity(FVector::ZeroVector)
		, AngularVelocity(FVector::ZeroVector)
	{}

	FRepPawnMoveData(const FVector& InLoc, const FQuat& InQuat, const FVector& InLinVeloc, const FVector& InAngVeloc)
		: Location(InLoc)
		, Rotation(InQuat)
		, LinearVelocity(InLinVeloc)
		, AngularVelocity(InAngVeloc)
	{}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;

		// 2 DP of precision
		bOutSuccess &= SerializePackedVector<100, 30>(Location, Ar);
		bOutSuccess &= Rotation.Serialize(Ar);									// FQuat also has a NetSerialize Function.. absolutely no idea where it's defined though.
		bOutSuccess &= SerializePackedVector<100, 30>(LinearVelocity, Ar);
		bOutSuccess &= SerializePackedVector<100, 30>(AngularVelocity, Ar);

		return true;
	}
};

/* Enables Net Serialization of FRepPawnMoveData */
template<>
struct TStructOpsTypeTraits<FRepPawnMoveData> : public TStructOpsTypeTraitsBase
{
	enum
	{
		WithNetSerializer = true
	};
};

USTRUCT()
struct FPawnMovementPostPhysicsTickFunction : public FTickFunction
{
	GENERATED_BODY()

public:
	/* Target for Tick Function */
	UNTGame_MovementComponent* MoveComponent;

	virtual void ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) override;
};

///////////////////////////////////
///// Network Prediction Data /////
///////////////////////////////////

/* Update Mode */
UENUM()
enum class EPostUpdateMode_PhysPawn : uint8
{
	PU_PrePhysRecord,
	PU_PrePhysReplay,
	PU_PostPhysRecord,
	PU_PostPhysReplay,
};

/* Client Adjustment */
struct NTGAME_API FClientPhysPawnAdjustment
{
public:
	FClientPhysPawnAdjustment()
		: TimeStamp(0.f)
		, DeltaTime(0.f)
		, MoveData(FRepPawnMoveData())
		, bAckGoodMove(false)
	{}

	float TimeStamp;
	float DeltaTime;
	FRepPawnMoveData MoveData;
	bool bAckGoodMove;
};

///////////////////////////////////
///// Simplified Network Data /////
///////////////////////////////////

typedef TSharedPtr<class FSavedPhysicsMove> FSavedPhysicsMovePtr;

class NTGAME_API FSavedPhysicsMove
{
public:
	// Constructor / Destructor
	FSavedPhysicsMove() {}
	virtual ~FSavedPhysicsMove() {}

	// Stored Data to replay this move
	float MoveTimestamp;
	float MoveDeltaTime;

	// Input used for acceleration calculation for this move
	FRepPlayerInput MoveInput;

	// Movement States before / after move is simulated.
	FRepPawnMoveData StartMoveData;
	FRepPawnMoveData EndMoveData;

	// Don't use this yet, essentially used to combine moves together and save network bandwidth
	uint8 bForceNoCombine : 1;
	// Whether timestamp is invalid when we detect a timestamp discrepancy
	uint8 bHasInvalidTimeStampWhenStampsReset : 1;

	void Clear();
	void PreUpdate(const UNTGame_MovementComponent* InComponent);
	void PostUpdate(const UNTGame_MovementComponent* InComponent);

	bool IsImportantMove(const FSavedPhysicsMovePtr& LastAckedMove) const;
	bool CanCombineWith(const FSavedPhysicsMovePtr& OtherPendingMove) const;
};

class NTGAME_API FNetworkPredictionData_Client_Physics : public FNetworkPredictionData_Client, protected FNoncopyable
{
public:
	FNetworkPredictionData_Client_Physics();
	virtual ~FNetworkPredictionData_Client_Physics() {}

	static const int32 MaxFreeMoves;
	static const int32 MaxSavedMoves;
	static const float MaxMoveDeltaTime;

	float ClientUpdateTime;
	float CurrentTimeStamp;
	uint8 bNeedsReplay : 1;

	TArray<FSavedPhysicsMovePtr> SavedMoves;
	TArray<FSavedPhysicsMovePtr> FreeMoves;
	FSavedPhysicsMovePtr PendingMove;
	FSavedPhysicsMovePtr LastAckedMove;
	FSavedPhysicsMovePtr CurrentMove;

	int32 GetMoveIndexFromTimeStamp(const float TimeStamp) const;
	void AcknowledgeMove(const int32 AckMoveIndex);
	void FreeMove(const FSavedPhysicsMovePtr& FreedMove);
	FSavedPhysicsMovePtr AllocateNewMove() { return FSavedPhysicsMovePtr(new FSavedPhysicsMove()); }
	FSavedPhysicsMovePtr CreateSavedMove();

	float UpdateTimeStampAndDeltaTime(const float InDeltaTime, const float MinTimeBetweenResets);
};

class NTGAME_API FNetworkPredictionData_Server_Physics : public FNetworkPredictionData_Server, protected FNoncopyable
{
public:
	FNetworkPredictionData_Server_Physics(const UWorld* InWorld);
	virtual ~FNetworkPredictionData_Server_Physics() {}
	
	float CurrentClientTimeStamp;
	float LastUpdateTime;
	float ServerTimeStampLastServerMove;

	FSavedPhysicsMovePtr CurrentlyProcessingClientMove;

	uint8 bForceClientUpdate : 1;
	uint8 bResolvingTimeDiscrepancy : 1;

	float LifetimeRawTimeDiscrepancy;
	float TimeDiscrepancy;
	float TimeDiscrepancyResolutionMoveDeltaOverride;
	float TimeDiscrepancyAccumulatedClientDeltasSinceLastServerTick;
	float WorldCreationTime;

	float GetServerMoveDeltaTime(const float ClientTimeStamp) const;
	float GetBaseServerMoveDeltaTime(const float ClientTimeStamp) const;

	FSavedPhysicsMovePtr CreateProcessingMove(const float MoveTimeStamp, const float AccelDelta, const FRepPlayerInput& ClientInput, const FRepPawnMoveData& ClientEndData);
};