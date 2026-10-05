#pragma once
#include "CoreMinimal.h"
#include "ArenaCharacter.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "System/LyraAssetManager.h"
#include "ArenaDemo.generated.h"

UCLASS()
class ARENAMOVEMENTRUNTIME_API UArenaDemoAssetManager : public ULyraAssetManager
{
 GENERATED_BODY()
protected:
 virtual void StartInitialLoading() override;
};

UCLASS()
class ARENAMOVEMENTRUNTIME_API AArenaDemoCharacter : public AArenaCharacter
{
 GENERATED_BODY()
public:
 AArenaDemoCharacter(const FObjectInitializer& Initializer=FObjectInitializer::Get());
 virtual void BeginPlay() override;
 virtual void Tick(float Delta) override;
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 virtual void CalcCamera(float Delta,FMinimalViewInfo& View) override;
 virtual void OnJumped_Implementation() override;
 UFUNCTION(BlueprintCallable) void PulseDodge(EArenaDodgeKind Kind);
 int32 GetCheckpoint() const {return Checkpoint;}
 float GetRunTime() const;
 int32 GetGroundDodges() const {return GroundDodges;}
 int32 GetWallDodges() const {return WallDodges;}
 int32 GetAirJumps() const {return AirJumps;}
private:
 UPROPERTY() TObjectPtr<class ULyraAbilitySystemComponent> DemoASC;
 UPROPERTY() TObjectPtr<class ULyraHealthSet> Health;
 UPROPERTY() TObjectPtr<class ULyraCombatSet> Combat;
 UPROPERTY() TObjectPtr<class UCameraComponent> FirstPerson;
 UPROPERTY() TObjectPtr<class UStaticMeshComponent> Marker;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> MarkerMaterial;
 float StartedAt=-1.f,FinishedAt=-1.f,PulseUntil=0;
 int32 Checkpoint=0;
 int32 GroundDodges=0,WallDodges=0,AirJumps=0;
 bool bAbilityReady=false;
 float DemoAge=0,StepAge=0;
 int32 DemoStep=0;
 void InitializeDemoAbilities();
 void AdvanceDemo(float Delta);
 void Forward(float Value);void Right(float Value);void Yaw(float Value);void Pitch(float Value);void Dodge();void ResetCourse();
 UFUNCTION(Server,Reliable) void ServerResetCourse();
};

UCLASS()
class ARENAMOVEMENTRUNTIME_API AArenaDemoWorld : public AActor
{
 GENERATED_BODY()
public:
 AArenaDemoWorld();
 virtual void OnConstruction(const FTransform& Transform) override;
 virtual void BeginPlay() override;
};

UCLASS()
class ARENAMOVEMENTRUNTIME_API AArenaDemoGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 AArenaDemoGameMode();
 virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
};

UCLASS()
class ARENAMOVEMENTRUNTIME_API AArenaDemoHUD : public AHUD
{
 GENERATED_BODY()
public: virtual void DrawHUD() override;
};
