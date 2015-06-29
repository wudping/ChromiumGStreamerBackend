// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_
#define EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/shared_memory.h"
#include "base/scoped_observer.h"
#include "extensions/common/user_script.h"
#include "extensions/renderer/script_injection.h"
#include "extensions/renderer/user_script_set_manager.h"

struct ExtensionMsg_ExecuteCode_Params;

namespace content {
class RenderFrame;
}

namespace extensions {
class Extension;
class ExtensionSet;

// The ScriptInjectionManager manages extensions injecting scripts into frames
// via both content/user scripts and tabs.executeScript(). It is responsible for
// maintaining any pending injections awaiting permission or the appropriate
// load point, and injecting them when ready.
class ScriptInjectionManager : public UserScriptSetManager::Observer {
 public:
  ScriptInjectionManager(const ExtensionSet* extensions,
                         UserScriptSetManager* user_script_set_manager);
  virtual ~ScriptInjectionManager();

  // Notifies that a new render view has been created.
  void OnRenderFrameCreated(content::RenderFrame* render_frame);

  // Removes pending injections of the unloaded extension.
  void OnExtensionUnloaded(const std::string& extension_id);

 private:
  // A RenderFrameObserver implementation which watches the various render
  // frames in order to notify the ScriptInjectionManager of different
  // document load states and IPCs.
  class RFOHelper;

  using FrameStatusMap =
      std::map<content::RenderFrame*, UserScript::RunLocation>;

  // Notifies that an injection has been finished.
  void OnInjectionFinished(ScriptInjection* injection);

  // UserScriptSetManager::Observer implementation.
  void OnUserScriptsUpdated(const std::set<HostID>& changed_hosts,
                            const std::vector<UserScript*>& scripts) override;

  // Notifies that an RFOHelper should be removed.
  void RemoveObserver(RFOHelper* helper);

  // Invalidate any pending tasks associated with |frame|.
  void InvalidateForFrame(content::RenderFrame* frame);

  // Starts the process to inject appropriate scripts into |frame|.
  void StartInjectScripts(content::RenderFrame* frame,
                          UserScript::RunLocation run_location);

  // Actually injects the scripts into |frame|.
  void InjectScripts(content::RenderFrame* frame,
                     UserScript::RunLocation run_location);

  // Try to inject and store injection if it has not finished.
  void TryToInject(scoped_ptr<ScriptInjection> injection,
                   UserScript::RunLocation run_location,
                   ScriptsRunInfo* scripts_run_info);

  // Handle the ExecuteCode extension message.
  void HandleExecuteCode(const ExtensionMsg_ExecuteCode_Params& params,
                         content::RenderFrame* render_frame);

  // Handle the ExecuteDeclarativeScript extension message.
  void HandleExecuteDeclarativeScript(content::RenderFrame* web_frame,
                                      int tab_id,
                                      const ExtensionId& extension_id,
                                      int script_id,
                                      const GURL& url);

  // Handle the GrantInjectionPermission extension message.
  void HandlePermitScriptInjection(int64 request_id);

  // Extensions metadata, owned by Dispatcher (which owns this object).
  const ExtensionSet* extensions_;

  // The map of active web frames to their corresponding statuses. The
  // RunLocation of the frame corresponds to the last location that has ran.
  FrameStatusMap frame_statuses_;

  // The collection of RFOHelpers.
  ScopedVector<RFOHelper> rfo_helpers_;

  // The set of UserScripts associated with extensions. Owned by the Dispatcher.
  UserScriptSetManager* user_script_set_manager_;

  // Pending injections which are waiting for either the proper run location or
  // user consent.
  ScopedVector<ScriptInjection> pending_injections_;

  // Running injections which are waiting for async callbacks from blink.
  ScopedVector<ScriptInjection> running_injections_;

  ScopedObserver<UserScriptSetManager, UserScriptSetManager::Observer>
      user_script_set_manager_observer_;

  DISALLOW_COPY_AND_ASSIGN(ScriptInjectionManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SCRIPT_INJECTION_MANAGER_H_
