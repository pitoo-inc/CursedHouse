// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/InteractableInterface.h"
#include "InteractableBase.generated.h"

class USphereComponent; 
class UWidgetComponent;
class UInteractionHUDWidget;
class ALevelSequenceActor;

UCLASS()
class EVENTSYSTEM_API AInteractableBase : public AActor , public IInteractableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AInteractableBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	// ==========================================
	// コンポーネント
	// ==========================================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* DefaultSceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* ProximitySensor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UWidgetComponent* Widget;

	// ==========================================
	// 変数
	// ==========================================
	// プレイヤーが近くにいるかどうか。UIの表示・非表示や、インタラクションの有効・無効を切り替える際に使用する。
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bIsPlayerNearby;

	// プレイヤーから注目されているかどうか。UIの表示・非表示や、インタラクションの有効・無効を切り替える際に使用する。
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bIsBeingLookedAt;

	// インタラクション可能かどうか。条件を満たしていない場合はfalseにする（例：ドアが開いているときは鍵を拾えないなど）。UIの表示・非表示もこれに基づいて行う。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract;

	// インタラクションが一度きりかどうか（例：アイテムの取得など）。trueの場合、インタラクション後に無効化される。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bIsOneShot;

	// キャッシュされたInteractionHUDウィジェット
	UPROPERTY()
	UInteractionHUDWidget* CachedInteractionHUD;
	
	//レベルシーケンスを再生するための変数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	ALevelSequenceActor* TargetSequence;


	// ==========================================
	// 関数・イベント
	// ==========================================
	// Overlapイベント
	UFUNCTION()
	void OnProximityOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProximityOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// BPの詳細パネルで設定するタイプ（デフォルトはNone）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	EInteractionType InteractType = EInteractionType::None;

	// IInteractableInterfaceの実装
	UFUNCTION()
	void ReceiveInteractionUpdate(EInteractionUIState NewState, EInteractionType ActionType, AActor* TargetActor);

	// UIの状態を更新する関数
	UFUNCTION(BlueprintCallable, Category = "Interaction|UI")
	void RefreshUIState();
};
