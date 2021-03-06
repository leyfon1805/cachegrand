#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>
#include <openssl/bn.h>

#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_CLFLUSHOPT == 1
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif


#include "memory_fences.h"
#include "exttypes.h"
#include "spinlock.h"
#include "misc.h"
#include "log.h"
#include "fatal.h"
#include "utils_cpu.h"
#include "xalloc.h"
#include "random.h"

#include "hashtable/hashtable.h"
#include "hashtable/hashtable_config.h"
#include "hashtable/hashtable_support_hash.h"
#include "hashtable/hashtable_op_set.h"

#include "support.h"

LOG_PRODUCER_CREATE_DEFAULT("test-support", test_support)

void test_support_hashtable_print_heatmap(
        hashtable_t* hashtable,
        uint8_t columns) {
    uint64_t slots_used_max = HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;

    fprintf(stdout, "\n");
    fprintf(stdout, "-------------------\n");
    fprintf(stdout, "HASHTABLE HEATMAP\n");
    fprintf(stdout, "-------------------\n");

    volatile hashtable_data_t* ht_data = hashtable->ht_current;
    uint8_t overflowed_chunks_counter_digits_max = 3;
    uint8_t overflowed_chunks_counter_highest = 3;

    for(
            hashtable_chunk_index_t chunk_index = 0;
            chunk_index < ht_data->chunks_count;
            chunk_index++) {
        uint64_t overflowed_chunks_counter = ht_data->half_hashes_chunk[chunk_index].metadata.overflowed_chunks_counter;
        overflowed_chunks_counter_highest = max(overflowed_chunks_counter_highest, overflowed_chunks_counter);
    }

    fprintf(stdout, " %5s |", "");
    for(
            hashtable_chunk_index_t chunk_index = 0;
            chunk_index < min(columns, ht_data->chunks_count);
            chunk_index++) {
        fprintf(
                stdout,
                " %*lu |",
                2 + 1 + overflowed_chunks_counter_digits_max,
                chunk_index);
    }
    fprintf(stdout, "\n");

    fprintf(stdout, "       +");
    for(
            hashtable_chunk_index_t chunk_index = 0;
            chunk_index < min(columns, ht_data->chunks_count);
            chunk_index++) {
        fprintf(stdout, "%.*s+", 5 + overflowed_chunks_counter_digits_max, "--------------------------------");
    }
    fprintf(stdout, "\n");

    fprintf(stdout, " %5d |", 0);
    hashtable_chunk_index_t chunk_index;
    hashtable_chunk_count_t overflowed_chunks_counter_remaining = 0;
    for(chunk_index = 0; chunk_index < ht_data->chunks_count; chunk_index++) {
        uint8_t slots_used = 0;
        uint8_t overflowed_chunks_counter = ht_data->half_hashes_chunk[chunk_index].metadata.overflowed_chunks_counter;

        if (overflowed_chunks_counter > 0 && overflowed_chunks_counter + 1 > overflowed_chunks_counter_remaining) {
            overflowed_chunks_counter_remaining = overflowed_chunks_counter + 1;
        }

        for(
                hashtable_chunk_slot_index_t chunk_slot_index = 0;
                chunk_slot_index < HASHTABLE_HALF_HASHES_CHUNK_SLOTS_COUNT;
                chunk_slot_index++) {
            if (ht_data->half_hashes_chunk[chunk_index].half_hashes[chunk_slot_index].slot_id == 0) {
                continue;
            }
            slots_used++;
        }

        if (chunk_index > 0 && chunk_index % columns == 0) {
            fprintf(stdout, " from <%05lu> -> to <%05lu>\n",
                    chunk_index - columns, chunk_index - 1);
            fprintf(stdout, " %5u |",
                    (int)chunk_index / columns);
        }

        char colour_escape_codes[255] = {0};

        if (slots_used == 0) {
            sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 255, 255, 255);
        } else if (slots_used < slots_used_max * 0.25) {
            sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 200, 255, 200);
        } else if (slots_used < slots_used_max * 0.50) {
            sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 255, 255, 160);
        } else if (slots_used < slots_used_max * 0.75) {
            sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 255, 220, 0);
        } else if (slots_used < slots_used_max * 0.85) {
            size_t colour_escape_codes_off = sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 255, 100, 0);
            sprintf(colour_escape_codes + colour_escape_codes_off,
                    "\033[38;2;%d;%d;%dm", 255, 255, 255);
        } else {
            size_t colour_escape_codes_off = sprintf(colour_escape_codes,
                    "\033[48;2;%d;%d;%dm", 255, 0, 0);
            sprintf(colour_escape_codes + colour_escape_codes_off,
                    "\033[38;2;%d;%d;%dm", 255, 255, 255);
        }

        if (overflowed_chunks_counter_remaining > 0) {
            fprintf(stdout, "%s", colour_escape_codes);
        }

        fprintf(stdout, " ");

        if (chunk_index > ht_data->chunks_count - HASHTABLE_HALF_HASHES_CHUNK_SEARCH_MAX) {
            fprintf(stdout, "\033[2m");
        }

        fprintf(stdout, "%s", colour_escape_codes);
        fprintf(stdout, "%2d/%*u",
                slots_used,
                overflowed_chunks_counter_digits_max,
                ht_data->half_hashes_chunk[chunk_index].metadata.overflowed_chunks_counter);

        if (overflowed_chunks_counter_remaining > 0) {
            overflowed_chunks_counter_remaining--;
        } else {
            fprintf(stdout, "\033[0m");
        }

        fprintf(stdout, " ");

        if (overflowed_chunks_counter_remaining == 0) {
            fprintf(stdout, "\033[0m|");
        } else {
            fprintf(stdout, "*\033[0m");
        }
    }

    if (chunk_index > 0 && chunk_index % columns != 0) {
        fprintf(stdout, "%*s",
                (int)((6 + overflowed_chunks_counter_digits_max) * (columns - (chunk_index % columns))),
                "");
    }
    fprintf(stdout, " from <%05lu> -> tp <%05lu>\n",
            chunk_index - (chunk_index % columns), ht_data->chunks_count - 1);

    fprintf(stdout, "-------------------\n");
}

