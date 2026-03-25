#include "data/data_win.h"
#include "data/txtr_pack_format.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

typedef enum
{
    UNKNOWN,
    IMG_PNG,
    IMG_JPEG,
    IMG_BMP,
    IMG_GIF,
    IMG_WEBP,
    IMG_DDS
} ImageFormat;

typedef struct
{
    uint8_t *rgba;
    int width;
    int height;
} DecodedTexture;

static ImageFormat detectImageFormat(const uint8_t *data, size_t size)
{
    if (data == NULL || size < 4)
    {
        return UNKNOWN;
    }

    if (size >= 8 && memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0)
    {
        return IMG_PNG;
    }

    if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
    {
        return IMG_JPEG;
    }

    if (size >= 2 && memcmp(data, "BM", 2) == 0)
    {
        return IMG_BMP;
    }

    if (size >= 6 && memcmp(data, "GIF87a", 6) == 0)
    {
        return IMG_GIF;
    }

    if (size >= 6 && memcmp(data, "GIF89a", 6) == 0)
    {
        return IMG_GIF;
    }

    if (size >= 4 && memcmp(data, "RIFF", 4) == 0 && size >= 12 && memcmp(data + 8, "WEBP", 4) == 0)
    {
        return IMG_WEBP;
    }

    if (size >= 4 && memcmp(data, "DDS ", 4) == 0)
    {
        return IMG_DDS;
    }

    return UNKNOWN;
}

