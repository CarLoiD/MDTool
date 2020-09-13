#define _CRT_SECURE_NO_WARNINGS

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
    MD_OK,
    MD_SHOW_INSTRUCTIONS,
    MD_INVALID_ARGC,
    MD_INVALID_ARGUMENT,
    MD_WRONG_SIGNATURE,
    MD_FAILURE
} StatusCode;

void PrintInstructions();

int32_t ParseCommandLineArgs(int argc, char** argv, char* mdFilePath, char* outputPath);
int32_t ExportMD(FILE* stream, const char* path);

int main(int argc, char* argv[])
{
    char mdFilePath[256];
    char outputPath[256];

    int32_t ret = ParseCommandLineArgs(argc, argv, mdFilePath, outputPath);
    if (ret != MD_OK) {
        return ret;
    }

    FILE* inputStream = NULL;

#ifdef _DEBUG
    fopen_s(&inputStream, DEBUG_IN_FILE, "rb");
#else
    fopen_s(&inputStream, mdFilePath, "rb");
#endif

    if (inputStream == NULL) 
    {
        printf("Error: Failed to open the specified MD file\n");
        return -1;
    }

#ifdef _DEBUG
    ret = ExportMD(inputStream, DEBUG_OUT_FILE);
#else
    ret = ExportMD(inputStream, outputPath);
#endif

    if (ret != MD_OK) 
    {
        fclose(inputStream);
        return ret;
    }

    // If all goes well, free the file input stream
    fclose(inputStream);

    return 0;
}

void PrintInstructions()
{
    printf("%s", "----------------------------------\n");
    printf("%s", "- GODHAND MD Tool by CarLoiD      \n");
    printf("%s", "----------------------------------\n\n");

    printf("%s", "Usage: MDTool MDFilePath OutFilePath\n");
}

int32_t ParseCommandLineArgs(const int argc, char** argv, char* mdFilePath, char* outputPath)
{
#ifndef _DEBUG
    if (argc == 2)
    {
        if (strcmp(argv[1], "--help") == 0) {
            PrintInstructions();
            return MD_SHOW_INSTRUCTIONS;
        }
    }
    
    // Other than help, it always needs three args (programPath, mdPath and outPath)
    if (argc != 3) 
    {
        printf("Error: Invalid argument count\n\n");
        PrintInstructions();

        return MD_INVALID_ARGC;
    }

    strcpy_s(mdFilePath, 256, argv[1]);
    strcpy_s(outputPath, 256, argv[2]);
#endif

    return MD_OK;
}

inline int32_t IsValidHeaderSignature(FILE* stream)
{
    char signature[4];

    fread(signature, sizeof(char), sizeof(signature) - 1, stream);
    signature[sizeof(signature) - 1] = '\0';

    return strcmp(signature, "scr") == 0;
}

inline void SeekForHeader(FILE* stream, const char* header, const uint32_t size)
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

        // Reset the file pointer cursor and jump to the next line
        fseek(stream, ftell(stream) - sizeMinusOne + HEX_LINE_OFFSET, SEEK_SET);
    }

    free(buffer);
}

inline void SeekForSubdvisionHeader(FILE* stream, const int32_t align16)
{
    if (align16)
    {
        while ((ftell(stream) & HEX_LINE_OFFSET) != 0) {
            fseek(stream, 1, SEEK_CUR);
        }
    }

    while (1)
    {
        uint32_t uintBuffer;
        fread(&uintBuffer, sizeof(uint32_t), 1, stream);

        // For some unknown reason subdvision header always starts with a 0x20
        if (uintBuffer == 0x20) {
            break;
        }

        fseek(stream, ftell(stream) - sizeof(uint32_t) + HEX_LINE_OFFSET, SEEK_SET);
    }
}

inline void ReadVertexPositionBuffer(FILE* stream, VertexPosition* vertexPosBuffer, const uint32_t size)
{
    fread(vertexPosBuffer, sizeof(VertexPosition), size, stream);
}

inline void ReadVertexUVBuffer(FILE* stream, VertexUV* vertexUVBuffer, const uint32_t size)
{
    fseek(stream, ftell(stream) + (2 * sizeof(uint16_t)) * size, SEEK_SET);

    while ((ftell(stream) & HEX_LINE_OFFSET) != 0) {
        fseek(stream, 1, SEEK_CUR);
    }

    for (uint32_t index = 0; index < size; ++index)
    {
        uint16_t tmpU, tmpV;

		fread(&tmpU, sizeof(uint16_t), 1, stream);
		fread(&tmpV, sizeof(uint16_t), 1, stream);

        vertexUVBuffer[index].u = ((float)tmpU / 32767.0f) * 8.0f;
        vertexUVBuffer[index].v = 1.0f - ((float)tmpV / 32767.0f) * 8.0f;
    }
}

inline void StringGetFirstNotOf(const char* source, const char element, char* dest)
{
    const uint32_t sourceLenght = strlen(source);
    uint32_t elementIndex = 0;

    for (uint32_t index = 0; index < sourceLenght; ++index) 
    {
        if (source[index] == element) 
        {
            elementIndex = index;
            break;
        }
    }

    if (elementIndex > 0) {
        strncpy(dest, source, elementIndex);
        dest[elementIndex] = '\0';
    }
}

