#include "Subsystems/AttentionChecker.h"

void UAttentionChecker::Reinit()
{
    WatchPairs.Empty();
}

void UAttentionChecker::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    for (int i = WatchPairs.Num() - 1; i >= 0; i--)
    {
        auto& pair = WatchPairs[i];

        if (!pair.WatchDelegate->OnCapture.IsBound() &&
            !pair.WatchDelegate->OnLose.IsBound() || 
            pair.Player == NULL || pair.Target == NULL)
        {
            WatchPairs.RemoveAt(i);
            continue;
        }
        
        bool curVisibility = CheckConeVisibility(pair.Player, pair.Target, pair.ConeAngle, pair.MaxDistance);
        if (curVisibility && pair.ConsiderCollision)
            curVisibility &= CheckPhysicsVisibility(pair.Player, pair.Target);

        UpdatePerception(pair, curVisibility, DeltaTime);

        EAttentionState newAttentionState = EAttentionState::Undetermined;
        if (pair.CurrentPerception >= 1.f)
            newAttentionState = EAttentionState::Captured;
        if (pair.CurrentPerception <= 0.f)
            newAttentionState = EAttentionState::Lost;

        bool detectedChange = pair.CurrentState != EAttentionState::Undetermined && newAttentionState != EAttentionState::Undetermined && pair.CurrentState != newAttentionState;

        if (detectedChange)
        {
            pair.CurrentState = newAttentionState;
            // WatchPairs[i] = pair;

            switch (pair.CurrentState)
            {
                case EAttentionState::Captured:
                    pair.WatchDelegate->OnCapture.Broadcast(pair.Target);
                    break;
                case EAttentionState::Lost:
                    pair.WatchDelegate->OnLose.Broadcast(pair.Target);
                    break;
            }

            if (pair.DetectOnce)
            {
                WatchPairs.RemoveAt(i);
            }
        }
    }
}

UAttentionWatchDelegate* UAttentionChecker::WatchTarget(USceneComponent* Target, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate, float MaxDistance, bool ConsiderCollision)
{
    auto* playerChar = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    auto* playerCamera = playerChar->FindComponentByClass<UCameraComponent>();

    return GetOrAddPair(
        FAttentionWatchPair(playerCamera, Target, MaxDistance, ConsiderCollision),
        CaptureDelegate, LoseDelegate
    );
}

UAttentionWatchDelegate* UAttentionChecker::DetectVisible(USceneComponent* Target, const FAttentionBPDelegate& Delegate, float MaxDistance, bool ConsiderCollision)
{
    auto* playerChar = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    auto* playerCamera = playerChar->FindComponentByClass<UCameraComponent>();

    auto searchPair = FAttentionWatchPair(playerCamera, Target, MaxDistance, ConsiderCollision);
    searchPair.DetectOnce = true;
    searchPair.CurrentState = EAttentionState::Lost;
    return GetOrAddPair(searchPair, Delegate, FAttentionBPDelegate());
}

UAttentionWatchDelegate* UAttentionChecker::DetectInvisible(USceneComponent* Target, const FAttentionBPDelegate& Delegate, float MaxDistance, bool ConsiderCollision)
{
    auto* playerChar = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    auto* playerCamera = playerChar->FindComponentByClass<UCameraComponent>();

    auto searchPair = FAttentionWatchPair(playerCamera, Target, MaxDistance, ConsiderCollision);
    searchPair.DetectOnce = true;
    searchPair.CurrentState = EAttentionState::Captured;
    return GetOrAddPair(searchPair, FAttentionBPDelegate(), Delegate);
}

UAttentionWatchDelegate* UAttentionChecker::GetWatchTarget(USceneComponent* Target)
{
    for (auto& pair : WatchPairs)
    {
        if (pair.Target == Target)
        {
            return pair.WatchDelegate;
        }
    }
    
    return NULL;
}

UAttentionWatchDelegate* UAttentionChecker::UnwatchTarget(USceneComponent* Target, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate)
{
    auto* foundPairDelegate = GetWatchTarget(Target);
    if (foundPairDelegate != NULL)
    {
        if (CaptureDelegate.IsBound())
            foundPairDelegate->OnCapture.Remove(CaptureDelegate);
        if (LoseDelegate.IsBound())
            foundPairDelegate->OnLose.Remove(LoseDelegate);
    }

    return foundPairDelegate;
}

UAttentionWatchDelegate* UAttentionChecker::GetOrAddPair(FAttentionWatchPair NewPair, const FAttentionBPDelegate& CaptureDelegate, const FAttentionBPDelegate& LoseDelegate)
{
    int foundPairIdx;
    if (WatchPairs.Find(NewPair, foundPairIdx))
    {
        NewPair = WatchPairs[foundPairIdx];
    }
    else
    {
        NewPair.WatchDelegate = NewObject<UAttentionWatchDelegate>(this);
        WatchPairs.Add(NewPair);
    }

    if (CaptureDelegate.IsBound())
        NewPair.WatchDelegate->OnCapture.Add(CaptureDelegate); // / TODO: If possible avoid multiple binding
    if (LoseDelegate.IsBound())
        NewPair.WatchDelegate->OnLose.Add(LoseDelegate);

    return NewPair.WatchDelegate;
}

bool UAttentionChecker::CheckConeVisibility(USceneComponent* Observer, USceneComponent* Target, float ConeAngle, float MaxDistance)
{
	// calc angle from forward to direction to this actor
	FVector cameraLoc = Observer->GetComponentLocation();
	FVector cameraForward = Observer->GetForwardVector();
	FVector targetLoc = Target->GetComponentLocation();
	FVector toPlayer = targetLoc - cameraLoc;
	bool isInBounds = MaxDistance <= 0 || toPlayer.SizeSquared() < MaxDistance * MaxDistance;
	toPlayer.Normalize();
	float angle = FMath::Acos(FVector::DotProduct(cameraForward, toPlayer));
	angle = FMath::RadiansToDegrees(angle);
	isInBounds &= angle <= ConeAngle;
	
	return isInBounds;
}

bool UAttentionChecker::CheckPhysicsVisibility(USceneComponent* Observer, USceneComponent* Target)
{
    // TODO: Implement this
	return true;
}

void UAttentionChecker::UpdatePerception(FAttentionWatchPair& Pair, bool IsVisible, float DeltaTime)
{
    float perceptionChangeSign = IsVisible ? 1.f : -1.f;
    float changeVelocity = IsVisible ? Pair.CaptureDelay : Pair.LoseDelay;

    float perceptionDelta;
    if (changeVelocity > 0.f)
    {
        perceptionDelta = changeVelocity * DeltaTime * perceptionChangeSign;
    }
    else
    {
        perceptionDelta = perceptionChangeSign;
    }

    float newPerceptionValue = Pair.CurrentPerception + perceptionDelta;
    newPerceptionValue = FMath::Clamp(newPerceptionValue, 0.f, 1.f);
    Pair.CurrentPerception = newPerceptionValue;
}
