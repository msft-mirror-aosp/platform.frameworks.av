/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <mutex>

#include <dlfcn.h>

#include "log/log.h"

#include <apex/ApexCodecs.h>

// This file provides a lazy interface to libapexcodecs.so to address early boot dependencies.

// Method pointers to libapexcodecs methods are held in an array which simplifies checking
// all pointers are initialized.
enum MethodIndex {
    k_ApexCodec_Component_create,
    k_ApexCodec_Component_destroy,
    k_ApexCodec_Component_flush,
    k_ApexCodec_Component_getConfigurable,
    k_ApexCodec_Component_process,
    k_ApexCodec_Component_start,
    k_ApexCodec_Component_reset,
    k_ApexCodec_Configurable_config,
    k_ApexCodec_Configurable_query,
    k_ApexCodec_Configurable_querySupportedParams,
    k_ApexCodec_Configurable_querySupportedValues,
    k_ApexCodec_GetComponentStore,
    k_ApexCodec_ParamDescriptors_getDescriptor,
    k_ApexCodec_ParamDescriptors_getIndices,
    k_ApexCodec_ParamDescriptors_release,
    k_ApexCodec_SettingResults_getResultAtIndex,
    k_ApexCodec_SettingResults_release,
    k_ApexCodec_SupportedValues_getTypeAndValues,
    k_ApexCodec_SupportedValues_release,
    k_ApexCodec_Traits_get,

    // Marker for count of methods
    k_MethodCount
};

// Table of methods pointers in libapexcodecs APIs.
static void* g_Methods[k_MethodCount];

static void* LoadLibapexcodecs(int dlopen_flags) {
    return dlopen("libapexcodecs.so", dlopen_flags);
}

// Initialization and symbol binding.

static void BindSymbol(void* handle, const char* name, enum MethodIndex index) {
    void* symbol = dlsym(handle, name);
    LOG_ALWAYS_FATAL_IF(symbol == nullptr, "Failed to find symbol '%s' in libapexcodecs.so: %s",
                        name, dlerror());
    g_Methods[index] = symbol;
}

static void InitializeOnce() {
    void* handle = LoadLibstatssocket(RTLD_NOW);
    LOG_ALWAYS_FATAL_IF(handle == nullptr, "Failed to load libapexcodecs.so: %s", dlerror());

#undef BIND_SYMBOL
#define BIND_SYMBOL(name) BindSymbol(handle, #name, k_##name);
    BIND_SYMBOL(ApexCodec_Component_create);
    BIND_SYMBOL(ApexCodec_Component_destroy);
    BIND_SYMBOL(ApexCodec_Component_flush);
    BIND_SYMBOL(ApexCodec_Component_getConfigurable);
    BIND_SYMBOL(ApexCodec_Component_process);
    BIND_SYMBOL(ApexCodec_Component_start);
    BIND_SYMBOL(ApexCodec_Component_reset);
    BIND_SYMBOL(ApexCodec_Configurable_config);
    BIND_SYMBOL(ApexCodec_Configurable_query);
    BIND_SYMBOL(ApexCodec_Configurable_querySupportedParams);
    BIND_SYMBOL(ApexCodec_Configurable_querySupportedValues);
    BIND_SYMBOL(ApexCodec_GetComponentStore);
    BIND_SYMBOL(ApexCodec_ParamDescriptors_getDescriptor);
    BIND_SYMBOL(ApexCodec_ParamDescriptors_getIndices);
    BIND_SYMBOL(ApexCodec_ParamDescriptors_release);
    BIND_SYMBOL(ApexCodec_SettingResults_getResultAtIndex);
    BIND_SYMBOL(ApexCodec_SettingResults_release);
    BIND_SYMBOL(ApexCodec_SupportedValues_getTypeAndValues);
    BIND_SYMBOL(ApexCodec_SupportedValues_release);
    BIND_SYMBOL(ApexCodec_Traits_get);
#undef BIND_SYMBOL

    // Check every symbol is bound.
    for (int i = 0; i < k_MethodCount; ++i) {
        LOG_ALWAYS_FATAL_IF(g_Methods[i] == nullptr,
                            "Uninitialized method in libapexcodecs_lazy at index: %d", i);
    }
}

static void EnsureInitialized() {
    static std::once_flag initialize_flag;
    std::call_once(initialize_flag, InitializeOnce);
}

