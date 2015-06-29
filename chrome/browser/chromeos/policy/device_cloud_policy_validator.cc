// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_validator.h"

#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

template class CloudPolicyValidator<em::ChromeDeviceSettingsProto>;

}  // namespace policy
