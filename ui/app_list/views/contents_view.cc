// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/contents_view.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/views/app_list_folder_view.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/apps_container_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/app_list/views/custom_launcher_page_view.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/search_result_list_view.h"
#include "ui/app_list/views/search_result_page_view.h"
#include "ui/app_list/views/search_result_tile_item_list_view.h"
#include "ui/app_list/views/start_page_view.h"
#include "ui/events/event.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace app_list {

ContentsView::ContentsView(AppListMainView* app_list_main_view)
    : apps_container_view_(nullptr),
      search_results_page_view_(nullptr),
      start_page_view_(nullptr),
      custom_page_view_(nullptr),
      app_list_main_view_(app_list_main_view),
      page_before_search_(0) {
  pagination_model_.SetTransitionDurations(kPageTransitionDurationInMs,
                                           kOverscrollPageTransitionDurationMs);
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

void ContentsView::Init(AppListModel* model) {
  DCHECK(model);

  AppListViewDelegate* view_delegate = app_list_main_view_->view_delegate();

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    std::vector<views::View*> custom_page_views =
        view_delegate->CreateCustomPageWebViews(GetLocalBounds().size());
    for (std::vector<views::View*>::const_iterator it =
             custom_page_views.begin();
         it != custom_page_views.end();
         ++it) {
      // Only the first launcher page is considered to represent
      // STATE_CUSTOM_LAUNCHER_PAGE.
      if (it == custom_page_views.begin()) {
        custom_page_view_ = new CustomLauncherPageView(*it);

        AddLauncherPage(custom_page_view_,
                        AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
      } else {
        AddLauncherPage(new CustomLauncherPageView(*it));
      }
    }

    // Start page.
    start_page_view_ = new StartPageView(app_list_main_view_, view_delegate);
    AddLauncherPage(start_page_view_, AppListModel::STATE_START);
  }

  // Search results UI.
  search_results_page_view_ = new SearchResultPageView();

  AppListModel::SearchResults* results = view_delegate->GetModel()->results();
  search_results_page_view_->AddSearchResultContainerView(
      results, new SearchResultListView(app_list_main_view_, view_delegate));

  if (app_list::switches::IsExperimentalAppListEnabled()) {
    search_results_page_view_->AddSearchResultContainerView(
        results,
        new SearchResultTileItemListView(GetSearchBoxView()->search_box()));
  }
  AddLauncherPage(search_results_page_view_,
                  AppListModel::STATE_SEARCH_RESULTS);

  apps_container_view_ = new AppsContainerView(app_list_main_view_, model);

  AddLauncherPage(apps_container_view_, AppListModel::STATE_APPS);

  int initial_page_index = app_list::switches::IsExperimentalAppListEnabled()
                               ? GetPageIndexForState(AppListModel::STATE_START)
                               : GetPageIndexForState(AppListModel::STATE_APPS);
  DCHECK_GE(initial_page_index, 0);

  page_before_search_ = initial_page_index;
  // Must only call SetTotalPages once all the launcher pages have been added
  // (as it will trigger a SelectedPageChanged call).
  pagination_model_.SetTotalPages(app_list_pages_.size());

  // Page 0 is selected by SetTotalPages and needs to be 'hidden' when selecting
  // the initial page.
  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  pagination_model_.SelectPage(initial_page_index, false);

  ActivePageChanged();
}

void ContentsView::CancelDrag() {
  if (apps_container_view_->apps_grid_view()->has_dragged_view())
    apps_container_view_->apps_grid_view()->EndDrag(true);
  if (apps_container_view_->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_view()) {
    apps_container_view_->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  apps_container_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void ContentsView::SetActiveState(AppListModel::State state) {
  SetActiveState(state, true);
}

void ContentsView::SetActiveState(AppListModel::State state, bool animate) {
  if (IsStateActive(state))
    return;

  SetActiveStateInternal(GetPageIndexForState(state), false, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

AppListModel::State ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(AppListModel::State state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(AppListModel::State state) const {
  // Find the index of the view corresponding to the given state.
  std::map<AppListModel::State, int>::const_iterator it =
      state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

AppListModel::State ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, AppListModel::State>::const_iterator it =
      view_to_state_.find(index);
  if (it == view_to_state_.end())
    return AppListModel::INVALID_STATE;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

void ContentsView::SetActiveStateInternal(int page_index,
                                          bool show_search_results,
                                          bool animate) {
  if (!GetPageView(page_index)->visible())
    return;

  if (!show_search_results)
    page_before_search_ = page_index;

  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  // Start animating to the new page.
  pagination_model_.SelectPage(page_index, animate);
  ActivePageChanged();

  if (!animate)
    Layout();
}

void ContentsView::ActivePageChanged() {
  AppListModel::State state = AppListModel::INVALID_STATE;

  std::map<int, AppListModel::State>::const_iterator it =
      view_to_state_.find(GetActivePageIndex());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_pages_[GetActivePageIndex()]->OnWillBeShown();

  app_list_main_view_->model()->SetState(state);

  if (switches::IsExperimentalAppListEnabled()) {
    DCHECK(start_page_view_);

    // Set the visibility of the search box's back button.
    app_list_main_view_->search_box_view()->back_button()->SetVisible(
        state != AppListModel::STATE_START);
    app_list_main_view_->search_box_view()->Layout();

    // Whenever the page changes, the custom launcher page is considered to have
    // been reset.
    app_list_main_view_->model()->ClearCustomLauncherPageSubpages();
  }

  app_list_main_view_->search_box_view()->ResetTabFocus(false);
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page = GetPageIndexForState(AppListModel::STATE_SEARCH_RESULTS);
  DCHECK_GE(search_page, 0);

  SetActiveStateInternal(show ? search_page : page_before_search_, show, true);
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(AppListModel::STATE_SEARCH_RESULTS);
}

void ContentsView::NotifyCustomLauncherPageAnimationChanged(double progress,
                                                            int current_page,
                                                            int target_page) {
  int custom_launcher_page_index =
      GetPageIndexForState(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE);
  if (custom_launcher_page_index == target_page) {
    app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
        progress);
  } else if (custom_launcher_page_index == current_page) {
    app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
        1 - progress);
  }
}

void ContentsView::UpdatePageBounds() {
  // The bounds calculations will potentially be mid-transition (depending on
  // the state of the PaginationModel).
  int current_page = std::max(0, pagination_model_.selected_page());
  int target_page = current_page;
  double progress = 1;
  if (pagination_model_.has_transition()) {
    const PaginationModel::Transition& transition =
        pagination_model_.transition();
    if (pagination_model_.is_valid_page(transition.target_page)) {
      target_page = transition.target_page;
      progress = transition.progress;
    }
  }

  NotifyCustomLauncherPageAnimationChanged(progress, current_page, target_page);

  AppListModel::State current_state = GetStateForPageIndex(current_page);
  AppListModel::State target_state = GetStateForPageIndex(target_page);

  // Update app list pages.
  for (AppListPage* page : app_list_pages_) {
    page->OnAnimationUpdated(progress, current_state, target_state);

    gfx::Rect to_rect = page->GetPageBoundsForState(target_state);
    gfx::Rect from_rect = page->GetPageBoundsForState(current_state);
    if (from_rect == to_rect)
      continue;

    // Animate linearly (the PaginationModel handles easing).
    gfx::Rect bounds(
        gfx::Tween::RectValueBetween(progress, from_rect, to_rect));

    page->SetBoundsRect(bounds);
  }

  // Update the search box.
  UpdateSearchBox(progress, current_state, target_state);
}

void ContentsView::UpdateSearchBox(double progress,
                                   AppListModel::State current_state,
                                   AppListModel::State target_state) {
  AppListPage* from_page = GetPageView(GetPageIndexForState(current_state));
  AppListPage* to_page = GetPageView(GetPageIndexForState(target_state));

  SearchBoxView* search_box = GetSearchBoxView();

  gfx::Rect search_box_from(from_page->GetSearchBoxBounds());
  gfx::Rect search_box_to(to_page->GetSearchBoxBounds());
  gfx::Rect search_box_rect =
      gfx::Tween::RectValueBetween(progress, search_box_from, search_box_to);

  int original_z_height = from_page->GetSearchBoxZHeight();
  int target_z_height = to_page->GetSearchBoxZHeight();

  if (original_z_height != target_z_height) {
    gfx::ShadowValue original_shadow = GetShadowForZHeight(original_z_height);
    gfx::ShadowValue target_shadow = GetShadowForZHeight(target_z_height);

    gfx::Vector2d offset(gfx::Tween::LinearIntValueBetween(
                             progress, original_shadow.x(), target_shadow.x()),
                         gfx::Tween::LinearIntValueBetween(
                             progress, original_shadow.y(), target_shadow.y()));
    search_box->SetShadow(gfx::ShadowValue(
        offset, gfx::Tween::LinearIntValueBetween(
                    progress, original_shadow.blur(), target_shadow.blur()),
        gfx::Tween::ColorValueBetween(progress, original_shadow.color(),
                                      target_shadow.color())));
  }
  search_box->GetWidget()->SetBounds(
      search_box->GetViewBoundsForSearchBoxContentsBounds(
          ConvertRectToWidget(search_box_rect)));
}

PaginationModel* ContentsView::GetAppsPaginationModel() {
  return apps_container_view_->apps_grid_view()->pagination_model();
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  apps_container_view_->ShowActiveFolder(item);
}

void ContentsView::Prerender() {
  apps_container_view_->apps_grid_view()->Prerender();
}

AppListPage* ContentsView::GetPageView(int index) const {
  DCHECK_GT(static_cast<int>(app_list_pages_.size()), index);
  return app_list_pages_[index];
}

SearchBoxView* ContentsView::GetSearchBoxView() const {
  return app_list_main_view_->search_box_view();
}

int ContentsView::AddLauncherPage(AppListPage* view) {
  view->set_contents_view(this);
  AddChildView(view);
  app_list_pages_.push_back(view);
  return app_list_pages_.size() - 1;
}

int ContentsView::AddLauncherPage(AppListPage* view,
                                  AppListModel::State state) {
  int page_index = AddLauncherPage(view);
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
  return page_index;
}

gfx::Rect ContentsView::GetDefaultSearchBoxBounds() const {
  gfx::Rect search_box_bounds(0, 0, GetDefaultContentsSize().width(),
                              GetSearchBoxView()->GetPreferredSize().height());
  if (switches::IsExperimentalAppListEnabled()) {
    search_box_bounds.set_y(kExperimentalSearchBoxPadding);
    search_box_bounds.Inset(kExperimentalSearchBoxPadding, 0);
  }
  return search_box_bounds;
}

gfx::Rect ContentsView::GetSearchBoxBoundsForState(
    AppListModel::State state) const {
  AppListPage* page = GetPageView(GetPageIndexForState(state));
  return page->GetSearchBoxBounds();
}

gfx::Rect ContentsView::GetDefaultContentsBounds() const {
  gfx::Rect bounds(gfx::Point(0, GetDefaultSearchBoxBounds().bottom()),
                   GetDefaultContentsSize());
  return bounds;
}

bool ContentsView::Back() {
  AppListModel::State state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case AppListModel::STATE_START:
      // Close the app list when Back() is called from the start page.
      return false;
    case AppListModel::STATE_CUSTOM_LAUNCHER_PAGE:
      if (app_list_main_view_->model()->PopCustomLauncherPageSubpage())
        app_list_main_view_->view_delegate()->CustomLauncherPagePopSubpage();
      else
        SetActiveState(AppListModel::STATE_START);
      break;
    case AppListModel::STATE_APPS:
      if (apps_container_view_->IsInFolderView())
        apps_container_view_->app_list_folder_view()->CloseFolderPage();
      else
        SetActiveState(AppListModel::STATE_START);
      break;
    case AppListModel::STATE_SEARCH_RESULTS:
      GetSearchBoxView()->ClearSearch();
      ShowSearchResults(false);
      break;
    case AppListModel::INVALID_STATE:  // Falls through.
      NOTREACHED();
      break;
  }
  return true;
}

gfx::Size ContentsView::GetDefaultContentsSize() const {
  return apps_container_view_->apps_grid_view()->GetPreferredSize();
}

gfx::Size ContentsView::GetPreferredSize() const {
  gfx::Rect search_box_bounds = GetDefaultSearchBoxBounds();
  gfx::Rect default_contents_bounds = GetDefaultContentsBounds();
  gfx::Vector2d bottom_right =
      search_box_bounds.bottom_right().OffsetFromOrigin();
  bottom_right.SetToMax(
      default_contents_bounds.bottom_right().OffsetFromOrigin());
  return gfx::Size(bottom_right.x(), bottom_right.y());
}

void ContentsView::Layout() {
  // Immediately finish all current animations.
  pagination_model_.FinishAnimation();

  double progress =
      IsStateActive(AppListModel::STATE_CUSTOM_LAUNCHER_PAGE) ? 1 : 0;

  // Notify the custom launcher page that the active page has changed.
  app_list_main_view_->view_delegate()->CustomLauncherPageAnimationChanged(
      progress);

  if (GetContentsBounds().IsEmpty())
    return;

  for (AppListPage* page : app_list_pages_) {
    page->SetBoundsRect(page->GetPageBoundsForState(GetActiveState()));
  }

  // The search box is contained in a widget so set the bounds of the widget
  // rather than the SearchBoxView. In athena, the search box widget will be the
  // same as the app list widget so don't move it.
  views::Widget* search_box_widget = GetSearchBoxView()->GetWidget();
  if (search_box_widget && search_box_widget != GetWidget()) {
    gfx::Rect search_box_bounds = GetSearchBoxBoundsForState(GetActiveState());
    search_box_widget->SetBounds(ConvertRectToWidget(
        GetSearchBoxView()->GetViewBoundsForSearchBoxContentsBounds(
            search_box_bounds)));
  }
}

bool ContentsView::OnKeyPressed(const ui::KeyEvent& event) {
  bool handled = app_list_pages_[GetActivePageIndex()]->OnKeyPressed(event);

  if (!handled) {
    if (event.key_code() == ui::VKEY_TAB && event.IsShiftDown()) {
      GetSearchBoxView()->MoveTabFocus(true);
      handled = true;
    }
  }

  return handled;
}

void ContentsView::TotalPagesChanged() {
}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0)
    app_list_pages_[old_selected]->OnHidden();

  if (new_selected >= 0)
    app_list_pages_[new_selected]->OnShown();
}

void ContentsView::TransitionStarted() {
}

void ContentsView::TransitionChanged() {
  UpdatePageBounds();
}

}  // namespace app_list
