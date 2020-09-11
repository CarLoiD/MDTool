#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifdef _DEBUG
#define DEBUG_IN_FILE "CHIHUAHUA.MD"
#define DEBUG_OUT_FILE "CHIHUAHUA.h"
#endif

#define STRIP_FLAG_SKIP (0x00008000)
#define HEX_LINE_OFFSET (0x00000010)

typedef struct {
    uint32_t SubmeshAmount;
} MDInfoHeader;

typedef struct {
    uint32_t SubmeshType;
    uint16_t SubmeshId;
    uint16_t SubdivisionAmount;
} MDBInfoHeader;

typedef struct {
    uint32_t Reserved[4];
    uint16_t VertexCount;
} SubdivisionInfoHeader;

typedef struct {
    float X;
    float Y;
    float Z;
    uint32_t StripFlag;
} VertexPosition;

typedef struct {
    float u;
    float v;
} VertexUV;

typedef enum {
    EXPORT_VERTEXLIST,
    EXPORT_OBJ
} ExportOption;

typedef enum {
    MD_OK = 0,
    MD_SHOW_INSTRUCTIONS,
    MD_INVALID_ARGC,
    MD_INVALID_ARGUMENT,
    MD_WRONG_SIGNATURE,
    MD_FAILURE
} StatusCode;

void PrintInstructions();
void FreeMDImportData(FILE* stream, VertexPosition* posBuffer, VertexUV* uvBuffer);

int32_t ParseCommandLineArg(int argc, char** argv, char* mdFilePath,
                            char* outputPath, ExportOption* exportOption);

int32_t ExportMD(FILE* stream, const char* path, const ExportOption option);

int main(int argc, char* argv[])
{
    char mdFilePath[256];
    char outputPath[256];
    ExportOption exportOption;

    int32_t ret = ParseCommandLineArg(argc, argv, mdFilePath, outputPath, &exportOption);
    if (ret != MD_OK) {
        return ret;
    }

    FILE* streamInput = NULL;

#ifdef _DEBUG
    fopen_s(&streamInput, DEBUG_IN_FILE, "rb");
#else
    fopen_s(&streamInput, mdFilePath, "rb");
#endif

    if (streamInput == NULL) 
    {
        printf("Error: Failed to open the specified MD file\n");
        return -1;
    }

#ifdef _DEBUG
    ret = ExportMD(streamInput, DEBUG_OUT_FILE, exportOption);
#else
    ret = ExportMD(streamInput, outputPath, exportOption);
#endif

    if (ret != MD_OK) 
    {
        fclose(streamInput);
        return ret;
    }

    // All goes well, free the file stream
    fclose(streamInput);

    return 0;
}

void PrintInstructions()
{
    printf("%s", "----------------------------------\n");
    printf("%s", "- GODHAND MD Tool by CarLoiD\n");
    printf("%s", "----------------------------------\n\n");

    printf("%s", "Usage: MDTool [OPTION] MDFilePath OutFilePath\n");
    printf("%s", "OPTION:\n");
    printf("%s", "\t--help          -> Display this information\n");
    printf("%s", "\t--vertexlist    -> Export the MD as vertex list (PSP format)\n");
    printf("%s", "\t--obj           -> Export the MD as wavefront OBJ\n");
}

int32_t ParseCommandLineArg(const int argc, char** argv, char* mdFilePath,
                            char* outputPath, ExportOption* exportOption)
{
#ifndef _DEBUG
    if (strcmp(argv[1], "--help") == 0) {
        PrintInstructions();
        return MD_SHOW_INSTRUCTIONS;
    }
    
    // option mdpath outpath
    if (argc != 3) 
    {
        printf("Error: Invalid argument count\n\n");
        PrintInstructions();

        return MD_INVALID_ARGC;
    }

    if (strcmp(argv[1], "--vertexlist") == 0) {
        *exportOption = EXPORT_VERTEXLIST;
    } else if (strcmp(argv[1], "--obj") == 0) {
        *exportOption = EXPORT_OBJ;
    }

    mdFilePath = argv[2];
    outputPath = argv[3];
#endif

    return MD_OK;
}

void FreeMDImportData(FILE* stream, VertexPosition* posBuffer, VertexUV* uvBuffer)
{
    fclose(stream);

    if (posBuffer != NULL) {
        free(posBuffer);
    }

    if (uvBuffer != NULL) {
        free(uvBuffer);
    }
}

inline int32_t IsValidHeaderSignature(FILE* stream)
{
    char signature[4];
    
    fread(signature, sizeof(char), sizeof(signature) - 1, stream);
    signature[sizeof(signature) - 1] = '\0';

    return strcmp(signature, "scr") == 0;
}

