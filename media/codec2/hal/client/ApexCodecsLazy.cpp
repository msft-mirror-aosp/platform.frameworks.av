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

//#define LOG_NDEBUG 0
#define LOG_TAG "ApexCodecsLazy"
#include <log/log.h>

#include <mutex>

#include <dlfcn.h>

#include <android-base/no_destructor.h>
#include <apex/ApexCodecs.h>
#include <utils/RWLock.h>

using android::RWLock;

namespace {

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

class ApexCodecsLazyLoader {
public:
    ApexCodecsLazyLoader() = default;

    static ApexCodecsLazyLoader &Get() {
        static ::android::base::NoDestructor<ApexCodecsLazyLoader> sLoader;
        return *sLoader;
    }

    void *getMethodAt(enum MethodIndex index) {
        RWLock::AutoRLock l(mLock);
        if (mInit) {
            return mMethods[index];
        } else {
            mLock.unlock();
            if (!init()) {
                return nullptr;
            }
            mLock.readLock();
            return mMethods[index];
        }
    }

private:
    static void* LoadLibapexcodecs(int dlopen_flags) {
        return dlopen("libapexcodecs.so", dlopen_flags);
    }

    // Initialization and symbol binding.
    void bindSymbol_l(void* handle, const char* name, enum MethodIndex index) {
        void* symbol = dlsym(handle, name);
        ALOGI_IF(symbol == nullptr, "Failed to find symbol '%s' in libapexcodecs.so: %s",
                 name, dlerror());
        mMethods[index] = symbol;
    }

    bool init() {
        {
            RWLock::AutoRLock l(mLock);
            if (mInit) {
                return true;
            }
        }
        void* handle = LoadLibapexcodecs(RTLD_NOW);
        if (handle == nullptr) {
            ALOGI("Failed to load libapexcodecs.so: %s", dlerror());
            return false;
        }

        RWLock::AutoWLock l(mLock);
#undef BIND_SYMBOL
#define BIND_SYMBOL(name) bindSymbol_l(handle, #name, k_##name);
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
            if (mMethods[i] == nullptr) {
                ALOGI("Uninitialized method in libapexcodecs_lazy at index: %d", i);
                return false;
            }
        }
        mInit = true;
        return true;
    }

    RWLock mLock;
    // Table of methods pointers in libapexcodecs APIs.
    void* mMethods[k_MethodCount];
    bool mInit{false};
};

}  // anonymous namespace

#define INVOKE_METHOD(name, returnIfNull, args...)                          \
    do {                                                                    \
        void* method = ApexCodecsLazyLoader::Get().getMethodAt(k_##name);   \
        if (!method) return (returnIfNull);                                 \
        return reinterpret_cast<decltype(&name)>(method)(args);             \
    } while (0)

//
// Forwarding for methods in ApexCodecs.h.
//

ApexCodec_ComponentStore *ApexCodec_GetComponentStore() {
    INVOKE_METHOD(ApexCodec_GetComponentStore, nullptr);
}

ApexCodec_ComponentTraits *ApexCodec_Traits_get(
        ApexCodec_ComponentStore *store, size_t index) {
    INVOKE_METHOD(ApexCodec_Traits_get, nullptr, store, index);
}

ApexCodec_Status ApexCodec_Component_create(
        ApexCodec_ComponentStore *store, const char *name, ApexCodec_Component **comp) {
    INVOKE_METHOD(ApexCodec_Component_create, APEXCODEC_STATUS_OMITTED, store, name, comp);
}

void ApexCodec_Component_destroy(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_destroy, void(), comp);
}

ApexCodec_Status ApexCodec_Component_start(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_start, APEXCODEC_STATUS_OMITTED, comp);
}

ApexCodec_Status ApexCodec_Component_flush(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_flush, APEXCODEC_STATUS_OMITTED, comp);
}

ApexCodec_Status ApexCodec_Component_reset(ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_reset, APEXCODEC_STATUS_OMITTED, comp);
}

ApexCodec_Configurable *ApexCodec_Component_getConfigurable(
        ApexCodec_Component *comp) {
    INVOKE_METHOD(ApexCodec_Component_getConfigurable, nullptr, comp);
}

