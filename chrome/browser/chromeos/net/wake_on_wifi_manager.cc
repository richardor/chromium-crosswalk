// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/sys_info.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "components/gcm_driver/gcm_driver.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kWakeOnNone[] = "none";
const char kWakeOnPacket[] = "packet";
const char kWakeOnSsid[] = "ssid";
const char kWakeOnPacketAndSsid[] = "packet_and_ssid";

std::string WakeOnWifiFeatureToString(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  switch (feature) {
    case WakeOnWifiManager::WAKE_ON_NONE:
      return kWakeOnNone;
    case WakeOnWifiManager::WAKE_ON_PACKET:
      return kWakeOnPacket;
    case WakeOnWifiManager::WAKE_ON_SSID:
      return kWakeOnSsid;
    case WakeOnWifiManager::WAKE_ON_PACKET_AND_SSID:
      return kWakeOnPacketAndSsid;
  }

  NOTREACHED() << "Unknown wake on wifi feature: " << feature;
  return std::string();
}

// Weak pointer.  This class is owned by ChromeBrowserMainPartsChromeos.
WakeOnWifiManager* g_wake_on_wifi_manager = NULL;

}  // namespace

// Simple class that listens for a connection to the GCM server and passes the
// connection information down to shill.  Each profile gets its own instance of
// this class.
class WakeOnWifiManager::WakeOnPacketConnectionObserver
    : public gcm::GCMConnectionObserver {
 public:
  explicit WakeOnPacketConnectionObserver(Profile* profile)
      : profile_(profile),
        ip_endpoint_(net::IPEndPoint()) {
    gcm::GCMProfileServiceFactory::GetForProfile(profile_)
        ->driver()
        ->AddConnectionObserver(this);
  }

  ~WakeOnPacketConnectionObserver() override {
    if (!(ip_endpoint_ == net::IPEndPoint()))
      OnDisconnected();

    gcm::GCMProfileServiceFactory::GetForProfile(profile_)
        ->driver()
        ->RemoveConnectionObserver(this);
  }

  // gcm::GCMConnectionObserver overrides.

  void OnConnected(const net::IPEndPoint& ip_endpoint) override {
    ip_endpoint_ = ip_endpoint;

    NetworkHandler::Get()
        ->network_device_handler()
        ->AddWifiWakeOnPacketConnection(
            ip_endpoint_,
            base::Bind(&base::DoNothing),
            network_handler::ErrorCallback());
  }

  void OnDisconnected() override {
    if (ip_endpoint_ == net::IPEndPoint()) {
      LOG(WARNING) << "Received GCMConnectionObserver::OnDisconnected without "
                   << "a valid IPEndPoint.";
      return;
    }

    NetworkHandler::Get()
        ->network_device_handler()
        ->RemoveWifiWakeOnPacketConnection(
            ip_endpoint_,
            base::Bind(&base::DoNothing),
            network_handler::ErrorCallback());

    ip_endpoint_ = net::IPEndPoint();
  }

 private:
  Profile* profile_;
  net::IPEndPoint ip_endpoint_;

  DISALLOW_COPY_AND_ASSIGN(WakeOnPacketConnectionObserver);
};

// static
WakeOnWifiManager* WakeOnWifiManager::Get() {
  DCHECK(g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_wake_on_wifi_manager;
}

WakeOnWifiManager::WakeOnWifiManager() {
  // This class must be constructed before any users are logged in, i.e., before
  // any profiles are created or added to the ProfileManager.  Additionally,
  // IsUserLoggedIn always returns true when we are not running on a Chrome OS
  // device so this check should only run on real devices.
  CHECK(!base::SysInfo::IsRunningOnChromeOS() ||
        !LoginState::Get()->IsUserLoggedIn());
  DCHECK(!g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  g_wake_on_wifi_manager = this;

  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_ADDED,
                 content::NotificationService::AllBrowserContextsAndSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());

  NetworkHandler::Get()
      ->network_device_handler()
      ->RemoveAllWifiWakeOnPacketConnections(
          base::Bind(&base::DoNothing),
          network_handler::ErrorCallback());
}

WakeOnWifiManager::~WakeOnWifiManager() {
  DCHECK(g_wake_on_wifi_manager);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  g_wake_on_wifi_manager = NULL;
}

void WakeOnWifiManager::OnPreferenceChanged(
    WakeOnWifiManager::WakeOnWifiFeature feature) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::WiFi());
  if (!device)
    return;

  std::string feature_string(WakeOnWifiFeatureToString(feature));
  DCHECK(!feature_string.empty());

  NetworkHandler::Get()->network_device_handler()->SetDeviceProperty(
      device->path(),
      shill::kWakeOnWiFiFeaturesEnabledProperty,
      base::StringValue(feature_string),
      base::Bind(&base::DoNothing),
      network_handler::ErrorCallback());
}

void WakeOnWifiManager::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_ADDED: {
      OnProfileAdded(content::Source<Profile>(source).ptr());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      OnProfileDestroyed(content::Source<Profile>(source).ptr());
      break;
    }
    default:
      NOTREACHED();
  }
}

void WakeOnWifiManager::OnProfileAdded(Profile* profile) {
  // add will do nothing if |profile| already exists in |connection_observers_|.
  connection_observers_.add(
      profile,
      make_scoped_ptr(
          new WakeOnWifiManager::WakeOnPacketConnectionObserver(profile)));
}

void WakeOnWifiManager::OnProfileDestroyed(Profile* profile) {
  connection_observers_.erase(profile);
}

}  // namespace chromeos
