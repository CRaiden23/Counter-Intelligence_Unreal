


#include "TwitchIRCComponent.h"

#include "TimerManager.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include <string>


// Sets default values for this component's properties
UTwitchIRCComponent::UTwitchIRCComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}

UTwitchIRCComponent::~UTwitchIRCComponent()
{
	if(TwitchIRCSocket != nullptr)
	{
		TwitchIRCSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(TwitchIRCSocket);
	}
}

// Called when the game starts
void UTwitchIRCComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void UTwitchIRCComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTwitchIRCComponent::SetupUserInfo(FString OAuthToken, FString UserName, FString Channel)
{
	this->OAuth = OAuthToken;
	this->BotUsername = UserName;
	this->ChannelName = Channel;
	bSetupComplete = true;
}

/* Establishes a connection with the Twitch IRC server */
bool UTwitchIRCComponent::Connect()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	TSharedRef<FInternetAddr> ServerAddr = SocketSubsystem->CreateInternetAddr();
	
	// resolve hostname
	//const FAddressInfoResult ServerInfo = SocketSubsystem->GetAddressInfo(TEXT("irc.twitch.tv"), TEXT("6667"), EAddressInfoFlags::Default, FName("ipv4"), ESocketType::SOCKTYPE_Streaming);
	//if(ServerInfo.ReturnCode != SE_NO_ERROR)
	
	if(SocketSubsystem->GetHostByName("irc.chat.twitch.tv", *ServerAddr) != SE_NO_ERROR)		// deprecated, but it works
		return false;
	
	ServerAddr->SetPort(6667); //IRC port

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("TwitchIRCSocket"), false);
	
	// check if socket was created
	if(Socket == nullptr)
		return false;

	if(!Socket->Connect(*ServerAddr))
	{
		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);
		
		return false;
	}

	// register the ReceiveData function to update every 5 seconds
	GetWorld()->GetTimerManager().SetTimer(this->RefreshTimer, this, &UTwitchIRCComponent::ReceiveData, 1.0f, true);
	this->TwitchIRCSocket = Socket;	
	
	return true;
}

/* Authenticates the actual Twitch User */
bool UTwitchIRCComponent::Authenticate()
{
	// make sure socket is connected
	if(this->TwitchIRCSocket == nullptr || !bSetupComplete)
		return false;

	bool bOAuth_OK = SendMessage("PASS " + this->OAuth);
	bool bNick_OK  = SendMessage("NICK " + this->BotUsername);

	bool bJoin_OK = false;
	if(this->ChannelName != "Channel_To_Join")
		bJoin_OK = SendMessage("JOIN #" + this->ChannelName);

	if(bOAuth_OK && bNick_OK && bJoin_OK)
		OnAuthentication.Broadcast();
	
	return (bOAuth_OK && bNick_OK && bJoin_OK);
}

/* Sends a message using IRC */
bool UTwitchIRCComponent::SendMessage(FString Message) const
{
	// if our socket is connected
	if(TwitchIRCSocket && TwitchIRCSocket->GetConnectionState() == SCS_Connected)
	{
		Message += "\n";
		TCHAR* SerializedChar = Message.GetCharArray().GetData();
		int32 BytesSent;
		return TwitchIRCSocket->Send((uint8*)TCHAR_TO_UTF8(SerializedChar), FCString::Strlen(SerializedChar), BytesSent);	
	}
	
	return false;
}

bool UTwitchIRCComponent::SendToChat(FString Message, FString Channel) 
{
	// if our socket is connected
	if(TwitchIRCSocket && TwitchIRCSocket->GetConnectionState() == SCS_Connected)
	{
		Message = "PRIVMSG #" + Channel + " :" + Message;
		
		Message += "\n";
		TCHAR* SerializedChar = Message.GetCharArray().GetData();
		int32 BytesSent;
		return TwitchIRCSocket->Send((uint8*)TCHAR_TO_UTF8(SerializedChar), FCString::Strlen(SerializedChar), BytesSent);	
	}
	
	return false;
}

/* Called on a Timer, collects the chat, passes data along to the Delegate Event OnMessageReceived */
void UTwitchIRCComponent::ReceiveData()
{
	if(TwitchIRCSocket == nullptr)
		return;

	TArray<uint8> MessageData;
	uint32  DataLength;
	bool bReceived = false;

	if(TwitchIRCSocket->HasPendingData(DataLength))
	{
		MessageData.SetNumUninitialized(DataLength); // Increase buffer for the data
		
		int32 BytesRead;
		TwitchIRCSocket->Recv(MessageData.GetData(), MessageData.Num(), BytesRead); // we can now officially receive the data
		
		bReceived = (BytesRead > 0); // did we actually read any bytes?

		if(bReceived)
			MessageData[BytesRead-1] = '\0'; // add the null terminator		
	}

	FString ReadableMessage = "";
	if(bReceived)
	{
		const std::string c_string_data(reinterpret_cast<char*>(MessageData.GetData()), MessageData.Num()); // Conversion from uint8 to char
		ReadableMessage = FString(c_string_data.c_str()); // Convert cstring to an FString
	}

	if(ReadableMessage != "")
	{
		TArray<FString> Users;
		TArray<FString> Parsed_Messages = ParseMessage(ReadableMessage, Users); // parse metadata out of the message
		
		for(int32 i = 0; i < Parsed_Messages.Num(); i++)
			OnMessageReceived.Broadcast(Parsed_Messages[i], Users[i]);
	}
}
/* Parses metadata out of the raw string */
TArray<FString> UTwitchIRCComponent::ParseMessage(FString ReadableMessage, TArray<FString>& out_Users)
{
	TArray<FString> ParsedMessages;
	TArray<FString> ReadableMessageInLines;
	ReadableMessage.ParseIntoArrayLines(ReadableMessageInLines); // THIS IS EXPENSIVE

	for(int32 i = 0; i < ReadableMessageInLines.Num(); i++)
	{
		// Twitch IRC sends this message periodically to make sure premature exits don't happen
		if(ReadableMessageInLines[i] == "PING :tmi.twitch.tv")
		{
			SendMessage("PONG :tmi.twitch.tv");
			continue;
		}

		// ALL TWITCH MESSAGES FOLLOW THIS FORMAT
		// ":twitch_username!twitch_username@twitch_username.tmi.twitch.tv PRIVMSG #channel :message here"

		TArray<FString> SplitMessage; // [0] will have the metadata, [1] will have the message
		ReadableMessageInLines[i].ParseIntoArray(SplitMessage, TEXT(":"));

		TArray<FString> MetaData;
		SplitMessage[0].ParseIntoArrayWS(MetaData);

		// MetaData looks like this
		// [0] :twitch_username!twitch_username@twitch_username.tmi.twitch.tv
		// [1] PRIVMSG
		// [2] #channel
		
		// handle collecting of usernames here
		FString User = "";
		if(MetaData[1] == "PRIVMSG")
			MetaData[0].Split("!", &User, nullptr); // collect the left side of the !

		// handle collecting of messages here
		if(SplitMessage.Num() > 1)
		{
			FString Content = SplitMessage[1];

			if(SplitMessage.Num() > 2) // someone decided to put :'s in their chat, now we have to put their message back together
				for(int j = 2; j < SplitMessage.Num(); j++)
					Content += ":" + SplitMessage[j];
			
			ParsedMessages.Add(Content);
			out_Users.Add(User);
		}
	}
	
	return ParsedMessages;
}