ApexCodec_Status ApexCodec_SupportedValues_getTypeAndValues(
        ApexCodec_SupportedValues *supportedValues,
        ApexCodec_SupportedValuesType *type,
        ApexCodec_SupportedValuesNumberType *numberType,
        ApexCodec_Value **values,
        uint32_t *numValues) {
    INVOKE_METHOD(ApexCodec_SupportedValues_getTypeAndValues, APEXCODEC_STATUS_OMITTED,
                  supportedValues, type, numberType, values, numValues);
}

void ApexCodec_SupportedValues_release(ApexCodec_SupportedValues *values) {
    INVOKE_METHOD(ApexCodec_SupportedValues_release, void(), values);
}

ApexCodec_Status ApexCodec_SettingResults_getResultAtIndex(
        ApexCodec_SettingResults *results,
        size_t index,
        ApexCodec_SettingResultFailure *failure,
        ApexCodec_ParamFieldValues *field,
        ApexCodec_ParamFieldValues **conflicts,
        size_t *numConflicts) {
    INVOKE_METHOD(ApexCodec_SettingResults_getResultAtIndex, APEXCODEC_STATUS_OMITTED,
                  results, index, failure, field, conflicts, numConflicts);
}

void ApexCodec_SettingResults_release(ApexCodec_SettingResults *results) {
    INVOKE_METHOD(ApexCodec_SettingResults_release, void(), results);
}

ApexCodec_Status ApexCodec_Component_process(
        ApexCodec_Component *comp,
        const ApexCodec_Buffer *input,
        ApexCodec_Buffer *output,
        size_t *consumed,
        size_t *produced) {
    INVOKE_METHOD(ApexCodec_Component_process, APEXCODEC_STATUS_OMITTED,
                  comp, input, output, consumed, produced);
}

ApexCodec_Status ApexCodec_Configurable_config(
        ApexCodec_Configurable *comp,
        ApexCodec_LinearBuffer *config,
        ApexCodec_SettingResults **results) {
    INVOKE_METHOD(ApexCodec_Configurable_config, APEXCODEC_STATUS_OMITTED, comp, config, results);
}

ApexCodec_Status ApexCodec_Configurable_query(
        ApexCodec_Configurable *comp,
        uint32_t indices[],
        size_t numIndices,
        ApexCodec_LinearBuffer *config,
        size_t *writtenOrRequested) {
    INVOKE_METHOD(ApexCodec_Configurable_query, APEXCODEC_STATUS_OMITTED,
                  comp, indices, numIndices, config, writtenOrRequested);
}

ApexCodec_Status ApexCodec_ParamDescriptors_getIndices(
        ApexCodec_ParamDescriptors *descriptors,
        uint32_t **indices,
        size_t *numIndices) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_getIndices, APEXCODEC_STATUS_OMITTED,
                  descriptors, indices, numIndices);
}

ApexCodec_Status ApexCodec_ParamDescriptors_getDescriptor(
        ApexCodec_ParamDescriptors *descriptors,
        uint32_t index,
        ApexCodec_ParamAttribute *attr,
        const char **name,
        uint32_t **dependencies,
        size_t *numDependencies) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_getDescriptor, APEXCODEC_STATUS_OMITTED,
                  descriptors, index, attr, name, dependencies, numDependencies);
}

ApexCodec_Status ApexCodec_ParamDescriptors_release(
        ApexCodec_ParamDescriptors *descriptors) {
    INVOKE_METHOD(ApexCodec_ParamDescriptors_release, APEXCODEC_STATUS_OMITTED, descriptors);
}

ApexCodec_Status ApexCodec_Configurable_querySupportedParams(
        ApexCodec_Configurable *comp,
        ApexCodec_ParamDescriptors **descriptors) {
    INVOKE_METHOD(ApexCodec_Configurable_querySupportedParams, APEXCODEC_STATUS_OMITTED,
                  comp, descriptors);
}

ApexCodec_Status ApexCodec_Configurable_querySupportedValues(
        ApexCodec_Configurable *comp,
        ApexCodec_SupportedValuesQuery *queries,
        size_t numQueries) {
    INVOKE_METHOD(ApexCodec_Configurable_querySupportedValues, APEXCODEC_STATUS_OMITTED,
                  comp, queries, numQueries);
}
