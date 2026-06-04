// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StatefulActivatableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UStatefulActivatableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class EVENTSYSTEM_API IStatefulActivatableInterface
{
	GENERATED_BODY()

public:
	// Blueprintで実装可能な関数。アクティベーション状態を設定する。
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Activation")
	void SetActivationState(bool bConditionMet, AActor* Activator);
};
