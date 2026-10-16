#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <assert.h>

#define BUSTER_ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))
#define BUSTER_COMPILE_TIME_STRING_LENGTH(strlit) (BUSTER_ARRAY_LENGTH(strlit) - 1)
#define BUSTER_S8_INIT(strlit) {.pointer = (char*)(strlit), .length = BUSTER_COMPILE_TIME_STRING_LENGTH(strlit)}
#define BUSTER_S8(strlit) ((String8)BUSTER_S8_INIT(strlit))

// OS-dependent includes
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

// CPU architecture dependent includes
#include <immintrin.h>

typedef struct String8 String8;
struct String8
{
    char* pointer;
    uint64_t length;
};

static void string_printf(String8 string)
{
    assert(string.length <= INT_MAX);
    printf("%.*s", (int)string.length, string.pointer);
}

typedef struct CompileOptions CompileOptions;
struct CompileOptions
{
    uint32_t foo;
};

typedef struct CompileResult CompileResult;
struct CompileResult
{
    bool success;
};

#define VECTOR_COUNT (4)
#define VECTOR_SIZE (64)

typedef char VCharV __attribute__((ext_vector_type(64)));

typedef struct VChar VChar;
struct VChar
{
    VCharV v[VECTOR_COUNT];
};

typedef struct MaskBool MaskBool;
struct MaskBool
{
    __mmask64 m[VECTOR_COUNT];
};

static MaskBool mask_and(MaskBool a, MaskBool b)
{
    MaskBool result;

    for (uint64_t i = 0; i < VECTOR_COUNT; i += 1)
    {
        result.m[i] = a.m[i] & b.m[i];
    }

    return result;
}

static MaskBool mask_or(MaskBool a, MaskBool b)
{
    MaskBool result;

    for (uint64_t i = 0; i < VECTOR_COUNT; i += 1)
    {
        result.m[i] = a.m[i] | b.m[i];
    }

    return result;
}

static MaskBool mask_xor(MaskBool a, MaskBool b)
{
    MaskBool result;

    for (uint64_t i = 0; i < VECTOR_COUNT; i += 1)
    {
        result.m[i] = a.m[i] ^ b.m[i];
    }

    return result;
}

static VChar vchar_load(const char* const restrict pointer)
{
    VChar result;

    for (uint64_t i = 0; i < VECTOR_COUNT; i += 1)
    {
        result.v[i] = _mm512_loadu_epi8(pointer + i * VECTOR_SIZE);
    }

    return result;
}

static MaskBool vchar_compare_unique(VChar v, char unique)
{
    MaskBool result;
    VCharV unique_v = _mm512_set1_epi8(unique);

    for (uint64_t i = 0; i < VECTOR_COUNT; i += 1)
    {
        result.m[i] = _mm512_cmpeq_epu8_mask(v.v[i], unique_v);
    }

    return result;
}

static bool primitive_tokenize_chunk(String8 chunk)
{
    bool result = false;
    const char* restrict it = chunk.pointer;
    const char* const restrict start = it;
    uint64_t chunk_length = chunk.length;
    const char* const restrict top = start + chunk_length;

    while (it < top)
    {
        const char* const restrict this_iteration_pointer = it;

        VChar chunks = vchar_load(this_iteration_pointer);
        it += sizeof(chunks);

        MaskBool is_lf = vchar_compare_unique(chunks, '\n');
        MaskBool is_cr = vchar_compare_unique(chunks, '\r');
        MaskBool is_tab = vchar_compare_unique(chunks, '\t');
        MaskBool is_whitespace = vchar_compare_unique(chunks, ' ');

        MaskBool is_space = mask_or(mask_or(is_lf, is_cr), mask_or(is_tab, is_whitespace));

        (void)mask_and;
        (void)mask_xor;
        (void)is_space;

        result = true;
        break;
    }

    return result;
}

static bool primitive_tokenize_file(String8 path)
{
    bool result = false;
    int fd = open(path.pointer, O_RDONLY | O_CLOEXEC, 0644);
    if (fd != -1)
    {
        struct stat file_stats;
        if (fstat(fd, &file_stats) == 0)
        {
            uint64_t length = (uint64_t)file_stats.st_size;
            void* mapped = mmap(0, length, PROT_READ, MAP_PRIVATE, fd, 0);
            if (mapped != MAP_FAILED)
            {
                String8 chunk = { .pointer = (char*)mapped, .length = length };
                result = primitive_tokenize_chunk(chunk);
            }
        }
    }

    return result;
}

static bool parse_compile_unit(String8 path)
{
    bool result = false;

    String8 current_file_path = path;

    while (true)
    {
        primitive_tokenize_file(current_file_path);
    }

    return result;
}

static CompileResult compile(String8 path, CompileOptions options)
{
    CompileResult result = {0};
    printf("Compiling ");
    string_printf(path);
    printf("...\n");
    bool parse_result = parse_compile_unit(path);
    result.success = parse_result;
    return result;
}

int main(void)
{
    printf("Hello world\n");
    compile(BUSTER_S8(__FILE__), (CompileOptions){0});
    return 0;
}
