// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/InteractionType.h"
#include "InteractionHandlerComponent.generated.h"


// UIへ通知するためのデリゲート
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInteractionUpdate, EInteractionUIState, NewState, EInteractionType, ActionType, AActor*, TargetActor);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EVENTSYSTEM_API UInteractionHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UInteractionHandlerComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 入力（Eキー）時に呼ぶ関数
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void PerformInteract();

	// UI更新のためのデリゲート
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionUpdate OnInteractionUpdate;
private:
	void ScanForInteractables();

	// インタラクション可能な対象を見つけるためのパラメータ
	UPROPERTY(EditAnywhere, Category = "Interaction")
	float ActiveDistance = 200.0f;   // [E]アイコンが出る距離

	UPROPERTY(EditAnywhere, Category = "Interaction")
	float TraceRadius = 15.0f; // 少し太めのSphereTraceで遊びを作る

	// 現在注視している対象のインターフェースキャッシュ
	TWeakObjectPtr<UObject> CurrentTarget;
	// 現在のUI状態を保持
	EInteractionUIState CurrentUIState = EInteractionUIState::None;
};