inline void SeekForHeader(FILE* stream, const char* header, const uint32_t size, uint64_t* actualFilePtr)
{
    char* buffer = (char*)malloc(size);
    const uint32_t sizeMinusOne = size - 1;

    while (1)
    {
        fread(buffer, sizeof(char), sizeMinusOne, stream);
        buffer[sizeMinusOne] = '\0';
        
        if (strcmp(buffer, header) == 0) {
            break;
        }

        *actualFilePtr = ftell(stream) - sizeMinusOne + HEX_LINE_OFFSET;
        fseek(stream, *actualFilePtr, SEEK_SET);
    }

    free(buffer);
}

inline void SeekForSubdvisionHeader(FILE* stream, uint64_t* actualFilePtr)
{
    while (1)
    {
        uint32_t uintBuffer;
        fread(&uintBuffer, sizeof(uint32_t), 1, stream);

        if (uintBuffer == 0x20) {
            break;
        }

        *actualFilePtr = ftell(stream) - sizeof(uint32_t) + HEX_LINE_OFFSET;
        fseek(stream, *actualFilePtr, SEEK_SET);
    }
}

inline void ReadVertexPositionBuffer(FILE* stream, VertexPosition* vertexPosBuffer,
                                     const uint32_t size, uint64_t* actualFilePtr)
{
    for (uint32_t index = 0; index < size; ++index) {
        fread(&vertexPosBuffer[index], sizeof(VertexPosition), 1, stream);
        *actualFilePtr += sizeof(VertexPosition);
    }
}

inline void ReadVertexUVBuffer(FILE* stream, VertexUV* vertexUVBuffer,
                               const uint32_t size, uint64_t* actualFilePtr)
{
    *actualFilePtr = ftell(stream) + (2 * sizeof(uint16_t)) * size;
    fseek(stream, *actualFilePtr, SEEK_SET);

    while (*actualFilePtr & HEX_LINE_OFFSET != 0)
    {
        fseek(stream, 1, SEEK_CUR);
        *actualFilePtr++;
    }

    for (uint32_t index = 0; index < size; ++index)
    {
        fread(&vertexUVBuffer[index], sizeof(VertexUV), 1, stream);

        vertexUVBuffer[index].u = ((float)vertexUVBuffer[index].u / 32767.0f) * 8.0f;
        vertexUVBuffer[index].v = 1.0f - ((float)vertexUVBuffer[index].v / 32767.0f) * 8.0f;
    }
}

int32_t ExportMD(FILE* stream, const char* path, const ExportOption option)
{
    // Validate MD header

    if (!IsValidHeaderSignature(stream))
    {
        printf("Error: Failed to import the MD file, wrong header signature\n");
        return MD_WRONG_SIGNATURE;
    }

    fseek(stream, 0x08, SEEK_SET);

    MDInfoHeader mdInfoHeader;
    fread(&mdInfoHeader, sizeof(MDInfoHeader), 1, stream);

    uint64_t actualFilePtr = HEX_LINE_OFFSET;
    fseek(stream, HEX_LINE_OFFSET, SEEK_SET);

    for (uint32_t index = 0; index < mdInfoHeader.SubmeshAmount; ++index)
    {
        const char* strMDBHeader = "mdb";
        SeekForHeader(stream, strMDBHeader, sizeof(strMDBHeader), &actualFilePtr);

        actualFilePtr = ftell(stream) + 1;
        fseek(stream, actualFilePtr, SEEK_SET);

        // Gather mdb header data

        MDBInfoHeader mdbInfoHeader;
        fread(&mdbInfoHeader, sizeof(MDBInfoHeader), 1, stream);
        
        // Go to the next 16 byte aligned data
        actualFilePtr = ftell(stream) + 4;
        fseek(stream, actualFilePtr, SEEK_SET);
        
        for (uint32_t nIndex = 0; nIndex < mdbInfoHeader.SubdivisionAmount; ++nIndex)
        {
            SeekForSubdvisionHeader(stream, &actualFilePtr);

            SubdivisionInfoHeader subdivisionInfoHeader;
            fread(&subdivisionInfoHeader, sizeof(SubdivisionInfoHeader), 1, stream);

            actualFilePtr = ftell(stream) + 2 * sizeof(uint32_t);
            fseek(stream, actualFilePtr, SEEK_SET);
        
            const uint32_t bufferSize = subdivisionInfoHeader.VertexCount;

            VertexPosition* vertexPosBuffer = (VertexPosition*)malloc(bufferSize);
            VertexUV* vertexUVBuffer = (VertexUV*)malloc(bufferSize);

            ReadVertexPositionBuffer(stream, vertexPosBuffer, bufferSize, &actualFilePtr);
            ReadVertexUVBuffer(stream, vertexUVBuffer, bufferSize, &actualFilePtr);

            free(vertexUVBuffer);
            free(vertexPosBuffer);
        }
    }

    return MD_FAILURE;
}