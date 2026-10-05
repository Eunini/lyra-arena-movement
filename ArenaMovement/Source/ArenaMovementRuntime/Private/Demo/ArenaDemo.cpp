#include "Demo/ArenaDemo.h"
#include "Demo/PortfolioVisual.h"
#include "Abilities/ArenaGameplayAbility_Dodge.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/Attributes/LyraCombatSet.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Character/LyraPawnData.h"
#include "System/LyraGameData.h"
#include "AbilitySystemGlobals.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Demo/PortfolioCapture.h"
#include "Misc/Parse.h"

namespace
{
 const FVector Checkpoints[]={FVector(300,0,0),FVector(1000,-200,0),FVector(1750,200,0),FVector(2600,0,0)};
}
AArenaDemoCharacter::AArenaDemoCharacter(const FObjectInitializer& Initializer):Super(Initializer)
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.bStartWithTickEnabled=true;
 bUseControllerRotationYaw=true;
 DemoASC=CreateDefaultSubobject<ULyraAbilitySystemComponent>(TEXT("DemoAbilitySystem"));
 DemoASC->SetIsReplicated(true);DemoASC->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
 Health=CreateDefaultSubobject<ULyraHealthSet>(TEXT("DemoHealth"));
 Combat=CreateDefaultSubobject<ULyraCombatSet>(TEXT("DemoCombat"));
 FirstPerson=CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPerson"));
 FirstPerson->SetupAttachment(GetRootComponent());FirstPerson->SetRelativeLocation(FVector(0,0,64));
 FirstPerson->bUsePawnControlRotation=true;
 Marker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlayerMarker"));
 Marker->SetupAttachment(GetRootComponent());
 Marker->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
 Marker->SetRelativeScale3D(FVector(.5f,.5f,1.65f));Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Marker->SetOwnerNoSee(true);
}
void AArenaDemoCharacter::BeginPlay()
{
 Super::BeginPlay();
 if (HasAuthority())
 {
  DemoASC->SetNumericAttributeBase(ULyraHealthSet::GetMaxHealthAttribute(),100.f);
  DemoASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(),100.f);
 }
 InitializeDemoAbilities();
 MarkerMaterial=PortfolioVisual::Color(Marker,FLinearColor(.12f,.55f,.72f));
 if (GetClass()==StaticClass()) GetArenaMovement()->OnDodge.AddUObject(this,&AArenaDemoCharacter::PulseDodge);
}
void UArenaDemoAssetManager::StartInitialLoading()
{
 UAssetManager::StartInitialLoading();
 UAbilitySystemGlobals::Get().InitGlobalData();
 GameDataMap.Add(ULyraGameData::StaticClass(),NewObject<ULyraGameData>(this));
}
void AArenaDemoCharacter::InitializeDemoAbilities()
{
 if (bAbilityReady) return;
 auto* Extension=ULyraPawnExtensionComponent::FindPawnExtensionComponent(this);
 if (!Extension) return;
 if (HasAuthority() && !Extension->GetPawnData<ULyraPawnData>())
  if (const auto* Data=LoadObject<ULyraPawnData>(nullptr,TEXT("/Game/ArenaDemo/DA_Runner.DA_Runner"))) Extension->SetPawnData(Data);
 if (!Extension->GetPawnData<ULyraPawnData>()) return;
 Extension->InitializeAbilitySystem(DemoASC,this);
 if (HasAuthority()) DemoASC->GiveAbility(FGameplayAbilitySpec(UArenaGameplayAbility_Dodge::StaticClass(),1));
 bAbilityReady=true;
}
void AArenaDemoCharacter::CalcCamera(float Delta,FMinimalViewInfo& View){FirstPerson->GetCameraView(Delta,View);}
void AArenaDemoCharacter::OnJumped_Implementation()
{
 Super::OnJumped_Implementation();
 if (JumpCurrentCount>=2)++AirJumps;
}
void AArenaDemoCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
 Super::SetupPlayerInputComponent(Input);
 Input->BindAxis(TEXT("ArenaForward"),this,&AArenaDemoCharacter::Forward);
 Input->BindAxis(TEXT("ArenaRight"),this,&AArenaDemoCharacter::Right);
 Input->BindAxis(TEXT("ArenaYaw"),this,&AArenaDemoCharacter::Yaw);
 Input->BindAxis(TEXT("ArenaPitch"),this,&AArenaDemoCharacter::Pitch);
 Input->BindAction(TEXT("ArenaJump"),IE_Pressed,this,&ACharacter::Jump);
 Input->BindAction(TEXT("ArenaJump"),IE_Released,this,&ACharacter::StopJumping);
 Input->BindAction(TEXT("ArenaDodge"),IE_Pressed,this,&AArenaDemoCharacter::Dodge);
 Input->BindAction(TEXT("ArenaReset"),IE_Pressed,this,&AArenaDemoCharacter::ResetCourse);
}
void AArenaDemoCharacter::Forward(float V){if(Controller) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::X),V);}
void AArenaDemoCharacter::Right(float V){if(Controller) AddMovementInput(FRotationMatrix(FRotator(0,Controller->GetControlRotation().Yaw,0)).GetUnitAxis(EAxis::Y),V);}
void AArenaDemoCharacter::Yaw(float V){AddControllerYawInput(V);}
void AArenaDemoCharacter::Pitch(float V){AddControllerPitchInput(V);}
void AArenaDemoCharacter::Dodge(){DemoASC->TryActivateAbilityByClass(UArenaGameplayAbility_Dodge::StaticClass());}
void AArenaDemoCharacter::PulseDodge(EArenaDodgeKind Kind)
{
 if (Kind==EArenaDodgeKind::Ground)++GroundDodges;
 if (Kind==EArenaDodgeKind::Wall)++WallDodges;
 PulseUntil=GetWorld()->GetTimeSeconds()+.25f;
 if (MarkerMaterial) MarkerMaterial->SetVectorParameterValue(TEXT("Tint"),Kind==EArenaDodgeKind::Wall?FLinearColor(.7f,.3f,.95f):FLinearColor(.95f,.5f,.1f));
}
void AArenaDemoCharacter::ResetCourse()
{
 StartedAt=FinishedAt=-1.f;Checkpoint=0;ServerResetCourse();
}
void AArenaDemoCharacter::ServerResetCourse_Implementation()
{
 GetCharacterMovement()->StopMovementImmediately();SetActorLocation(FVector(-500,0,100));
}
float AArenaDemoCharacter::GetRunTime() const
{
 if (StartedAt<0) return 0;
 return (FinishedAt<0?GetWorld()->GetTimeSeconds():FinishedAt)-StartedAt;
}
void AArenaDemoCharacter::Tick(float Delta)
{
 Super::Tick(Delta);
 InitializeDemoAbilities();
 if (IsLocallyControlled() && FParse::Param(FCommandLine::Get(),TEXT("PortfolioDemo")))
 {
  const auto* Capture=GetWorld()->GetSubsystem<UArenaCaptureSubsystem>();
  if (!FParse::Param(FCommandLine::Get(),TEXT("PortfolioCapture")) || (Capture && Capture->IsReady())) AdvanceDemo(Delta);
 }
 if (MarkerMaterial && GetWorld()->GetTimeSeconds()>PulseUntil) MarkerMaterial->SetVectorParameterValue(TEXT("Tint"),FLinearColor(.12f,.55f,.72f));
 if (!IsLocallyControlled() || FinishedAt>=0) return;
 if (StartedAt<0 && GetVelocity().Size2D()>10) StartedAt=GetWorld()->GetTimeSeconds();
 if (Checkpoint<4 && FVector::DistSquared2D(GetActorLocation(),Checkpoints[Checkpoint])<FMath::Square(180.f))
 {
  ++Checkpoint;
  if (Checkpoint==4) FinishedAt=GetWorld()->GetTimeSeconds();
 }
}

