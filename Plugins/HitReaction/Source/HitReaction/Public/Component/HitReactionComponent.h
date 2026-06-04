// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DataAsset/HitReactionProfile.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "HitReactionComponent.generated.h"

// よろけイベントのデリゲート宣言
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnStaggerTriggeredSignature, FStaggerData, StaggerData, FVector, HitDirection, UPhysicalMaterial*, HitPhysMat);

// 死亡イベントのデリゲート宣言
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeathSignature);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class HITREACTION_API UHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHitReactionComponent();

	// エディタ（BP）から割り当てる「敵ごとのよろけ設定」
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReaction")
	TObjectPtr<UHitReactionProfile> HitReactionProfile;

	// 外部（インターフェース等）から呼ばれる、ダメージ加算のメイン関数
	UFUNCTION(BlueprintCallable, Category = "HitReaction")
	void ApplyPartDamage(float DamageAmount, UPhysicalMaterial* HitPhysMat, FVector HitDirection, AActor* DamageCauser);

	// よろけが発生したときにBP側で受け取るためのイベント
	UPROPERTY(BlueprintAssignable, Category = "HitReaction")
	FOnStaggerTriggeredSignature OnStaggerTriggered; 

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 現在の体力（全体HP用）を管理する変数
	UPROPERTY(BlueprintReadOnly, Category = "HitReaction", meta = (AllowPrivateAccess = "true"))
	float CurrentHealth;

	// 死亡状態を管理する変数
	UPROPERTY(BlueprintReadOnly, Category = "HitReaction", meta = (AllowPrivateAccess = "true"))
	bool bIsDead = false;

public:	
	// 部位ごとの「現在の蓄積ダメージ」を裏で管理するMap
	UPROPERTY()
	TMap<TObjectPtr<UPhysicalMaterial>, float> AccumulatedDamageMap;

	// 死亡イベントをBP側で受け取るためのイベント
	UPROPERTY(BlueprintAssignable, Category = "HitReaction")
	FOnDeathSignature OnDeath;
};