inline void WriteVertexInputToFile(FILE* stream, const uint32_t count, const VertexPosition position,
                                   const VertexUV uv)
{
    for (uint32_t index = 0; index < count; ++index)
    {
        fprintf(stream, "\t{ { %0.5ff, %0.5ff }, ", uv.u, uv.v);
        fprintf(stream, "0xFFFFFFFF, ");
        fprintf(stream, "{ %0.5ff, %0.5ff, %0.5ff } },\n", position.X, position.Y, position.Z);
    }
}

inline void WriteBuffersToVertexList(FILE* stream, const VertexPosition* vertexPosBuffer,
                                     const VertexUV* vertexUVBuffer, const uint32_t size)
{
    for (uint32_t index = 0; index < size; ++index)
    {
        // When the strip flag is in skip mode, degenerate the triangle
        if (vertexPosBuffer[index].StripFlag == STRIP_FLAG_SKIP)
            WriteVertexInputToFile(stream, 1, vertexPosBuffer[index], vertexUVBuffer[index]);
        else
            WriteVertexInputToFile(stream, 1, vertexPosBuffer[index], vertexUVBuffer[index]);
    }
}

inline void WriteIndices(FILE* stream, const uint32_t size)
{
    for (uint32_t index = 0; index < (size - 2); ++index) {
        fprintf(stream, "\t%d, %d, %d,\n", index, index + 1, index + 2);
    }
}

int32_t ExportMD(FILE* stream, const char* path)
{
    FILE* outputStream = NULL;
    fopen_s(&outputStream, path, "w");

    // Validate MD header

    if (!IsValidHeaderSignature(stream))
    {
        printf("Error: Failed to import the MD file, wrong header signature\n");
        fclose(outputStream);

        return MD_WRONG_SIGNATURE;
    }

    fprintf(outputStream, "%s\n", "typedef struct {");
    fprintf(outputStream, "%s\n", "    float TexCoord[2];");
    fprintf(outputStream, "%s\n", "    uint32_t DiffuseColor;");
    fprintf(outputStream, "%s\n", "    float Position[3];");
    fprintf(outputStream, "%s\n", "} VertexInput;\n");

    MDInfoHeader mdInfoHeader;
    fseek(stream, 0x08, SEEK_SET);
    fread(&mdInfoHeader, sizeof(MDInfoHeader), 1, stream);
    fseek(stream, HEX_LINE_OFFSET, SEEK_SET);

    for (uint32_t index = 0; index < mdInfoHeader.SubmeshAmount; ++index)
    {
        const char* strMDBHeader = "mdb";
        SeekForHeader(stream, strMDBHeader, sizeof(strMDBHeader));
        fseek(stream, 1, SEEK_CUR);

        // Gather mdb header data

        MDBInfoHeader mdbInfoHeader;
        fread(&mdbInfoHeader, sizeof(MDBInfoHeader), 1, stream);
        
        // Go to the next 16 byte aligned data
        fseek(stream, 4, SEEK_CUR);
        
        for (uint32_t submeshIndex = 0; submeshIndex < mdbInfoHeader.SubdivisionAmount; ++submeshIndex)
        {
            const uint32_t filePtr = ftell(stream);

            if (submeshIndex > 0)
                SeekForSubdvisionHeader(stream, 1);
            else
                SeekForSubdvisionHeader(stream, 0);

            SubdivisionInfoHeader subdivisionInfoHeader;
            fread(&subdivisionInfoHeader, sizeof(SubdivisionInfoHeader), 1, stream);
            fseek(stream, ftell(stream) + 2 * sizeof(uint32_t), SEEK_SET);
        
            const uint32_t bufferSize = subdivisionInfoHeader.VertexCount;

            VertexPosition* vertexPosBuffer = (VertexPosition*)malloc(bufferSize * sizeof(VertexPosition));
            VertexUV* vertexUVBuffer = (VertexUV*)malloc(bufferSize * sizeof(VertexUV));

            ReadVertexPositionBuffer(stream, vertexPosBuffer, bufferSize);
            ReadVertexUVBuffer(stream, vertexUVBuffer, bufferSize);

            char vertexListName[256];
            StringGetFirstNotOf(path, '.', vertexListName);

            char vertexListId[7];
            sprintf(vertexListId, "_%02d_%02d", index + 1, submeshIndex + 1);
            strcat(vertexListName, vertexListId);

            fprintf(outputStream, "%s%s%s\n", "const VertexInput ALIGN(16) ", vertexListName, "[] = \n{");
            WriteBuffersToVertexList(outputStream, vertexPosBuffer, vertexUVBuffer, bufferSize);
            fprintf(outputStream, "};\n\n");

            fprintf(outputStream, "%s%s%s\n", "const uint16_t ", vertexListName, "Indices[] = \n{");
            WriteIndices(outputStream, bufferSize);
            fprintf(outputStream, "%s%s%s", "}; const uint32_t ", vertexListName, "IndexCount = sizeof(");
            fprintf(outputStream, "%s%s\n", vertexListName, "Indices) / sizeof(uint16_t);\n");

            free(vertexUVBuffer);
            free(vertexPosBuffer);
        }
    }

    fclose(outputStream);

#ifdef _DEBUG
    printf("Done...\n");
#endif

    return MD_OK;
}