test_key_same_bucket_t* test_support_same_hash_mod_fixtures_generate(
        hashtable_bucket_count_t bucket_count,
        const char* key_prefix,
        uint32_t count) {
    char* key_test;
    char* key_test_copy;
    char* key_model;
    size_t key_test_size;
    test_key_same_bucket_t* test_key_same_bucket_fixtures;
    bool reference_hash_set = false;
    hashtable_bucket_index_t reference_bucket_index;

    uint32_t matches_counter = 0;

    // Build the key model
    key_model = (char*)malloc(strlen(key_prefix) + 5);
    sprintf(key_model, "%s%%09d", key_prefix);

    // Build the struct to hold the fixtures (+1 to keep the last as NULL)
    test_key_same_bucket_fixtures = (test_key_same_bucket_t*)malloc(sizeof(test_key_same_bucket_t) * (count + 1));
    memset(test_key_same_bucket_fixtures, 0, sizeof(test_key_same_bucket_t) * (count + 1));

    // Build the temporary key
    key_test_size = strlen(key_prefix) + 10;
    key_test = (char*)malloc(key_test_size);
    memset(key_test, 0, key_test_size);

    for(uint64_t i = 0; i<=999999999; i++) {
        bool add_fixture = false;

        snprintf(key_test, key_test_size, key_model, i);
        hashtable_hash_t hash_test = hashtable_support_hash_calculate(key_test, strlen(key_test));

        if (!reference_hash_set) {
            reference_bucket_index = hash_test & (bucket_count - 1);
            add_fixture = reference_hash_set = true;
        } else {
            add_fixture = reference_bucket_index == (hash_test & (bucket_count - 1));
        }

        if (add_fixture) {
            key_test_copy = (char*)malloc(key_test_size);
            memset(key_test_copy, 0, key_test_size);
            strncpy(key_test_copy, key_test, key_test_size);

            test_key_same_bucket_fixtures[matches_counter].key = key_test_copy;
            test_key_same_bucket_fixtures[matches_counter].key_len = strlen(key_test);
            test_key_same_bucket_fixtures[matches_counter].key_hash = hash_test;
            test_key_same_bucket_fixtures[matches_counter].key_hash_half =
                    hashtable_support_hash_half(hash_test);
            test_key_same_bucket_fixtures[matches_counter].key_hash_quarter =
                    hashtable_support_hash_quarter(hashtable_support_hash_half(hash_test));

            matches_counter++;

            if (matches_counter == count) {
                break;
            }
        }
    }

    free(key_model);
    free(key_test);

    assert(matches_counter == count);

    return test_key_same_bucket_fixtures;
}

void test_support_same_hash_mod_fixtures_free(
        test_key_same_bucket_t* test_key_same_bucket) {
    uint32_t i = 0;

    while(test_key_same_bucket[i].key != NULL) {
        free(test_key_same_bucket[i].key);
        i++;
    }

    free(test_key_same_bucket);
}

