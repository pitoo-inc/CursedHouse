// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/ConditionSourceComponent.h"

// Sets default values for this component's properties
UConditionSourceComponent::UConditionSourceComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UConditionSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UConditionSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

// 条件をアクティブにする関数（BP_EventTriggerから呼び出される）
void UConditionSourceComponent::ActivateCondition()
{
    // SourceBehaviorに応じてロジックを分岐
    if (SourceBehavior == ESourceBehavior::Toggle)
    {
        // トグル式：状態を反転
        bConditionMet = !bConditionMet;
        // 状態変更を通知
        OnConditionStateChanged.Broadcast(this, bConditionMet);
    }
    else if (SourceBehavior == ESourceBehavior::LatchOn)
    {
        // ラッチ式：まだオフの場合のみオンにし、それ以降は状態を維持
        if (!bConditionMet)
        {
            bConditionMet = true;
            // 状態変更を通知
            OnConditionStateChanged.Broadcast(this, bConditionMet);
        }
    }
}
