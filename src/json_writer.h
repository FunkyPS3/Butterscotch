#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// ===[ JsonWriter Type ]===

typedef struct {
    char* buffer;
    size_t length;
    size_t capacity;
    bool needsComma;
} JsonWriter;

// ===[ Internal Helpers ]===

static void jsonWriter_ensureCapacity(JsonWriter* writer, size_t additional) {
    size_t required = writer->length + additional;
    if (required <= writer->capacity) return;

    size_t newCapacity = writer->capacity;
    while (newCapacity < required) {
        newCapacity *= 2;
    }

    writer->buffer = realloc(writer->buffer, newCapacity);
    if (writer->buffer == nullptr) {
        fprintf(stderr, "JsonWriter: realloc failed\n");
        abort();
    }
    writer->capacity = newCapacity;
}

static void jsonWriter_appendRaw(JsonWriter* writer, const char* data, size_t len) {
    jsonWriter_ensureCapacity(writer, len + 1);
    memcpy(writer->buffer + writer->length, data, len);
    writer->length += len;
    writer->buffer[writer->length] = '\0';
}

static void jsonWriter_appendStr(JsonWriter* writer, const char* str) {
    jsonWriter_appendRaw(writer, str, strlen(str));
}

static void jsonWriter_appendChar(JsonWriter* writer, char c) {
    jsonWriter_ensureCapacity(writer, 2);
    writer->buffer[writer->length++] = c;
    writer->buffer[writer->length] = '\0';
}

static void jsonWriter_writeCommaIfNeeded(JsonWriter* writer) {
    if (writer->needsComma) {
        jsonWriter_appendChar(writer, ',');
    }
}

static void jsonWriter_writeEscapedString(JsonWriter* writer, const char* str) {
    jsonWriter_appendChar(writer, '"');
    for (const char* p = str; *p != '\0'; p++) {
        unsigned char c = (unsigned char) *p;
        switch (c) {
            case '"':  jsonWriter_appendStr(writer, "\\\""); break;
            case '\\': jsonWriter_appendStr(writer, "\\\\"); break;
            case '\b': jsonWriter_appendStr(writer, "\\b");  break;
            case '\f': jsonWriter_appendStr(writer, "\\f");  break;
            case '\n': jsonWriter_appendStr(writer, "\\n");  break;
            case '\r': jsonWriter_appendStr(writer, "\\r");  break;
            case '\t': jsonWriter_appendStr(writer, "\\t");  break;
            default:
                if (32 > c) {
                    char escape[7];
                    snprintf(escape, sizeof(escape), "\\u%04x", c);
                    jsonWriter_appendStr(writer, escape);
                } else {
                    jsonWriter_appendChar(writer, (char) c);
                }
                break;
        }
    }
    jsonWriter_appendChar(writer, '"');
}

// ===[ Lifecycle ]===

static JsonWriter JsonWriter_create(void) {
    size_t initialCapacity = 256;
    char* buffer = malloc(initialCapacity);
    if (buffer == nullptr) {
        fprintf(stderr, "JsonWriter: malloc failed\n");
        abort();
    }
    buffer[0] = '\0';
    return (JsonWriter) {
        .buffer = buffer,
        .length = 0,
        .capacity = initialCapacity,
        .needsComma = false,
    };
}

static void JsonWriter_free(JsonWriter* writer) {
    free(writer->buffer);
    writer->buffer = nullptr;
    writer->length = 0;
    writer->capacity = 0;
}

// ===[ Structure ]===

static void JsonWriter_beginObject(JsonWriter* writer) {
    jsonWriter_writeCommaIfNeeded(writer);
    jsonWriter_appendChar(writer, '{');
    writer->needsComma = false;
}

static void JsonWriter_endObject(JsonWriter* writer) {
    jsonWriter_appendChar(writer, '}');
    writer->needsComma = true;
}

static void JsonWriter_beginArray(JsonWriter* writer) {
    jsonWriter_writeCommaIfNeeded(writer);
    jsonWriter_appendChar(writer, '[');
    writer->needsComma = false;
}

static void JsonWriter_endArray(JsonWriter* writer) {
    jsonWriter_appendChar(writer, ']');
    writer->needsComma = true;
}

// ===[ Object Keys ]===

static void JsonWriter_key(JsonWriter* writer, const char* key) {
    jsonWriter_writeCommaIfNeeded(writer);
    jsonWriter_writeEscapedString(writer, key);
    jsonWriter_appendChar(writer, ':');
    writer->needsComma = false;
}

// ===[ Values ]===

static void JsonWriter_string(JsonWriter* writer, const char* value) {
    jsonWriter_writeCommaIfNeeded(writer);
    if (value == nullptr) {
        jsonWriter_appendStr(writer, "null");
    } else {
        jsonWriter_writeEscapedString(writer, value);
    }
    writer->needsComma = true;
}

static void JsonWriter_int(JsonWriter* writer, int64_t value) {
    jsonWriter_writeCommaIfNeeded(writer);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long) value);
    jsonWriter_appendStr(writer, buf);
    writer->needsComma = true;
}

static void JsonWriter_double(JsonWriter* writer, double value) {
    jsonWriter_writeCommaIfNeeded(writer);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.17g", value);
    jsonWriter_appendStr(writer, buf);
    writer->needsComma = true;
}

static void JsonWriter_bool(JsonWriter* writer, bool value) {
    jsonWriter_writeCommaIfNeeded(writer);
    jsonWriter_appendStr(writer, value ? "true" : "false");
    writer->needsComma = true;
}

static void JsonWriter_null(JsonWriter* writer) {
    jsonWriter_writeCommaIfNeeded(writer);
    jsonWriter_appendStr(writer, "null");
    writer->needsComma = true;
}

// ===[ Property Convenience ]===

static void JsonWriter_propertyString(JsonWriter* writer, const char* key, const char* value) {
    JsonWriter_key(writer, key);
    JsonWriter_string(writer, value);
}

static void JsonWriter_propertyInt(JsonWriter* writer, const char* key, int64_t value) {
    JsonWriter_key(writer, key);
    JsonWriter_int(writer, value);
}

static void JsonWriter_propertyDouble(JsonWriter* writer, const char* key, double value) {
    JsonWriter_key(writer, key);
    JsonWriter_double(writer, value);
}

static void JsonWriter_propertyBool(JsonWriter* writer, const char* key, bool value) {
    JsonWriter_key(writer, key);
    JsonWriter_bool(writer, value);
}

static void JsonWriter_propertyNull(JsonWriter* writer, const char* key) {
    JsonWriter_key(writer, key);
    JsonWriter_null(writer);
}

// ===[ Output ]===

static const char* JsonWriter_getOutput(const JsonWriter* writer) {
    return writer->buffer;
}

static char* JsonWriter_copyOutput(const JsonWriter* writer) {
    return strdup(writer->buffer);
}

static size_t JsonWriter_getLength(const JsonWriter* writer) {
    return writer->length;
}