void test_support_set_thread_affinity(
        int thread_index) {
#if !defined(__MINGW32__)
    int res;
    cpu_set_t cpuset;
    pthread_t thread;

    uint32_t logical_core_count = utils_cpu_count();
    uint32_t logical_core_index = (thread_index % logical_core_count) * 2;

    if (logical_core_index >= logical_core_count) {
        logical_core_index = logical_core_index - logical_core_count + 1;
    }

    LOG_DI("Thread <%u> on core <%u>", thread_index, logical_core_index);

    CPU_ZERO(&cpuset);
    CPU_SET(logical_core_index, &cpuset);

    thread = pthread_self();
    res = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        perror("pthread_setaffinity_np");
    }
#endif
}

static void* test_support_build_keys_random_max_length_thread_func(
        void *arg) {
    uint64_t keys_generated = 0;
    keyset_generator_thread_info_t* ti = (keyset_generator_thread_info_t*)arg;

    // Pre-initialize the seed to always generate the same sequence of random numbers
    random_init(ti->random_seed_base + ti->thread_num);

    test_support_set_thread_affinity(ti->thread_num);
    LOG_DI("[Thread <%u>] waiting for start_flag", ti->thread_num);

    do {
        HASHTABLE_MEMORY_FENCE_LOAD();
    } while (*ti->start_flag == 0);

    LOG_DI("[Thread <%u>] start_flag == 1", ti->thread_num);

    for (uint64_t i = ti->start; i < ti->end; i++) {
        keys_generated++;
        char* keys_current = ti->keyset + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);
        uint8_t length =TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH;

        for (uint8_t i2 = 0; i2 < length; i2++) {
            *keys_current = ti->charset_list[random_generate() % TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_SIZE];
            keys_current++;
        }
        *keys_current = 0;
    }

    LOG_DI("[Thread <%u>] key generation completed, generated <%lu> keys", ti->thread_num, keys_generated);

    return 0;
}

static void* test_support_build_keys_random_random_length_thread_func(
        void *arg) {
    uint64_t keys_generated = 0;
    keyset_generator_thread_info_t* ti = (keyset_generator_thread_info_t*)arg;

    // Pre-initialize the seed to always generate the same sequence of random numbers
    random_init(ti->random_seed_base + ti->thread_num);

    test_support_set_thread_affinity(ti->thread_num);
    LOG_DI("[Thread <%u>] waiting for start_flag", ti->thread_num);

    do {
        HASHTABLE_MEMORY_FENCE_LOAD();
    } while (*ti->start_flag == 0);

    LOG_DI("[Thread <%u>] start_flag == 1", ti->thread_num);

    for (uint64_t i = ti->start; i < ti->end; i++) {
        keys_generated++;
        char* keys_current = ti->keyset + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

        uint8_t length =
                ((random_generate() % (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH - TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH)) + TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH) -
                1;

        for (uint8_t i2 = 0; i2 < length; i2++) {
            *keys_current = ti->charset_list[random_generate() % TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_SIZE];
            keys_current++;
        }

        *keys_current = 0;
    }

    LOG_DI("[Thread <%u>] key generation completed, generated <%lu> keys", ti->thread_num, keys_generated);

    return 0;
}

