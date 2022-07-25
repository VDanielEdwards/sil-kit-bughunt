/* Copyright (c) 2022 Vector Informatik GmbH

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */


#ifdef WIN32
#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable : 5105 4204)
#include "windows.h"
#define SleepMs(X) Sleep(X)
#else
#include <unistd.h>
#define SleepMs(X) usleep((X)*1000)
#endif
#define UNUSED_ARG(X) (void)(X)

#include "silkit/capi/SilKit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void AbortOnFailedAllocation(const char* failedAllocStrucName)
{
    fprintf(stderr, "Error: Allocation of \"%s\" failed, aborting...", failedAllocStrucName);
    abort();
}

char* LoadFile(char const* path)
{
    size_t length = 0;
    char* result = NULL;
    FILE* f = fopen(path, "rb");

    if (f)
    {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buffer = (char*)malloc((length + 1) * sizeof(char));
        if (buffer)
        {
            size_t num = fread(buffer, sizeof(uint8_t), length, f);
            if(num != length)
            {
                printf("Warning: short read on config file: %zu/%zu",
                    num, length);
                exit(1);
            }
            buffer[length] = '\0';
            result = buffer;
        }
        fclose(f);
    }
    return result;
}


SilKit_Participant* participant;
SilKit_RpcClient* client;
SilKit_RpcServer* server;

uint8_t callCounter = 0;

int receiveCallCount = 0;
const int numCalls = 10;

char* participantName;

uint8_t buffer[3];

void PrintByteVector(const SilKit_ByteVector* data)
{
    for (size_t i = 0; i < data->size; i++)
    {
        printf("%i", data->data[i]);
        if (i < data->size - 1)
        {
            printf(", ");
        }
    }
    printf("\n");
}

void CallHandler(void* context, SilKit_RpcServer* cbServer, const SilKit_RpcCallEvent* event)
{
    UNUSED_ARG(context);

    receiveCallCount += 1;
    uint8_t* tmp = (uint8_t*)malloc(event->argumentData.size * sizeof(uint8_t));
    if (tmp == NULL)
    {
        AbortOnFailedAllocation("SilKit_ByteVector");
        return;
    }
    printf("[Server] Call received: ");
    PrintByteVector(&event->argumentData);
    for (size_t i = 0; i < event->argumentData.size; i++)
    {
        tmp[i] = event->argumentData.data[i] + (uint8_t)100;
    }

    const SilKit_ByteVector returnData = {tmp, event->argumentData.size};
    SilKit_RpcServer_SubmitResult(cbServer, event->callHandle, &returnData);
    free(tmp);
}

void CallReturnHandler(void* context, SilKit_RpcClient* cbClient, const SilKit_RpcCallResultEvent* event)
{
    UNUSED_ARG(context);
    UNUSED_ARG(cbClient);

    if (event->callStatus == SilKit_CallStatus_SUCCESS)
    {
        printf("[client] Call returned: ");
        PrintByteVector(&event->resultData);
    }
    else
    {
        printf("[client] Call failed with error code %i\n", event->callStatus);
    }
}

void DiscoveryResultHandler(void* context, const SilKit_RpcDiscoveryResultList* discoveryResults)
{
    UNUSED_ARG(context);

    for (uint32_t i = 0; i < discoveryResults->numResults; i++)
    {
        printf("Discovered RpcServer with functionName=\"%s\", mediaType=\"%s\", labels={",
               discoveryResults->results[i].functionName, discoveryResults->results[i].mediaType);
        for (uint32_t j = 0; j < discoveryResults->results[i].labelList->numLabels; j++)
        {
            printf("{\"%s\", \"%s\"}", discoveryResults->results[i].labelList->labels[j].key, discoveryResults->results[i].labelList->labels[j].value);
        }
        printf("}\n");
    }
}

void Copy_Label(SilKit_KeyValuePair* dst, const SilKit_KeyValuePair* src)
{
    dst->key = malloc(strlen(src->key) + 1);
    dst->value = malloc(strlen(src->value) + 1);
    if (dst->key != NULL && dst->value != NULL)
    {
        strcpy((char*)dst->key, src->key);
        strcpy((char*)dst->value, src->value);
    }
}

