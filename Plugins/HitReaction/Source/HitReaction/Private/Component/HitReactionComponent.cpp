// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/HitReactionComponent.h"

// Sets default values for this component's properties
UHitReactionComponent::UHitReactionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	// 減衰処理を行うためにTickを有効にする
	PrimaryComponentTick.bCanEverTick = true;

	// データアセットから初期体力を設定
	if(HitReactionProfile)
	{
		CurrentHealth = HitReactionProfile->MaxHealth;
	}
}


void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!HitReactionProfile || AccumulatedDamageMap.Num() == 0) return;

	// 全部位の蓄積値を少しずつ減らす
	for (auto& Elem : AccumulatedDamageMap)
	{
		if (Elem.Value > 0.0f)
		{
			// 0以下にならないように計算
			Elem.Value = FMath::Max(0.0f, Elem.Value - (HitReactionProfile->RecoveryRatePerSecond * DeltaTime));
		}
	}
}


void UHitReactionComponent::ApplyPartDamage(float DamageAmount, UPhysicalMaterial* HitPhysMat, FVector HitDirection, AActor * DamageCauser)
{
	UE_LOG(LogTemp, Warning, TEXT("ApplyPartDamage Called! Amount: %f"), DamageAmount);

	// 安全対策1：すでに死亡している、または必要なデータがない場合は無視
	if (bIsDead ||!HitPhysMat || !HitReactionProfile)
	{
		return;
	}

	// 全体HP用のダメージ処理
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (CurrentHealth <= 0.0f)
	{
		bIsDead = true;
		OnDeath.Broadcast(); // 死亡！
		return; // 死んだらよろけ判定は不要なので抜ける
	}

	// 安全対策2：撃たれた部位が「よろけ設定」に含まれていなければ無視（例：普通の胴体など）
	if (!HitReactionProfile->StaggerSettings.Contains(HitPhysMat))
	{
		return;
	}

	// 現在の蓄積ダメージを取り出し、今回のダメージを加算
	float CurrentDamage = AccumulatedDamageMap.FindRef(HitPhysMat);
	CurrentDamage += DamageAmount;

	// 設定ファイルから「この部位のよろけ条件」を取得
	FStaggerData TargetData = HitReactionProfile->StaggerSettings[HitPhysMat];

	// ApplyPartDamage 関数の中の、しきい値を超えた時の処理
	if (CurrentDamage >= TargetData.Threshold)
	{
		// 修正後：デリゲートを放送（Broadcast）する
		OnStaggerTriggered.Broadcast(TargetData, HitDirection, HitPhysMat);

		AccumulatedDamageMap.Add(HitPhysMat, 0.0f);
	}
	else
	{
		// まだ耐えられるので、加算した値をMapに保存して終了
		AccumulatedDamageMap.Add(HitPhysMat, CurrentDamage);
	}

}