void AArenaDemoCharacter::AdvanceDemo(float Delta)
{
 DemoAge+=Delta;StepAge+=Delta;
 if (!Controller || !bAbilityReady || DemoAge<2.f || FinishedAt>=0) return;
 auto Next=[this](){++DemoStep;StepAge=0;};
 auto Move=[this,Delta](FVector Goal,float Radius=70.f)->bool
 {
  FVector Offset=Goal-GetActorLocation();Offset.Z=0;
  Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(),Offset.Rotation(),Delta,5.f));
  if (Offset.Size2D()<Radius) return true;
  AddMovementInput(Offset.GetSafeNormal2D());return false;
 };
 switch (DemoStep)
 {
  case 0:if(Move(FVector(-100,0,0))){Dodge();Next();}break;
  case 1:if(Move(Checkpoints[0],140.f))Next();break;
  case 2:if(Move(FVector(300,-330,0)))Next();break;
  case 3:if(Move(FVector(850,-330,0)))Next();break;
  case 4:if(Move(FVector(1030,-365,0),15.f)){Jump();Next();}break;
  case 5:
   Controller->SetControlRotation(FRotator(0,90,0));AddMovementInput(FVector(0,1,0));
   if(StepAge>.05f){StopJumping();Dodge();Next();}break;
  case 6:if(Move(Checkpoints[1],140.f))Next();break;
  case 7:if(Move(FVector(1500,200,0))){Jump();Next();}break;
  case 8:AddMovementInput(FVector(1,0,0));if(StepAge>.25f){StopJumping();Jump();Next();}break;
  case 9:if(Move(Checkpoints[2],150.f)){StopJumping();Next();}break;
  case 10:if(Move(FVector(1750,650,0)))Next();break;
  case 11:if(Move(FVector(2600,650,0)))Next();break;
  default:Move(Checkpoints[3],150.f);break;
 }

}
AArenaDemoWorld::AArenaDemoWorld(){SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));}
void AArenaDemoWorld::OnConstruction(const FTransform& Transform)
{
 Super::OnConstruction(Transform);
 using namespace PortfolioVisual;Clear(this);Environment(this);
 const FLinearColor Dark(.04f,.07f,.11f),Stone(.22f,.29f,.35f),Orange(.95f,.36f,.1f),Blue(.12f,.62f,.7f);
 Part(this,FVector(1000,0,-50),FVector(5000,2400,100),Dark);
 for(int32 Side:{-1,1})
 {
  Part(this,FVector(1000,Side*1050,210),FVector(4800,80,420),Stone);
  Part(this,FVector(1000,Side*990,380),FVector(4600,20,30),Blue,false);
 }
 Part(this,FVector(650,120,80),FVector(280,550,160),Stone);
 Part(this,FVector(1300,-460,300),FVector(900,80,600),Stone);
 Part(this,FVector(1820,-180,140),FVector(600,430,280),Stone);
 Part(this,FVector(2250,250,100),FVector(270,450,200),Stone);
 for (int32 i=0;i<4;++i)
 {
  Part(this,Checkpoints[i]+FVector(0,0,5),FVector(170,170,12),Orange,false);
  Label(this,FString::Printf(TEXT("0%d"),i+1),Checkpoints[i]+FVector(0,0,330),65);
 }
 Label(this,TEXT("VECTOR / ARENA MOVEMENT"),FVector(-1100,0,630),85);
 Label(this,TEXT("WALL DODGE"),FVector(1240,-440,680),50);
 Label(this,TEXT("DOUBLE JUMP"),FVector(1750,0,600),50);
 Label(this,TEXT("FINISH"),FVector(2600,0,650),80);
}
AArenaDemoGameMode::AArenaDemoGameMode()
{
 DefaultPawnClass=AArenaDemoCharacter::StaticClass();HUDClass=AArenaDemoHUD::StaticClass();
 if (!IsRunningCommandlet())
 {
  static ConstructorHelpers::FClassFinder<APawn> Blueprint(TEXT("/Game/ArenaDemo/Blueprints/BP_ArenaRunner"));
   if (Blueprint.Succeeded()) DefaultPawnClass=Blueprint.Class;
 }
}
void AArenaDemoGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
 Super::InitGame(MapName,Options,ErrorMessage);
 if (!TActorIterator<AArenaDemoWorld>(GetWorld())) GetWorld()->SpawnActor<AArenaDemoWorld>();
}
void AArenaDemoHUD::DrawHUD()
{
 Super::DrawHUD();
 auto* Player=Cast<AArenaDemoCharacter>(GetOwningPawn());if (!Canvas || !Player) return;
 const float W=Canvas->ClipX,H=Canvas->ClipY;
 const FLinearColor Text(.88f,.94f,.95f),Orange(.98f,.46f,.18f),Dark(.02f,.035f,.05f,.85f);
 DrawRect(Dark,0,0,W,112);DrawText(TEXT("VECTOR / LYRA ARENA MOVEMENT"),Text,30,24,GEngine->GetLargeFont());
 DrawText(FString::Printf(TEXT("SPEED  %.0f cm/s    CHECKPOINT  %d / 4    TIME  %.2f s"),Player->GetVelocity().Size2D(),Player->GetCheckpoint(),Player->GetRunTime()),Orange,30,70,GEngine->GetSmallFont());
 DrawText(FString::Printf(TEXT("GROUND DODGES  %d    WALL DODGES  %d    AIR JUMPS  %d"),Player->GetGroundDodges(),Player->GetWallDodges(),Player->GetAirJumps()),Text,30,90,GEngine->GetSmallFont());
 DrawRect(Text,W*.5f-2,H*.5f-2,4,4);
 DrawRect(Dark,0,H-60,W,60);
 DrawText(TEXT("WASD move / Mouse look / Space jump twice / Shift dodge / Jump near wall + dodge away / R restart"),Text,30,H-38,GEngine->GetSmallFont());
 if (Player->GetCheckpoint()==4) DrawText(TEXT("COURSE COMPLETE / R TO RESTART"),Orange,W*.25f,H*.3f,GEngine->GetLargeFont());
}


void AArenaDemoWorld::BeginPlay()
{
 Super::BeginPlay();OnConstruction(GetActorTransform());
}
