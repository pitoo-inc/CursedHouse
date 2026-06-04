// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ConditionSourceComponent.generated.h"

// 条件ソースのタイプを定義するEnum
// BP_EventTriggerによってGameEventDataから設定される
UENUM(BlueprintType)
enum class ESourceBehavior : uint8
{
	Toggle UMETA(DisplayName = "Toggle"),
	LatchOn UMETA(DisplayName = "Latch On"),
};

// 条件の状態が変化したときに呼び出されるデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConditionStateChanged, UConditionSourceComponent*, ConditionSourceComponent, bool, bConditionMet);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EVENTSYSTEM_API UConditionSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UConditionSourceComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// 条件が満たされているかどうかを示すプロパティ
	UPROPERTY(BlueprintReadWrite, Category = "Condition")
	bool bConditionMet = false;

	// 条件ソースの動作タイプを指定するプロパティ
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition")
	ESourceBehavior SourceBehavior = ESourceBehavior::Toggle;

	// 条件の状態が変化したときに呼び出されるイベント
	UPROPERTY(BlueprintAssignable, Category = "Condition")
	FOnConditionStateChanged OnConditionStateChanged;

	// 条件をアクティブにする関数（BP_EventTriggerから呼び出される）
	UFUNCTION(BlueprintCallable, Category = "Condition")
	void ActivateCondition();
};