void Create_Labels(SilKit_KeyValueList** outLabelList, const SilKit_KeyValuePair* labels, size_t numLabels)
{
    SilKit_KeyValueList* newLabelList;
    newLabelList = (SilKit_KeyValueList*)malloc(sizeof(SilKit_KeyValueList));
    if (newLabelList == NULL)
    {
        AbortOnFailedAllocation("SilKit_KeyValueList");
        return;
    }
    newLabelList->numLabels = numLabels;
    newLabelList->labels = (SilKit_KeyValuePair*)malloc(numLabels * sizeof(SilKit_KeyValuePair));
    if (newLabelList->labels == NULL)
    {
        AbortOnFailedAllocation("SilKit_KeyValuePair");
        return;
    }
    for (size_t i = 0; i < numLabels; i++)
    {
        Copy_Label(&newLabelList->labels[i], &labels[i]);
    }
    *outLabelList = newLabelList;
}

void Labels_Destroy(SilKit_KeyValueList* labelList)
{
    if (labelList)
    {
        for (size_t i = 0; i < labelList->numLabels; i++)
        {
            free((char*)labelList->labels[i].key);
            free((char*)labelList->labels[i].value);
        }
        free(labelList);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("usage: SilKitDemoCData <ConfigJsonFile> <ParticipantName> [RegistryUri]\n");
        return 1;
    }

    char* jsonString = LoadFile(argv[1]);
    if (jsonString == NULL)
    {
        printf("Error: cannot open config file %s\n", argv[1]);
        return 1;
    }
    participantName = argv[2];

    const char* registryUri = "silkit://localhost:8500";
    if (argc >= 4)
    {
        registryUri = argv[3];
    }

    SilKit_ReturnCode returnCode;
    returnCode = SilKit_Participant_Create(&participant, jsonString, participantName, registryUri, SilKit_False);
    if (returnCode) {
        printf("%s\n", SilKit_GetLastErrorString());
        return 2;
    }
    printf("Creating participant '%s' for simulation '%s'\n", participantName, registryUri);

    if (strcmp(participantName, "Client") == 0)
    {
        const char* filterFunctionName = "";
        const char* filterMediaType = "";
        SilKit_KeyValueList* filterLabelList;
        size_t numLabels = 1;
        SilKit_KeyValuePair filterLabels[1] = {{"KeyA", "ValA"}};
        Create_Labels(&filterLabelList, filterLabels, numLabels);

        returnCode = SilKit_DiscoverServers(participant, filterFunctionName, filterMediaType, filterLabelList, NULL,
                                            &DiscoveryResultHandler);

        const char* mediaType = "A";
        SilKit_KeyValueList* labelList;
        numLabels = 1;
        SilKit_KeyValuePair labels[1] = { {"KeyA", "ValA"} };
        Create_Labels(&labelList, labels, numLabels);

        returnCode = SilKit_RpcClient_Create(&client, participant, "ClientCtrl1", "TestFunc", mediaType, labelList, NULL,
                                          &CallReturnHandler);

        for (uint8_t i = 0; i < numCalls; i++)
        {
            SleepMs(1000);
            buffer[0] = i;
            buffer[1] = i;
            buffer[2] = i;
            SilKit_ByteVector argumentData = { &buffer[0], 3 };
            printf("[Client] Call dispatched: ");
            PrintByteVector(&argumentData);
            SilKit_RpcCallHandle* callHandle;
            SilKit_RpcClient_Call(client, &callHandle, &argumentData);
        }
    }
    else if (strcmp(participantName, "Server") == 0)
    {
        const char* mediaType = "A";
        SilKit_KeyValueList* labelList;
        size_t numLabels = 2;
        SilKit_KeyValuePair labels[2] = {{"KeyA", "ValA"}, {"KeyB", "ValB"}};
        Create_Labels(&labelList, labels, numLabels);

        returnCode = SilKit_RpcServer_Create(&server, participant, "ServerCtrl1", "TestFunc", mediaType, labelList, NULL,
                                          &CallHandler);

        while (receiveCallCount < numCalls)
        {
            SleepMs(100);
        }
    }

    SilKit_Participant_Destroy(participant);
    if (jsonString)
    {
        free(jsonString);
    }

    return EXIT_SUCCESS;
}
