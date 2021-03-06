// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/overview/overview_button_tray.h"

#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/status_area_widget_test_helper.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_types.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

OverviewButtonTray* GetTray() {
  return StatusAreaWidgetTestHelper::GetStatusAreaWidget()->
      overview_button_tray();
}

OverviewButtonTray* GetSecondaryTray() {
  return StatusAreaWidgetTestHelper::GetSecondaryStatusAreaWidget()->
      overview_button_tray();
}

}  // namespace

class OverviewButtonTrayTest : public test::AshTestBase {
 public:
  OverviewButtonTrayTest() {}
  ~OverviewButtonTrayTest() override {}

  void SetUp() override;

 protected:
  views::ImageView* GetImageView(OverviewButtonTray* tray) {
    return tray->icon_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverviewButtonTrayTest);
};

void OverviewButtonTrayTest::SetUp() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshUseFirstDisplayAsInternal);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAshEnableScreenRotationAnimation);
  AshTestBase::SetUp();
}

// Ensures that creation doesn't cause any crashes and adds the image icon.
TEST_F(OverviewButtonTrayTest, BasicConstruction) {
  EXPECT_TRUE(GetImageView(GetTray()) != NULL);
}

// Test that maximize mode toggle changes visibility.
// OverviewButtonTray should only be visible when MaximizeMode is enabled.
// By default the system should not have MaximizeMode enabled.
TEST_F(OverviewButtonTrayTest, MaximizeModeObserverOnMaximizeModeToggled) {
  ASSERT_FALSE(GetTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());

  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(GetTray()->visible());
}

// Tests that activating this control brings up window selection mode.
TEST_F(OverviewButtonTrayTest, PerformAction) {
  ASSERT_FALSE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());

  // Overview Mode only works when there is a window
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  ui::GestureEvent tap(
      0, 0, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(Shell::GetInstance()->window_selector_controller()->
      IsSelecting());
}

// Tests that a second OverviewButtonTray has been created, and only shows
// when MaximizeMode has been enabled,  when we are using multiple displays.
// By default the DisplayManger is in extended mode.
TEST_F(OverviewButtonTrayTest, DisplaysOnBothDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,200x200");
  EXPECT_FALSE(GetTray()->visible());
  EXPECT_FALSE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(GetTray()->visible());
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests if Maximize Mode is enabled before a secondary display is attached
// that the second OverviewButtonTray should be created in a visible state.
TEST_F(OverviewButtonTrayTest, SecondaryTrayCreatedVisible) {
  if (!SupportsMultipleDisplays())
    return;

  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  UpdateDisplay("400x400,200x200");
  EXPECT_TRUE(GetSecondaryTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that the tray loses visibility when a user logs out, and that it
// regains visibility when a user logs back in.
TEST_F(OverviewButtonTrayTest, VisibilityChangesForLoginStatus) {
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  SetUserLoggedIn(false);
  Shell::GetInstance()->UpdateAfterLoginStatusChange(user::LOGGED_IN_NONE);
  EXPECT_FALSE(GetTray()->visible());
  SetUserLoggedIn(true);
  SetSessionStarted(true);
  Shell::GetInstance()->UpdateAfterLoginStatusChange(user::LOGGED_IN_USER);
  EXPECT_TRUE(GetTray()->visible());
  Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
}

// Tests that the tray only renders as active while selection is ongoing. Any
// dismissal of overview mode clears the active state.
TEST_F(OverviewButtonTrayTest, ActiveStateOnlyDuringOverviewMode) {
  ASSERT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  ASSERT_FALSE(GetTray()->draw_background_as_active());

  // Overview Mode only works when there is a window
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  ui::GestureEvent tap(
      0, 0, 0, base::TimeDelta(), ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  GetTray()->PerformAction(tap);
  EXPECT_TRUE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_TRUE(GetTray()->draw_background_as_active());

  Shell::GetInstance()->window_selector_controller()->OnSelectionEnded();
  EXPECT_FALSE(
      Shell::GetInstance()->window_selector_controller()->IsSelecting());
  EXPECT_FALSE(GetTray()->draw_background_as_active());
}

// Test that when a hide animation is aborted via deletion, that the
// OverviewButton is still hidden.
TEST_F(OverviewButtonTrayTest, HideAnimationAlwaysCompletes) {
  Shell::GetInstance()
      ->maximize_mode_controller()
      ->EnableMaximizeModeWindowManager(true);

  // Long duration for hide animation, to allow it to be interrupted.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> hide_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  GetTray()->SetVisible(false);

  // ScreenRotationAnimator copies the current layers, and deletes them upon
  // completion. Allow its animation to complete first.
  scoped_ptr<ui::ScopedAnimationDurationScaleMode> rotate_duration(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ash::ScreenRotationAnimator(gfx::Display::InternalDisplayId())
      .Rotate(gfx::Display::ROTATE_270, gfx::Display::ROTATION_SOURCE_ACTIVE);

  RunAllPendingInMessageLoop();
  EXPECT_FALSE(GetTray()->visible());
}

}  // namespace ash
