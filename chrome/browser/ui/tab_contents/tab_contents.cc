// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "chrome/browser/autofill/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_tab_helper.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/external_protocol/external_protocol_observer.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/load_time_stats.h"
#include "chrome/browser/omnibox_search_hint.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/pepper_broker_observer.h"
#include "chrome/browser/plugins/plugin_observer.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/printing/print_preview_message_handler.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_tab_observer.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ssl/ssl_tab_helper.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/alternate_error_tab_observer.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/hung_plugin_tab_helper.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/metro_pin_tab_helper.h"
#include "chrome/browser/ui/pdf/pdf_tab_helper.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/ui/snapshot_tab_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/tab_contents_synced_tab_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/thumbnail_support.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#endif

using content::WebContents;

namespace {

const char kTabContentsUserDataKey[] = "TabContentsUserData";

class TabContentsUserData : public base::SupportsUserData::Data {
 public:
  explicit TabContentsUserData(TabContents* tab_contents)
      : tab_contents_(tab_contents) {}
  virtual ~TabContentsUserData() {}
  TabContents* tab_contents() { return tab_contents_; }

 private:
  TabContents* tab_contents_;  // unowned
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabContents, public:

// static
TabContents* TabContents::Factory::CreateTabContents(WebContents* contents) {
  return new TabContents(contents);
}

// static
TabContents* TabContents::Factory::CloneTabContents(TabContents* contents) {
  return contents->Clone();
}

TabContents::TabContents(WebContents* contents)
    : content::WebContentsObserver(contents),
      in_destructor_(false),
      web_contents_(contents) {
  DCHECK(contents);
  DCHECK(!FromWebContents(contents));

  chrome::SetViewType(contents, chrome::VIEW_TYPE_TAB_CONTENTS);

  // Stash this in the WebContents.
  contents->SetUserData(&kTabContentsUserDataKey,
                        new TabContentsUserData(this));

  // Create the tab helpers.

  // SessionTabHelper comes first because it sets up the tab ID, and other
  // helpers may rely on that.
  SessionTabHelper::CreateForWebContents(contents);

  AlternateErrorPageTabObserver::CreateForWebContents(contents);
  autocomplete_history_manager_.reset(new AutocompleteHistoryManager(contents));
  autofill_delegate_.reset(new TabAutofillManagerDelegate(this));
  autofill_manager_ = new AutofillManager(autofill_delegate_.get(), this);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kExternalAutofillPopup)) {
    autofill_external_delegate_.reset(
        AutofillExternalDelegate::Create(this, autofill_manager_.get()));
    autofill_manager_->SetExternalDelegate(autofill_external_delegate_.get());
    autocomplete_history_manager_->SetExternalDelegate(
        autofill_external_delegate_.get());
  }
  BlockedContentTabHelper::CreateForWebContents(contents);
  BookmarkTabHelper::CreateForWebContents(contents);
  chrome_browser_net::LoadTimeStatsTabHelper::CreateForWebContents(contents);
  constrained_window_tab_helper_.reset(new ConstrainedWindowTabHelper(this));
  content_settings_.reset(new TabSpecificContentSettings(contents));
  CoreTabHelper::CreateForWebContents(contents);
  extensions::TabHelper::CreateForWebContents(contents);
  extensions::WebNavigationTabObserver::CreateForWebContents(contents);
  ExternalProtocolObserver::CreateForWebContents(contents);
  favicon_tab_helper_.reset(new FaviconTabHelper(contents));
  find_tab_helper_.reset(new FindTabHelper(contents));
  history_tab_helper_.reset(new HistoryTabHelper(contents));
  HungPluginTabHelper::CreateForWebContents(contents);
  infobar_tab_helper_.reset(new InfoBarTabHelper(contents));
  MetroPinTabHelper::CreateForWebContents(contents);
  password_manager_delegate_.reset(new PasswordManagerDelegateImpl(this));
  password_manager_.reset(
      new PasswordManager(contents, password_manager_delegate_.get()));
  PepperBrokerObserver::CreateForWebContents(contents);
  PluginObserver::CreateForWebContents(contents);
  prefs_tab_helper_.reset(new PrefsTabHelper(contents));
  prerender_tab_helper_.reset(new prerender::PrerenderTabHelper(this));
  SearchEngineTabHelper::CreateForWebContents(contents);
  chrome::search::SearchTabHelper::CreateForWebContents(contents);
  SnapshotTabHelper::CreateForWebContents(contents);
  SSLTabHelper::CreateForWebContents(contents);
  synced_tab_delegate_.reset(new TabContentsSyncedTabDelegate(this));
  translate_tab_helper_.reset(new TranslateTabHelper(contents));
  ZoomController::CreateForWebContents(contents);

#if defined(ENABLE_AUTOMATION)
  automation_tab_helper_.reset(new AutomationTabHelper(contents));
#endif

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  captive_portal::CaptivePortalTabHelper::CreateForWebContents(contents);
#endif

#if !defined(OS_ANDROID)
  if (OmniboxSearchHint::IsEnabled(profile()))
    OmniboxSearchHint::CreateForWebContents(contents);
  PDFTabHelper::CreateForWebContents(contents);
  SadTabHelper::CreateForWebContents(contents);
  WebIntentPickerController::CreateForWebContents(contents);
#endif