static void* test_support_build_keys_repeatible_set_min_max_length_thread_func(
        void *arg) {
    uint64_t keys_generated = 0;
    keyset_generator_thread_info_t* ti = (keyset_generator_thread_info_t*)arg;

    // Pre-initialize the seed to always generate the same sequence of random numbers
    random_init(ti->random_seed_base + ti->thread_num);

    test_support_set_thread_affinity(ti->thread_num);
    LOG_DI("[Thread <%u>] waiting for start_flag", ti->thread_num);

    do {
        HASHTABLE_MEMORY_FENCE_LOAD();
    } while (*ti->start_flag == 0);

    LOG_DI("[Thread <%u>] start_flag == 1", ti->thread_num);

    BN_CTX* bn_ctx = BN_CTX_new();
    BIGNUM* charset_size_big = BN_new();
    BIGNUM* beginning_key_index_big = BN_new();
    BIGNUM* beginning_key_length_big = BN_new();
    BIGNUM* ending_key_index_big = BN_new();
    BIGNUM* ending_key_length_big = BN_new();

    BN_set_word(charset_size_big, strlen(ti->charset_list));
    BN_set_word(beginning_key_index_big, 0);
    BN_set_word(beginning_key_length_big, TEST_SUPPORT_RANDOM_KEYS_MIN_LENGTH);
    BN_set_word(ending_key_length_big, TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH);
    BN_exp(ending_key_index_big, charset_size_big, ending_key_length_big, bn_ctx);
    BN_sub_word(ending_key_index_big, 1);

    for (uint64_t i = ti->start; i < ti->end; i++) {
        keys_generated++;
        char* keys_current = ti->keyset + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

        bool is_key_from_beginning_of_set = (i & 0x01u) == 0;

        BIGNUM* key_length_big = is_key_from_beginning_of_set ? beginning_key_length_big : ending_key_length_big;
        BIGNUM* key_index_big = is_key_from_beginning_of_set ? beginning_key_index_big : ending_key_index_big;

        uint32_t key_length = BN_get_word(key_length_big);

        for (uint32_t key_char_index = 0; key_char_index < key_length; key_char_index++) {
            uint32_t charset_index = 0;
            uint32_t pos_from_end = (key_length - key_char_index) - 1;

            BIGNUM* pos_from_end_big = BN_new();
            BIGNUM* divisor_big = BN_new();
            BIGNUM* value_to_mod_big = BN_new();
            BIGNUM* charset_index_big = BN_new();

            BN_set_word(pos_from_end_big, pos_from_end);

            if (pos_from_end == 0) {
                BN_set_word(divisor_big, 1);
            } else {
                BN_exp(divisor_big, charset_size_big, pos_from_end_big, bn_ctx);
            }

            BN_div(value_to_mod_big, NULL, key_index_big, divisor_big, bn_ctx);
            BN_mod(charset_index_big, value_to_mod_big, charset_size_big, bn_ctx);
            charset_index = BN_get_word(charset_index_big);

            *keys_current = ti->charset_list[charset_index];
            keys_current++;

            BN_free(divisor_big);
            BN_free(pos_from_end_big);
            BN_free(value_to_mod_big);
            BN_free(charset_index_big);
        }

        *keys_current = 0;

        if (is_key_from_beginning_of_set) {
            BIGNUM* current_keyset_max_index_big = BN_new();
            BN_exp(current_keyset_max_index_big, charset_size_big, key_length_big, bn_ctx);

            BN_add_word(beginning_key_index_big, 1);
            if (BN_cmp(beginning_key_index_big, current_keyset_max_index_big) >= 0) {
                BN_set_word(beginning_key_index_big, 0);
                BN_add_word(beginning_key_length_big, 1);
            }

            BN_free(current_keyset_max_index_big);
        } else {
            if (BN_is_one(ending_key_index_big) == 1) {
                BN_sub_word(ending_key_length_big, 1);
                BN_exp(ending_key_index_big, charset_size_big, ending_key_length_big, bn_ctx);
            } else {
                BN_sub_word(ending_key_index_big, 1);
            }
        }

        if (BN_cmp(beginning_key_length_big, ending_key_length_big) == 0) {
            assert(false);
        }
    }

    BN_free(charset_size_big);
    BN_free(beginning_key_index_big);
    BN_free(beginning_key_length_big);
    BN_free(ending_key_index_big);
    BN_free(ending_key_length_big);

    BN_CTX_free(bn_ctx);

    LOG_DI("[Thread <%u>] key generation completed, generated <%lu> keys", ti->thread_num, keys_generated);

    return 0;
}

void test_support_free_keys(
        char* keys,
        uint64_t count) {
    xalloc_mmap_free(keys, count * (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH + 1));
}