// Returns the offset of the first valid image signature (PNG or JPEG)
static size_t findImageSignatureOffset(const uint8_t *data, size_t size)
{
    if (data == NULL || size < 4)
    {
        return 0;
    }

    if (size >= 8 && memcmp(data, "\x89PNG\r\n\x1A\n", 8) == 0)
    {
        return 0;
    }

    if (size >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
    {
        return 0;
    }

    for (size_t i = 1; (i + 8) <= size; i++)
    {
        if (memcmp(data + i, "\x89PNG\r\n\x1A\n", 8) == 0)
        {
            return i;
        }

        if (data[i + 0] == 0xFF && data[i + 1] == 0xD8 && data[i + 2] == 0xFF)
        {
            return i;
        }
    }

    return 0;
}

static void dumpHeader(const uint8_t *data, size_t size)
{
    size_t n = size < 16 ? size : 16;
    fprintf(stderr, "Header bytes:");
    for (size_t i = 0; i < n; i++)
    {
        fprintf(stderr, " %02X", data[i]);
    }

    fprintf(stderr, "\n");
}

static void logDecodeFailure(const char *stage, int pageId, const Texture *texture, size_t sigOffset)
{
    const char *reason = stbi_failure_reason();
    const uint8_t *ptr = texture->blobData + sigOffset;
    size_t size = texture->blobSize - sigOffset;

    ImageFormat format = detectImageFormat(ptr, size);

    fprintf(
        stderr,
        "txtr_cooker: %s failed for texture %d (blobOffset=%u blobSize=%u sigOffset=%zu reason=%s detectedFormat=%d)\n",
        stage,
        pageId,
        texture != NULL ? texture->blobOffset : 0u,
        texture != NULL ? texture->blobSize : 0u,
        sigOffset,
        reason != NULL ? reason : "unknown",
        format);

    dumpHeader(ptr, size);
}

static bool decodeTexture(const Texture *texture, int pageId, DecodedTexture *outTexture)
{
    memset(outTexture, 0, sizeof(*outTexture));

    if (texture == NULL || texture->blobData == NULL || texture->blobSize == 0)
    {
        return false;
    }

    size_t sigOffset = findImageSignatureOffset(texture->blobData, (size_t)texture->blobSize);
    size_t decodeSize = (size_t)texture->blobSize - sigOffset;
    if (decodeSize == 0 || decodeSize > (size_t)INT_MAX)
    {
        fprintf(stderr,
                "txtr_cooker: texture %d has invalid decode size (blobSize=%u sigOffset=%zu decodeSize=%zu)\n",
                pageId,
                texture->blobSize,
                sigOffset,
                decodeSize);
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    uint8_t *rgba = stbi_load_from_memory(texture->blobData + sigOffset,
                                          (int)decodeSize,
                                          &width,
                                          &height,
                                          &channels,
                                          4);
    (void)channels;
    if (rgba == NULL || width <= 0 || height <= 0)
    {
        if (rgba != NULL)
        {
            stbi_image_free(rgba);
        }
        logDecodeFailure("stbi_load_from_memory", pageId, texture, sigOffset);
        return false;
    }

    outTexture->rgba = rgba;
    outTexture->width = width;
    outTexture->height = height;
    return true;
}

static bool writeTexturePack(const char *outputPath, DataWin *dataWin)
{
    uint32_t count = dataWin->txtr.count;
    size_t entryBytes = (size_t)count * (size_t)TXTR_PACK_ENTRY_SIZE;
    uint8_t *entries = entryBytes > 0 ? calloc(1, entryBytes) : NULL;
    if (entryBytes > 0 && entries == NULL)
    {
        fprintf(stderr, "txtr_cooker: failed to allocate entry table\n");
        return false;
    }

    FILE *out = fopen(outputPath, "wb");
    if (out == NULL)
    {
        fprintf(stderr, "txtr_cooker: failed to open output '%s'\n", outputPath);
        free(entries);
        return false;
    }

    uint8_t header[TXTR_PACK_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, TXTR_PACK_MAGIC, TXTR_PACK_MAGIC_SIZE);
    TxtrPack_writeU32LE(header + 8, TXTR_PACK_VERSION);
    TxtrPack_writeU32LE(header + 12, count);
    TxtrPack_writeU32LE(header + 16, 0u);

    if (fwrite(header, 1, sizeof(header), out) != sizeof(header))
    {
        fprintf(stderr, "txtr_cooker: failed to write pack header\n");
        fclose(out);
        free(entries);
        remove(outputPath);
        return false;
    }

    uint8_t zeroEntry[TXTR_PACK_ENTRY_SIZE];
    memset(zeroEntry, 0, sizeof(zeroEntry));
    for (uint32_t i = 0; i < count; i++)
    {
        if (fwrite(zeroEntry, 1, sizeof(zeroEntry), out) != sizeof(zeroEntry))
        {
            fprintf(stderr, "txtr_cooker: failed to reserve entry table\n");
            fclose(out);
            free(entries);
            remove(outputPath);
            return false;
        }
    }

    uint64_t nextDataOffset = (uint64_t)TXTR_PACK_HEADER_SIZE + ((uint64_t)count * (uint64_t)TXTR_PACK_ENTRY_SIZE);
    uint32_t cookedCount = 0;
    uint32_t skippedCount = 0;

    for (uint32_t pageId = 0; pageId < count; pageId++)
    {
        Texture *texture = &dataWin->txtr.textures[pageId];
        if (texture->blobData == NULL || texture->blobSize == 0)
        {
            skippedCount++;
            continue;
        }

        DecodedTexture decoded;
        if (!decodeTexture(texture, (int)pageId, &decoded))
        {
            fclose(out);
            free(entries);
            remove(outputPath);
            return false;
        }

        size_t rgbaSize = (size_t)decoded.width * (size_t)decoded.height * 4u;
        if (fwrite(decoded.rgba, 1, rgbaSize, out) != rgbaSize)
        {
            fprintf(stderr, "txtr_cooker: failed to write pixels for texture %u\n", pageId);
            stbi_image_free(decoded.rgba);
            fclose(out);
            free(entries);
            remove(outputPath);
            return false;
        }

        uint8_t *entry = entries + ((size_t)pageId * (size_t)TXTR_PACK_ENTRY_SIZE);
        TxtrPack_writeU32LE(entry + 0, (uint32_t)decoded.width);
        TxtrPack_writeU32LE(entry + 4, (uint32_t)decoded.height);
        TxtrPack_writeU64LE(entry + 8, nextDataOffset);
        TxtrPack_writeU64LE(entry + 16, (uint64_t)rgbaSize);
        TxtrPack_writeU32LE(entry + 24, texture->blobOffset);
        TxtrPack_writeU32LE(entry + 28, texture->blobSize);
        TxtrPack_writeU32LE(entry + 32, TXTR_PACK_PIXEL_FORMAT_RGBA8);
        TxtrPack_writeU32LE(entry + 36, TXTR_PACK_ENTRY_FLAG_PRESENT);

        nextDataOffset += (uint64_t)rgbaSize;
        cookedCount++;
        stbi_image_free(decoded.rgba);
    }

    if (fseek(out, (long)TXTR_PACK_HEADER_SIZE, SEEK_SET) != 0)
    {
        fprintf(stderr, "txtr_cooker: failed to seek back to entry table\n");
        fclose(out);
        free(entries);
        remove(outputPath);
        return false;
    }

    if (count > 0 && fwrite(entries, TXTR_PACK_ENTRY_SIZE, count, out) != count)
    {
        fprintf(stderr, "txtr_cooker: failed to write entry table\n");
        fclose(out);
        free(entries);
        remove(outputPath);
        return false;
    }

    fclose(out);
    free(entries);

    fprintf(stderr,
            "txtr_cooker: wrote %u cooked textures to %s (%u skipped)\n",
            cookedCount,
            outputPath,
            skippedCount);
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <input data.win> <output txtr.pack>\n", argv[0]);
        return 1;
    }

    DataWin *dataWin = DataWin_parse(
        argv[1],
        (DataWinParserOptions){
            .parseOptn = false,
            .parseLang = false,
            .parseExtn = false,
            .parseSond = false,
            .parseAgrp = false,
            .parseSprt = false,
            .parseBgnd = false,
            .parsePath = false,
            .parseScpt = false,
            .parseGlob = false,
            .parseShdr = false,
            .parseFont = false,
            .parseTmln = false,
            .parseObjt = false,
            .parseRoom = false,
            .parseTpag = false,
            .parseCode = false,
            .parseVari = false,
            .parseFunc = false,
            .parseGen8 = true,
            .parseStrg = true,
            .parseTxtr = true,
            .loadTxtrBlobData = true,
        });

    bool ok = writeTexturePack(argv[2], dataWin);
    DataWin_free(dataWin);
    return ok ? 0 : 1;
}
