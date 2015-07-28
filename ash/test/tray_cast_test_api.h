// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TRAY_CAST_TEST_API_H_
#define ASH_TEST_TRAY_CAST_TEST_API_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/cast/tray_cast.h"

namespace ash {

class TrayCastTestAPI {
 public:
  explicit TrayCastTestAPI(TrayCast* tray_cast);
  ~TrayCastTestAPI();

  bool IsTrayInitialized() const;
  bool IsTrayVisible() const;

  // IsTrayCastViewVisible returns true if the active casting view is
  // visible, ie, the TrayCast believes we are casting.
  // IsTraySelectViewVisible returns true when the view for selecting a
  // receiver is active, ie, the TrayCast believes we are not casting.
  bool IsTrayCastViewVisible() const;
  bool IsTraySelectViewVisible() const;

  // Start a new cast to the given receiver.
  void StartCast(const std::string& receiver_id);
  void StopCast();

  // Exposed callback to update the casting state. The test code needs to call
  // this function manually, as there is no actual casting going on. In a real
  // environment, this method is invoked by the casting system in Chrome.
  void OnCastingSessionStartedOrStopped(bool is_casting);

 private:
  bool IsViewDrawn(TrayCast::ChildViewId id) const;

  // Not owned.
  TrayCast* tray_cast_;

  DISALLOW_COPY_AND_ASSIGN(TrayCastTestAPI);
};

}  // namespace ash

#endif  // ASH_TEST_TRAY_CAST_TEST_API_H_
