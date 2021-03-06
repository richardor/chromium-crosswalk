// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!HTMLElement} element
 * @constructor
 * @struct
 */
function GearMenu(element) {
  /**
   * @type {!HTMLMenuItemElement}
   * @const
   */
  this.syncButton = /** @type {!HTMLMenuItemElement} */
      (queryRequiredElement(element, '#gear-menu-drive-sync-settings'));

  /**
   * @type {!HTMLMenuItemElement}
   * @const
   */
  this.hostedButton = /** @type {!HTMLMenuItemElement} */
      (queryRequiredElement(element, '#gear-menu-drive-hosted-settings'));

  /**
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.volumeSpaceInfo_ = queryRequiredElement(element, '#volume-space-info');

  /**
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.volumeSpaceInfoSeparator_ =
      queryRequiredElement(element, '#volume-space-info-separator');

  /**
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.volumeSpaceInfoLabel_ =
      queryRequiredElement(element, '#volume-space-info-label');

  /**
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.volumeSpaceInnerBar_ =
      queryRequiredElement(element, '#volume-space-info-bar');

  /**
   * @type {!HTMLElement}
   * @const
   * @private
   */
  this.volumeSpaceOuterBar_ = assertInstanceof(
      this.volumeSpaceInnerBar_.parentElement,
      HTMLElement);

  /**
   * Volume space info.
   * @type {Promise.<MountPointSizeStats>}
   * @private
   */
  this.spaceInfoPromise_ = null;

  // Initialize attributes.
  element.menuItemSelector = 'cr-menu-item, hr';
  this.syncButton.checkable = true;
  this.hostedButton.checkable = true;
}

/**
 * @param {Promise.<MountPointSizeStats>} spaceInfoPromise Promise to be
 *     fulfilled with space info.
 * @param {boolean} showLoadingCaption Whether show loading caption or not.
 */
GearMenu.prototype.setSpaceInfo = function(
    spaceInfoPromise, showLoadingCaption) {
  this.spaceInfoPromise_ = spaceInfoPromise;

  if (!spaceInfoPromise) {
    this.volumeSpaceInfo_.hidden = true;
    this.volumeSpaceInfoSeparator_.hidden = true;
    return;
  }

  this.volumeSpaceInfo_.hidden = false;
  this.volumeSpaceInfoSeparator_.hidden = false;
  this.volumeSpaceInnerBar_.setAttribute('pending', '');
  if (showLoadingCaption) {
    this.volumeSpaceInfoLabel_.innerText = str('WAITING_FOR_SPACE_INFO');
    this.volumeSpaceInnerBar_.style.width = '100%';
  }

  spaceInfoPromise.then(function(spaceInfo) {
    if (this.spaceInfoPromise_ != spaceInfoPromise)
      return;
    this.volumeSpaceInnerBar_.removeAttribute('pending');
    if (spaceInfo) {
      var sizeStr = util.bytesToString(spaceInfo.remainingSize);
      this.volumeSpaceInfoLabel_.textContent = strf('SPACE_AVAILABLE', sizeStr);

      var usedSpace = spaceInfo.totalSize - spaceInfo.remainingSize;
      this.volumeSpaceInnerBar_.style.width =
          (100 * usedSpace / spaceInfo.totalSize) + '%';

      this.volumeSpaceOuterBar_.hidden = false;
    } else {
      this.volumeSpaceOuterBar_.hidden = true;
      this.volumeSpaceInfoLabel_.textContent = str('FAILED_SPACE_INFO');
    }
  }.bind(this));
};
