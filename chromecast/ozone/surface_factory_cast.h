// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_OZONE_SURFACE_FACTORY_CAST_H_
#define CHROMECAST_OZONE_SURFACE_FACTORY_CAST_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace chromecast {
namespace ozone {

class CastEglPlatform;

// SurfaceFactoryOzone implementation for OzonePlatformCast.
class SurfaceFactoryCast : public ui::SurfaceFactoryOzone {
 public:
  explicit SurfaceFactoryCast(scoped_ptr<CastEglPlatform> egl_platform);
  ~SurfaceFactoryCast() override;

  // ui::SurfaceFactoryOzone implementation:
  intptr_t GetNativeDisplay() override;
  scoped_ptr<ui::SurfaceOzoneEGL> CreateEGLSurfaceForWidget(
      gfx::AcceleratedWidget widget) override;
  const int32* GetEGLSurfaceProperties(const int32* desired_list) override;
  bool LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) override;

  void SetToRelinquishDisplay(const base::Closure& callback);
  intptr_t GetNativeWindow();
  bool ResizeDisplay(gfx::Size viewport_size);
  void ChildDestroyed();
  void SendRelinquishResponse();

 private:
  enum HardwareState { kUninitialized, kInitialized, kFailed };

  // Window is destroyed if both SetToDestroyEGLDisplay()
  // and destructor are called (in either order) before the next
  // SurfaceOzoneEglCast is created in order to preserve the
  // window across surface creation whenever possible.
  enum DestroyWindowPendingState {
    kNoDestroyPending = 0,      // Surface does not exist
    kSurfaceExists,             // surface and window both exist
    kWindowDestroyPending,      // Relinquish before surface Destroy
    kSurfaceDestroyedRecently,  // surface Destroy before Relinquish
  };

  void CreateDisplayTypeAndWindowIfNeeded();
  void DestroyDisplayTypeAndWindow();
  void InitializeHardware();
  void ShutdownHardware();

  HardwareState state_;
  DestroyWindowPendingState destroy_window_pending_state_;
  base::Closure relinquish_display_callback_;
  intptr_t display_type_;
  intptr_t window_;
  const gfx::Size default_display_size_;
  gfx::Size display_size_;
  gfx::Size new_display_size_;
  scoped_ptr<CastEglPlatform> egl_platform_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryCast);
};

}  // namespace ozone
}  // namespace chromecast

#endif  // CHROMECAST_OZONE_SURFACE_FACTORY_CAST_H_