#define INVOKE_METHOD(name, args...)                            \
    do {                                                        \
        EnsureInitialized();                                    \
        void* method = g_Methods[k_##name];                     \
        return reinterpret_cast<decltype(&name)>(method)(args); \
    } while (0)

//
// Forwarding for methods in ApexCodecs.h.
//

ApexCodec_ComponentStore *ApexCodec_GetComponentStore() {
    INVOKE_METHOD(ApexCodec_GetComponentStore);
}

ApexCodec_ComponentTraits *ApexCodec_Traits_get(
        ApexCodec_ComponentStore *store, size_t index) {
    INVOKE_METHOD(ApexCodec_Traits_get, store, index);
}

ApexCodec_Status ApexCodec_Component_create(
        ApexCodec_ComponentStore *store, const char *name, ApexCodec_Component **comp) {
    INVOKE_METHOD(ApexCodec_Component_create, store, name, comp);
}

void ApexCodec_Component_destroy(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_destroy, comp);
}

ApexCodec_Status ApexCodec_Component_start(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_start, comp);
}

ApexCodec_Status ApexCodec_Component_flush(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_flush, comp);
}

ApexCodec_Status ApexCodec_Component_reset(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_reset, comp);
}

ApexCodec_Configurable *ApexCodec_Component_getConfigurable(
        ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_getConfigurable, comp);
}

ApexCodec_Status ApexCodec_SupportedValues_getTypeAndValues(
        ApexCodec_SupportedValues *supportedValues,
        ApexCodec_SupportedValuesType *type,
        ApexCodec_SupportedValuesNumberType *numberType,
        ApexCodec_Value **values,
        uint32_t *numValues) {
    INVOKE_METHOD(ApexCodec_SupportedValues_getTypeAndValues,
                  supportedValues, type, numberType, values, numValues);
}

void ApexCodec_SupportedValues_release(ApexCodec_SupportedValues *values) {
    INVOKE_METHOD(ApexCodec_SupportedValues_release, values);
}

ApexCodec_Status ApexCodec_SettingResults_getResultAtIndex(
        ApexCodec_SettingResults *results,
        size_t index,
        ApexCodec_SettingResultFailure *failure,
        ApexCodec_ParamFieldValues *field,
        ApexCodec_ParamFieldValues **conflicts,
        size_t *numConflicts) {
    INVOKE_METHOD(ApexCodec_SettingResults_getResultAtIndex,
                  results, index, failure, field, conflicts, numConflicts);
}

void ApexCodec_SettingResults_release(ApexCodec_SettingResults *results) {
    INVOKE_METHOD(ApexCodec_SettingResults_release, results);
}

ApexCodec_Status ApexCodec_Component_process(
        ApexCodec_Component *comp,
        const ApexCodec_Buffer *input,
        ApexCodec_Buffer *output,
        size_t *consumed,
        size_t *produced) {
    INVOKE_METHOD(ApexCodec_Component_process, comp, input, output, consumed, produced);
}

ApexCodec_Status ApexCodec_Configurable_config(
        ApexCodec_Configurable *comp,
        ApexCodec_LinearBuffer *config,
        ApexCodec_SettingResults **results) {
    INVOKE_METHOD(ApexCodec_Configurable_config, comp, config, results);
}

ApexCodec_Status ApexCodec_Configurable_query(
        ApexCodec_Configurable *comp,
        uint32_t indices[],
        size_t numIndices,
        ApexCodec_LinearBuffer *config,
        size_t *writtenOrRequested) {
    INVOKE_METHOD(ApexCodec_Configurable_query,
                  comp, indices, numIndices, config, writtenOrRequested);
}

ApexCodec_Status ApexCodec_ParamDescriptors_getIndices(
        ApexCodec_ParamDescriptors *descriptors,
        uint32_t **indices,
        size_t *numIndices) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_getIndices, descriptors, indices, numIndices);
}

ApexCodec_Status ApexCodec_ParamDescriptors_getDescriptor(
        ApexCodec_ParamDescriptors *descriptors,
        uint32_t index,
        ApexCodec_ParamAttribute *attr,
        const char **name,
        uint32_t **dependencies,
        size_t *numDependencies) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_getDescriptor,
                  descriptors, index, attr, name, dependencies, numDependencies);
}

ApexCodec_Status ApexCodec_ParamDescriptors_release(
        ApexCodec_ParamDescriptors *descriptors) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_release, descriptors);
}

ApexCodec_Status ApexCodec_Configurable_querySupportedParams(
        ApexCodec_Configurable *comp,
        ApexCodec_ParamDescriptors **descriptors) {
    INVOKE_METHOD(ApexCodec_Configurable_querySupportedParams, comp, descriptors);
}

ApexCodec_Status ApexCodec_Configurable_querySupportedValues(
        ApexCodec_Configurable *comp,
        ApexCodec_SupportedValuesQuery *queries,
        size_t numQueries) {
    INVOKE_METHOD(ApexCodec_Configurable_querySupportedValues, comp, queries, numQueries);
}