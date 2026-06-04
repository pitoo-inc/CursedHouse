// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/ActivatableInterface.h"
#include "Interface/StatefulActivatableInterface.h"
#include "ActivatesBase.generated.h"

class ALevelSequenceActor;

UCLASS()
class EVENTSYSTEM_API AActivatesBase : public AActor, public IActivatableInterface, public IStatefulActivatableInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AActivatesBase();

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

	// ==========================================
	// 変数
	// ==========================================
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Activation")
	ALevelSequenceActor* TargetSequence;

};