char* test_support_init_keys(
        uint64_t keyset_size,
        uint8_t keys_generator_method,
        uint64_t random_seed_base) {
    int res;
    void* ret;
    uint8_t start_flag;
    pthread_attr_t attr;
    char *keyset;
    char *charset_list;
    size_t charset_size;
    void *(*keyset_generator_fp) (void *);

    char charset_list_repeated[] = {TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_LIST};
    char charset_list_unique[] = {TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_UNIQUE_LIST};

    switch (keys_generator_method) {
        default:
            FATAL(LOG_PRODUCER_DEFAULT, "Keyset genererator method <%d> unsupported", keys_generator_method);
            break;

        case TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_MAX_LENGTH:
            keyset_generator_fp = test_support_build_keys_random_max_length_thread_func;
            charset_size = TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_SIZE;
            charset_list = (char*)malloc(charset_size);
            memset(charset_list, 0, charset_size);
            memcpy(charset_list, charset_list_repeated, charset_size);
            break;

        case TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_RANDOM_STR_RANDOM_LENGTH:
            keyset_generator_fp = test_support_build_keys_random_random_length_thread_func;
            charset_size = TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_REPEATED_SIZE;
            charset_list = (char*)malloc(charset_size);
            memset(charset_list, 0, charset_size);
            memcpy(charset_list, charset_list_repeated, charset_size);
            break;

        case TEST_SUPPORT_RANDOM_KEYS_GEN_FUNC_REPETIBLE_STR_ALTERNATEMINMAX_LENGTH:
            keyset_generator_fp = test_support_build_keys_repeatible_set_min_max_length_thread_func;
            charset_size = TEST_SUPPORT_RANDOM_KEYS_CHARACTER_SET_UNIQUE_SIZE;
            charset_list = (char*)malloc(charset_size);
            memset(charset_list, 0, charset_size);
            memcpy(charset_list, charset_list_unique, charset_size);
            break;
    }

    keyset = (char*) xalloc_mmap_alloc(keyset_size * TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL);
    uint32_t threads_count = utils_cpu_count();

    LOG_DI("generating keyset using <%u> threads", threads_count);

    res = pthread_attr_init(&attr);
    if (res != 0) {
        perror("pthread_attr_init");
    }

    keyset_generator_thread_info_t* threads_info =
            (keyset_generator_thread_info_t*)calloc(threads_count, sizeof(keyset_generator_thread_info_t));
    if (threads_info == NULL) {
        perror("calloc");
    }

    LOG_DI("generating keys using <%u> threads", threads_count);

    start_flag = 0;
    uint64_t window_len = (keyset_size / threads_count) + 1;
    for(uint32_t thread_num = 0; thread_num < threads_count; thread_num++) {
        // Not perfect calc but spread the load, this approach will reduce cache lines collisions and let the threads to
        // work sequentially on their "chunk" of memory to work on
        uint64_t start = thread_num * window_len;
        uint64_t end = start + window_len;
        if (end > keyset_size) {
            end = keyset_size;
        }

        threads_info[thread_num].charset_list = charset_list;
        threads_info[thread_num].charset_size = charset_size;
        threads_info[thread_num].random_seed_base = random_seed_base;
        threads_info[thread_num].start_flag = &start_flag;
        threads_info[thread_num].thread_num = thread_num;
        threads_info[thread_num].threads_count = threads_count;
        threads_info[thread_num].keyset_size = keyset_size;
        threads_info[thread_num].keyset = keyset;
        threads_info[thread_num].start = start;
        threads_info[thread_num].end = end;

        if (pthread_create(
                &threads_info[thread_num].thread_id,
                &attr,
                keyset_generator_fp,
                &threads_info[thread_num]) != 0) {
            perror("pthread_create");
        }
    }

    LOG_DI("starting threads");

    start_flag = 1;
    HASHTABLE_MEMORY_FENCE_STORE();

    for(uint32_t thread_num = 0; thread_num < threads_count; thread_num++) {
        res = pthread_join(threads_info[thread_num].thread_id, &ret);
        if (res != 0) {
            perror("pthread_join");
        }
    }

    LOG_DI("generation completed");
    free(threads_info);

    LOG_DI("keyset generated");

    return keyset;
}

hashtable_t* test_support_init_hashtable(
        uint64_t initial_size) {
    hashtable_config_t* hashtable_config = hashtable_config_init();
    hashtable_config->initial_size = initial_size;
    hashtable_config->can_auto_resize = false;

    return hashtable_init(hashtable_config);
}

bool test_support_hashtable_prefill(
        hashtable_t* hashtable,
        char* keyset,
        uint64_t value,
        uint64_t insert_count) {
    for(long int i = 0; i < insert_count; i++) {
        char* key = keyset + (TEST_SUPPORT_RANDOM_KEYS_MAX_LENGTH_WITH_NULL * i);

        bool result = hashtable_op_set(
                hashtable,
                key,
                strlen(key),
                value + i);

        if (!result) {
            return false;
        }
    }

    return true;
}

void test_support_flush_data_cache(
        void *start,
        size_t len)
{
    // TODO: the cacheline size is currently hardcoded, should be determined at build type and a macro should be used
    //       to properly keep track of it
    uint32_t cacheline_size = 64;
    void *end = start + len;
    void *p;

    // clflushopt is unordered, needs a full memory fence
    HASHTABLE_MEMORY_FENCE_LOAD_STORE();

    for (p = start; p < end; p += cacheline_size) {
#if CACHEGRAND_CMAKE_CONFIG_HOST_HAS_CLFLUSHOPT == 1
        _mm_clflushopt(p);
#else
        _mm_clflush(p);
#endif
    }

    HASHTABLE_MEMORY_FENCE_LOAD_STORE();
}
