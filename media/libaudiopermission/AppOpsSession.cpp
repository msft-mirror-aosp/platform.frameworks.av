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

#include <media/AppOpsSession.h>
#include <media/AttrSourceIter.h>

#include <binder/AppOpsManager.h>
#include <binder/PermissionController.h>

using ::android::content::AttributionSourceState;

namespace android::media::permission {

// params are unreliable at the moment
void DefaultAppOpsFacade::OpMonitor::opChanged(int32_t, const String16&) {
    DefaultAppOpsFacade x{};
    const auto allowed = x.checkAccess(mAttr, mOps);
    std::lock_guard l_{mLock};
    if (mCb == nullptr) return;
    mCb(allowed);
}

bool DefaultAppOpsFacade::startAccess(const ValidatedAttributionSourceState& attr_, Ops ops) {
    const AttributionSourceState& attr = attr_;
    // TODO no support for additional op at the moment
    if (ops.attributedOp == AppOpsManager::OP_NONE) return true;  // nothing to do
    // TODO caching and sync up-call marking
    AppOpsManager ap{};
    return ap.startOpNoThrow(
        /*op=*/ ops.attributedOp,
        /*uid=*/ attr.uid,
        /*packageName=*/ String16{attr.packageName.value_or("").c_str()},
        /*startModeIfDefault=*/ false,
        /*attributionTag=*/ attr.attributionTag.has_value() ?
            String16{attr.attributionTag.value().c_str()}
                                            : String16{},
        /*msg=*/ String16{"AppOpsSession start"});
}

void DefaultAppOpsFacade::stopAccess(const ValidatedAttributionSourceState& attr_, Ops ops) {
    const AttributionSourceState& attr = attr_;
    // TODO caching and sync up-call marking
    AppOpsManager ap{};
    return ap.finishOp(
        /*op=*/ ops.attributedOp,
        /*uid=*/ attr.uid,
        /*callingPackage=*/ String16{attr.packageName.value_or("").c_str()},
        /*attributionTag=*/ attr.attributionTag.has_value() ?
                                            String16{attr.attributionTag.value().c_str()}
                                            : String16{});
}

bool DefaultAppOpsFacade::checkAccess(const ValidatedAttributionSourceState& attr, Ops ops) {
    const auto check = [&](int32_t op) -> bool {
        if (op == AppOpsManager::OP_NONE) return true;
        return std::all_of(
                AttrSourceIter::cbegin(attr), AttrSourceIter::cend(), [&](const auto& x) {
                    return AppOpsManager{}.checkOp(op, x.uid,
                                                   String16{x.packageName.value_or("").c_str()}) ==
                                   AppOpsManager::MODE_ALLOWED;
                });
    };
    return check(ops.attributedOp) && check(ops.additionalOp);
}

uintptr_t DefaultAppOpsFacade::addChangeCallback(const ValidatedAttributionSourceState& attr,
                                                 Ops ops, std::function<void(bool)> cb) {
    const auto listener = sp<OpMonitor>::make(attr, ops, std::move(cb));
    const auto reg = [&](int32_t op) {
        std::for_each(AttrSourceIter::cbegin(attr), AttrSourceIter::cend(),
                      [&listener, op](const auto& x) {
                          AppOpsManager{}.startWatchingMode(
                                  op, String16{x.packageName.value_or("").c_str()},
                                  AppOpsManager::WATCH_FOREGROUND_CHANGES, listener);
                      });
    };
    if (ops.attributedOp != AppOpsManager::OP_NONE) reg(ops.attributedOp);
    if (ops.additionalOp != AppOpsManager::OP_NONE) reg(ops.additionalOp);
    std::lock_guard l_{sMapLock};
    const auto cookie = reinterpret_cast<uintptr_t>(listener.get());
    sCbMap[cookie] = std::move(listener);
    return cookie;
}

void DefaultAppOpsFacade::removeChangeCallback(uintptr_t ptr) {
    sp<OpMonitor> monitor;
    {
        std::lock_guard l_{sMapLock};
        if (const auto iter = sCbMap.find(ptr); iter != sCbMap.end()) {
            monitor = std::move(iter->second);
            sCbMap.erase(iter);
        }
    }
    LOG_ALWAYS_FATAL_IF(monitor == nullptr, "Unexpected nullptr in cb map");
    monitor->stopListening();
}

}  // namespace android::media::permission
