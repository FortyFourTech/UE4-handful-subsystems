#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "AttentionChecker.generated.h"

class UCameraComponent;
class USceneComponent;

UENUM()
enum class EAttentionState : uint8
{
    Undetermined, // switch to any determined state wont call delegate
    Captured,
    Lost,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FAttentionStateChangeDelegate, USceneComponent*, WatchedComp);
DECLARE_DYNAMIC_DELEGATE_OneParam (FAttentionBPDelegate, USceneComponent*, WatchedComp);

UCLASS(BlueprintType)
class TINYISLANDUE_API UAttentionWatchDelegate : public UObject
{
	GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FAttentionStateChangeDelegate OnCapture;
    UPROPERTY(BlueprintAssignable)
    FAttentionStateChangeDelegate OnLose;
};

USTRUCT()
struct FAttentionWatchPair
{
	GENERATED_BODY()

public:
    UCameraComponent* Player = NULL;
    USceneComponent* Target = NULL;
    float ConeAngle = 30.f;
    float MaxDistance = 1000.f; // Set to 0 or negative to skip distance check
    bool ConsiderCollision = false; // TODO: Not implemented yet
    bool DetectOnce = false;
    float CaptureDelay = 0.f;
    float LoseDelay = 0.f;
    EAttentionState CurrentState = EAttentionState::Undetermined;
    float CurrentPerception = 0.f;

    UPROPERTY() // will be garbage collected after destroying struct
    UAttentionWatchDelegate* WatchDelegate = NULL;

public:
    FAttentionWatchPair() {};

    FAttentionWatchPair(UCameraComponent* Player, USceneComponent* Target) :
        Player(Player), Target(Target) {};
    FAttentionWatchPair(UCameraComponent* Player, USceneComponent* Target, float Distance, bool ConsiderCollision = false) :
        Player(Player), Target(Target), MaxDistance(Distance), ConsiderCollision(ConsiderCollision) {};
    
    FORCEINLINE bool operator==(const FAttentionWatchPair& OtherPair) const
    {
        return this->Target == OtherPair.Target;
        //  this->Player == OtherPair.Player &&
    }
};

UCLASS()
class TINYISLANDUE_API UAttentionChecker : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UAttentionChecker() : Super() {};
	virtual TStatId GetStatId() const override { return GetStatID(); };

protected:
	void Reinit() override;
    void Tick(float DeltaTime) override;

private:
    UPROPERTY()
    TArray<FAttentionWatchPair> WatchPairs;

public:
    UFUNCTION(BlueprintCallable, Category = "AttentionChecker", Meta = (AutoCreateRefTerm = "CaptureDelegate,LoseDelegate"))
    UAttentionWatchDelegate* WatchTarget(USceneComponent* Target, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate, float MaxDistance = 1000.f, bool ConsiderCollision = false);
    UFUNCTION(BlueprintCallable, Category = "AttentionChecker")
    UAttentionWatchDelegate* DetectVisible(USceneComponent* Target, const FAttentionBPDelegate& Delegate, float MaxDistance = 1000.f, bool ConsiderCollision = false);
    UFUNCTION(BlueprintCallable, Category = "AttentionChecker")
    UAttentionWatchDelegate* DetectInvisible(USceneComponent* Target, const FAttentionBPDelegate& Delegate, float MaxDistance = 1000.f, bool ConsiderCollision = false);
    UFUNCTION(BlueprintCallable, Category = "AttentionChecker")
    UAttentionWatchDelegate* GetWatchTarget(USceneComponent* Target);
    UFUNCTION(BlueprintCallable, Category = "AttentionChecker", Meta = (AutoCreateRefTerm = "CaptureDelegate,LoseDelegate"))
    UAttentionWatchDelegate* UnwatchTarget(USceneComponent* Target, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate);

private:
    UAttentionWatchDelegate* GetOrAddPair(FAttentionWatchPair NewPair, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate);
    bool CheckConeVisibility(USceneComponent* Observer, USceneComponent* Target, float ConeAngle, float MaxDistance);
    bool CheckPhysicsVisibility(USceneComponent* Observer, USceneComponent* Target);
    void UpdatePerception(FAttentionWatchPair& Pair, bool IsVisible, float DeltaTime);
};
