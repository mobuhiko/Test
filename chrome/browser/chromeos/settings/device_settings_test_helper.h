// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/session_manager_client.h"

namespace chromeos {

// A helper class for tests mocking out session_manager's device settings
// interface. The pattern is to initialize DeviceSettingsService with the helper
// for the SessionManagerClient pointer. The helper records calls made by
// DeviceSettingsService. The test can then verify state, after which it should
// call one of the Flush() variants that will resume processing.
class DeviceSettingsTestHelper : public SessionManagerClient {
 public:
  // Wraps a device settings service instance for testing.
  DeviceSettingsTestHelper();
  virtual ~DeviceSettingsTestHelper();

  // Flushes operations on the current message loop and the blocking pool.
  void FlushLoops();

  // Runs all pending store callbacks.
  void FlushStore();

  // Runs all pending retrieve callbacks.
  void FlushRetrieve();

  // Flushes all pending operations.
  void Flush();

  bool store_result() {
    return store_result_;
  }
  void set_store_result(bool store_result) {
    store_result_ = store_result;
  }

  const std::string& policy_blob() {
    return policy_blob_;
  }
  void set_policy_blob(const std::string& policy_blob) {
    policy_blob_ = policy_blob;
  }

  // SessionManagerClient:
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool HasObserver(Observer* observer) OVERRIDE;
  virtual void EmitLoginPromptReady() OVERRIDE;
  virtual void EmitLoginPromptVisible() OVERRIDE;
  virtual void RestartJob(int pid, const std::string& command_line) OVERRIDE;
  virtual void RestartEntd() OVERRIDE;
  virtual void StartSession(const std::string& user_email) OVERRIDE;
  virtual void StopSession() OVERRIDE;
  virtual void StartDeviceWipe() OVERRIDE;
  virtual void RequestLockScreen() OVERRIDE;
  virtual void NotifyLockScreenShown() OVERRIDE;
  virtual void RequestUnlockScreen() OVERRIDE;
  virtual void NotifyLockScreenDismissed() OVERRIDE;
  virtual bool GetIsScreenLocked() OVERRIDE;
  virtual void RetrieveDevicePolicy(
      const RetrievePolicyCallback& callback) OVERRIDE;
  virtual void RetrieveUserPolicy(
      const RetrievePolicyCallback& callback) OVERRIDE;
  virtual void StoreDevicePolicy(const std::string& policy_blob,
                                 const StorePolicyCallback& callback) OVERRIDE;
  virtual void StoreUserPolicy(const std::string& policy_blob,
                               const StorePolicyCallback& callback) OVERRIDE;

 private:
  bool store_result_;
  std::string policy_blob_;

  std::vector<StorePolicyCallback> store_callbacks_;
  std::vector<RetrievePolicyCallback> retrieve_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsTestHelper);
};

// Wraps the singleton device settings and initializes it to the point where it
// reports OWNERSHIP_NONE for the ownership status.
class ScopedDeviceSettingsTestHelper : public DeviceSettingsTestHelper {
 public:
  ScopedDeviceSettingsTestHelper();
  virtual ~ScopedDeviceSettingsTestHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedDeviceSettingsTestHelper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SETTINGS_DEVICE_SETTINGS_TEST_HELPER_H_
