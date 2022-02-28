// NetworkClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <iomanip>

#include <initguid.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <Functiondiscoverykeys_devpkey.h>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "4444"



#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }
// code sample retrieved from https://docs.microsoft.com/en-us/answers/questions/214055/is-it-possible-to-get-status-or-event-when-device.html
// Documentation at 
// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
// https://docs.microsoft.com/en-us/windows/win32/api/mmdeviceapi/nf-mmdeviceapi-immdeviceenumerator-enumaudioendpoints
// https://docs.microsoft.com/en-us/windows/win32/coreaudio/device-properties
// can be found to be helpful when locating libraries to include.
BOOL IsMicrophoneRecording()
{
    // #1 Get the audio endpoint associated with the microphone device
    HRESULT hr = S_OK;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IAudioSessionManager2* pSessionManager = NULL;
    BOOL result = FALSE;

    CoInitialize(0);

    // Create the device enumerator.
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&pEnumerator);

    IMMDeviceCollection* dCol = NULL;
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &dCol);
    UINT dCount;
    hr = dCol->GetCount(&dCount);
    for (UINT i = 0; i < dCount; i++)
    {
        IMMDevice* pCaptureDevice = NULL;
        hr = dCol->Item(i, &pCaptureDevice);

        IPropertyStore* pProps = NULL;
        hr = pCaptureDevice->OpenPropertyStore(
            STGM_READ, &pProps);

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(
            PKEY_Device_FriendlyName, &varName);

        std::wstring nameStr(varName.pwszVal);

        // #2 Determine whether it is the microphone device you are focusing on
        // TODO: FIXME: Set NVIDIA to another string to find a different device.
        // TODO: Make this selectable or interactive for the user.
        std::size_t found = nameStr.find(L"NVIDIA");
        if (found != std::string::npos)
        {
            // Print endpoint friendly name.
            printf("Endpoint friendly name: \"%S\"\n", varName.pwszVal);

            // Get the session manager.
            hr = pCaptureDevice->Activate(
                __uuidof(IAudioSessionManager2), CLSCTX_ALL,
                NULL, (void**)&pSessionManager);
            break;
        }
    }
    // Get session state
    if (!pSessionManager)
    {
        return (result = FALSE);
    }

    int cbSessionCount = 0;
    LPWSTR pswSession = NULL;

    IAudioSessionEnumerator* pSessionList = NULL;
    IAudioSessionControl* pSessionControl = NULL;
    IAudioSessionControl2* pSessionControl2 = NULL;

    // Get the current list of sessions.
    hr = pSessionManager->GetSessionEnumerator(&pSessionList);

    // Get the session count.
    hr = pSessionList->GetCount(&cbSessionCount);
    //wprintf_s(L"Session count: %d\n", cbSessionCount);

    for (int index = 0; index < cbSessionCount; index++)
    {
        CoTaskMemFree(pswSession);
        SAFE_RELEASE(pSessionControl);

        // Get the <n>th session.
        hr = pSessionList->GetSession(index, &pSessionControl);

        hr = pSessionControl->QueryInterface(
            __uuidof(IAudioSessionControl2), (void**)&pSessionControl2);

        // Exclude system sound session
        hr = pSessionControl2->IsSystemSoundsSession();
        if (S_OK == hr)
        {
            //wprintf_s(L" this is a system sound.\n");
            continue;
        }

        // Optional. Determine which application is using Microphone for recording
        LPWSTR instId = NULL;
        hr = pSessionControl2->GetSessionInstanceIdentifier(&instId);
        if (S_OK == hr)
        {
            wprintf_s(L"SessionInstanceIdentifier: %s\n", instId);
        }

        AudioSessionState state;
        hr = pSessionControl->GetState(&state);
        switch (state)
        {
        case AudioSessionStateInactive:
            wprintf_s(L"Session state: Inactive\n", state);
            break;
        case AudioSessionStateActive:
            // #3 Active state indicates it is recording, otherwise is not recording.
            wprintf_s(L"Session state: Active\n", state);
            result = TRUE;
            break;
        case AudioSessionStateExpired:
            wprintf_s(L"Session state: Expired\n", state);
            break;
        }
    }

done:
    // Clean up.
    SAFE_RELEASE(pSessionControl);
    SAFE_RELEASE(pSessionList);
    SAFE_RELEASE(pSessionControl2);
    SAFE_RELEASE(pSessionManager);
    SAFE_RELEASE(pEnumerator);

    return result;
}

int sendCommand(const char* myCommand)
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;
    const char* myAddr = "192.168.29.14";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;


    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(myAddr, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);

        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }
    // Send an initial buffer
    iResult = send(ConnectSocket, myCommand, (int)strlen(myCommand), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }
    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    printf("Bytes Sent: %ld\n", iResult);
    



    // Receive until the peer closes the connection
    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0)
            printf("Bytes received: %d\n", iResult);
        else if (iResult == 0)
            printf("Connection closed\n");
        else
            printf("recv failed with error: %d\n", WSAGetLastError());

    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();
}

std::ostream& field(std::ostream& o)
{
    // usually the console is 80-character wide.
    // divide the line into four fields.
    return o << std::setw(20) << std::right;
}

int __cdecl main(int argc, char** argv)
{
    bool lastState = FALSE;
    const char* sendEnable = "1";
    const char* sendDisable = "0";


    /* Validate the parameters
    if (argc != 2) {
        printf("usage: %s server-name\n", argv[0]);
        return 1;
    }*/


    std::cout << "Connected to server!\n";
    std::cout << "Welcome, detecting audio streams..." << std::endl;
    Sleep(1000);
    // Begin main program loop
    while (true) {
        if (IsMicrophoneRecording() && lastState == FALSE) {
            std::string n = "Microphone is recording.";
            size_t fieldWidth = n.size();

            std::cout << field << n << std::endl;

            //std::cout << "Microphone is recording.\n";
            sendCommand(sendEnable);
            lastState = TRUE;

        }
        else if (!IsMicrophoneRecording() && lastState == TRUE) {
            //std::cout << "Microphone is not recording.\n";
  
            sendCommand(sendDisable);
            lastState = FALSE;
        }
        if (lastState) {
            std::string n = "Microphone is recording.";

            std::cout << "+----------------------+" << std::endl
                << "|                      |" << std::endl
                << "|                      |" << std::endl
                << "|        ON AIR        |" << std::endl
                << "|                      |" << std::endl
                << "|                      |" << std::endl
                << "+----------------------+" << std::endl;
            size_t fieldWidth = n.size();

            std::cout << field << n << std::endl;
        }
        else if (!lastState) {
            std::string n = "Microphone is not recording.";
            std::cout << "+----------------------+" << std::endl
                << "|                      |" << std::endl
                << "|                      |" << std::endl
                << "|       OFF AIR        |" << std::endl
                << "|                      |" << std::endl
                << "|                      |" << std::endl
                << "+----------------------+" << std::endl;
            size_t fieldWidth = n.size();

            std::cout << field << n << std::endl;
        }
        Sleep(500); // sleep for half a second, close to realtime detection.
        system("CLS");
    }   
    return 0;
}
