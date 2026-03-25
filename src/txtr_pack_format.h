#pragma once

#include <stddef.h>
#include <stdint.h>

#define TXTR_PACK_MAGIC "CRTXPACK"
#define TXTR_PACK_MAGIC_SIZE 8u
#define TXTR_PACK_VERSION 2u
#define TXTR_PACK_FILENAME "txtr.pack"

#define TXTR_PACK_PIXEL_FORMAT_RGBA8 1u
#define TXTR_PACK_PIXEL_FORMAT_ARGB8 2u
#define TXTR_PACK_ENTRY_FLAG_PRESENT 0x1u

#define TXTR_PACK_HEADER_SIZE 20u
#define TXTR_PACK_ENTRY_SIZE 40u

static inline uint32_t TxtrPack_readU32LE(const uint8_t* src) {
    return ((uint32_t)src[0]) |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline uint64_t TxtrPack_readU64LE(const uint8_t* src) {
    return ((uint64_t)src[0]) |
           ((uint64_t)src[1] << 8) |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) |
           ((uint64_t)src[7] << 56);
}

static inline void TxtrPack_writeU32LE(uint8_t* dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

static inline void TxtrPack_writeU64LE(uint8_t* dst, uint64_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
    dst[4] = (uint8_t)((value >> 32) & 0xFFu);
    dst[5] = (uint8_t)((value >> 40) & 0xFFu);
    dst[6] = (uint8_t)((value >> 48) & 0xFFu);
    dst[7] = (uint8_t)((value >> 56) & 0xFFu);
}

static inline uint64_t TxtrPack_entryFileOffset(uint32_t pageId) {
    return (uint64_t)TXTR_PACK_HEADER_SIZE + ((uint64_t)pageId * (uint64_t)TXTR_PACK_ENTRY_SIZE);
}