  navigation_metrics_recorder_.reset(new NavigationMetricsRecorder(contents));
  safe_browsing_tab_observer_.reset(
      new safe_browsing::SafeBrowsingTabObserver(this));

#if defined(ENABLE_PRINTING)
  printing::PrintPreviewMessageHandler::CreateForWebContents(contents);
  printing::PrintViewManager::CreateForWebContents(contents);
#endif

  // Start the in-browser thumbnailing if the feature is enabled.
  if (ShouldEnableInBrowserThumbnailing()) {
    thumbnail_generator_.reset(new ThumbnailGenerator);
    thumbnail_generator_->StartThumbnailing(web_contents_.get());
  }

#if defined(ENABLE_ONE_CLICK_SIGNIN)
  // If this is not an incognito window, setup to handle one-click login.
  // We don't want to check that the profile is already connected at this time
  // because the connected state may change while this tab is open.  Having a
  // one-click signin helper attached does not cause problems if the profile
  // happens to be already connected.
  if (OneClickSigninHelper::CanOffer(contents, "", false))
      OneClickSigninHelper::CreateForWebContents(contents);
#endif
}

TabContents::~TabContents() {
  in_destructor_ = true;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      content::Source<TabContents>(this),
      content::NotificationService::NoDetails());

  // Need to tear down infobars before the WebContents goes away.
  // TODO(avi): Can we get this handled by the tab helper itself?
  infobar_tab_helper_.reset();
}

TabContents* TabContents::Clone() {
  WebContents* new_web_contents = web_contents()->Clone();
  return new TabContents(new_web_contents);
}

// static
TabContents* TabContents::FromWebContents(WebContents* contents) {
  TabContentsUserData* user_data = static_cast<TabContentsUserData*>(
      contents->GetUserData(&kTabContentsUserDataKey));

  return user_data ? user_data->tab_contents() : NULL;
}

// static
const TabContents* TabContents::FromWebContents(const WebContents* contents) {
  TabContentsUserData* user_data = static_cast<TabContentsUserData*>(
      contents->GetUserData(&kTabContentsUserDataKey));

  return user_data ? user_data->tab_contents() : NULL;
}

WebContents* TabContents::web_contents() const {
  return web_contents_.get();
}

Profile* TabContents::profile() const {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsObserver overrides

void TabContents::WebContentsDestroyed(WebContents* tab) {
  // Destruction of the WebContents should only be done by us from our
  // destructor. Otherwise it's very likely we (or one of the helpers we own)
  // will attempt to access the WebContents and we'll crash.
  DCHECK(in_destructor_);
}
