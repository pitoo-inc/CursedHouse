// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RSInventoryComponent.generated.h"

// どちらの金庫にアクセスするかを決める列挙型
UENUM(BlueprintType)
enum class ERSInventoryType : uint8
{
	Personal UMETA(DisplayName = "Personal (PlayerState)"),
	Global   UMETA(DisplayName = "Global (GameState)")
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class EVENTSYSTEM_API URSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	URSInventoryComponent();

public:	
	// エディタの「詳細パネル」でアクセス先を切り替えられるようにする
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RS|Inventory")
	ERSInventoryType InventoryType;

	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void AddItem(FName ItemID);

	UFUNCTION(BlueprintCallable, Category = "RS|Inventory")
	void RemoveItem(FName ItemID);

	UFUNCTION(BlueprintPure, Category = "RS|Inventory")
	bool HasItem(FName ItemID) const;
		
};
