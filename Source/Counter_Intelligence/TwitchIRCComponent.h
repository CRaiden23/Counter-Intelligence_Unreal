

#pragma once

#include "CoreMinimal.h"
#include "TwitchIRCComponent.generated.h"

// Delegate event that fires when a user sends a message on Twitch
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMessageReceived, const FString&, Message, const FString&, User);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAuthentication);

UCLASS(Blueprintable, ClassGroup=(Twitch), meta=(BlueprintSpawnableComponent) )
class COUNTER_INTELLIGENCE_API UTwitchIRCComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UTwitchIRCComponent();
	~UTwitchIRCComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString OAuth = "OAuth_Token";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString BotUsername = "Bot_Username";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ChannelName = "Channel_To_Join";

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ChatRefreshRate = 1.0f;
	
private:
	// Delegate Functions
	UPROPERTY(BlueprintAssignable)
	FMessageReceived OnMessageReceived;
	UPROPERTY(BlueprintAssignable)
	FOnAuthentication OnAuthentication;
	
	bool bSetupComplete = false;

	FSocket* TwitchIRCSocket;
	FTimerHandle RefreshTimer;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Local Setup
	UFUNCTION(BlueprintCallable)
	void SetupUserInfo(FString OAuthToken, FString UserName, FString Channel);
	
	// TWITCH SETUP
	UFUNCTION(BlueprintCallable)
	bool Connect();
	UFUNCTION(BlueprintCallable)
	bool Authenticate();

	// Send and Receive
	bool SendMessage(FString Message) const;
	UFUNCTION(BlueprintCallable)
	bool SendToChat(FString Message, FString Channel);
	void ReceiveData();
	TArray<FString> ParseMessage(FString ReadableMessage, TArray<FString>& out_Users);
	
};
