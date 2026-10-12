/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * libexynoscamera3 was built against the Android 12 libui, where android::Fence
 * has a non-virtual destructor and is { LightRefBase::mCount, base::unique_fd }.
 * Since Android 13 ~Fence() is virtual, so libui's Fence starts with a vtable
 * pointer and is twice as large. The blob allocates the old size, constructs it
 * with libui, then refcounts and destroys it with its own inlined code, which
 * touches the wrong fields: the fence is never closed and one sync_file fd
 * leaks per frame.
 *
 * Provide the two Fence methods the blob imports, with the layout it expects.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <atomic>

#include <android-base/unique_fd.h>
#include <sync/sync.h>
#include <utils/Errors.h>

namespace android {

class Fence {
  public:
    explicit Fence(int fenceFd);
    status_t wait(int timeout);

  private:
    friend struct FenceLayout;

    mutable std::atomic<int32_t> mCount;  // LightRefBase<Fence>
    base::unique_fd mFenceFd;
};

struct FenceLayout {
    static_assert(sizeof(Fence) == 8);
    static_assert(offsetof(Fence, mCount) == 0);
    static_assert(offsetof(Fence, mFenceFd) == 4);
};

Fence::Fence(int fenceFd) : mCount(0), mFenceFd(fenceFd) {}

status_t Fence::wait(int timeout) {
    if (mFenceFd == -1) {
        return NO_ERROR;
    }
    int err = sync_wait(mFenceFd, timeout);
    return err < 0 ? -errno : status_t(NO_ERROR);
}

}  // namespace android
