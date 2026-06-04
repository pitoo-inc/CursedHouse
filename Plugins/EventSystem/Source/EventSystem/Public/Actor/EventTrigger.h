// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DataAsset/GameEventData.h"
#include "EventTrigger.generated.h"

// Forward declaration
class UConditionSourceComponent;

UCLASS()
class EVENTSYSTEM_API AEventTrigger : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEventTrigger();

protected:
	virtual void BeginPlay() override;

protected:

	// トリガーするイベントのデータアセット
	UPROPERTY(EditAnywhere, Category = "Trigger")
	UGameEventData* TriggerData;

	// これまでにスイッチが押された合計回数
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Event")
	int32 CurrentPressCount = 0;

	// 途中で順番を間違えたかどうかを記憶する密かなフラグ
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RS|Event")
	bool bIsSequenceFailed = false;

	// 条件を評価して、必要ならイベントをトリガーする関数
	bool EvaluateCondition(UConditionSourceComponent* ChangedComponent = nullptr);

	// シーケンスを間違えた時用のリセット関数
	void ResetSequence();

private:
	// 各ターゲットアクターの現在のトグル状態
	UPROPERTY()
	TMap<AActor*, bool> OneShotActivationStates;

	// 前回の条件評価結果（Stateful制御のために必要）
	UPROPERTY()
	bool bLastConditionMet = false;

	// 条件ソースコンポーネントからのデリゲートにバインドする関数
	void BindToConditionSourceDelegates();

	// 条件ソースの状態変化イベントに対応する関数
	UFUNCTION()
	void OnConditionStateChanged(UConditionSourceComponent* ConditionSourceComponent, bool bConditionMet);

};
