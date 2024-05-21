// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/favicon_source.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/url_constants.h"
#include "grit/locale_settings.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/favicon_size.h"

FaviconSource::FaviconSource(Profile* profile, IconType type)
    : DataSource(type == FAVICON ? chrome::kChromeUIFaviconHost :
                     chrome::kChromeUITouchIconHost,
                 MessageLoop::current()) {
  Init(profile, type);
}

FaviconSource::FaviconSource(Profile* profile,
                             IconType type,
                             const std::string& source_name)
    : DataSource(source_name, MessageLoop::current()) {
  Init(profile, type);
}

FaviconSource::~FaviconSource() {
}

void FaviconSource::Init(Profile* profile, IconType type) {
  profile_ = profile->GetOriginalProfile();
  icon_types_ = type == FAVICON ? history::FAVICON :
      history::TOUCH_PRECOMPOSED_ICON | history::TOUCH_ICON |
      history::FAVICON;
}

void FaviconSource::StartDataRequest(const std::string& path,
                                     bool is_incognito,
                                     int request_id) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service || path.empty()) {
    SendDefaultResponse(IconRequest(request_id, 16, ui::SCALE_FACTOR_NONE));
    return;
  }

  int pixel_size = gfx::kFaviconSize;
  ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_NONE;

  FaviconService::Handle handle;
  if (path.size() > 8 &&
      (path.substr(0, 8) == "iconurl/" || path.substr(0, 8) == "iconurl@")) {
    size_t prefix_length = 8;
    // Optional scale factor appended to iconurl, which may be @1x or @2x.
    if (path.at(7) == '@') {
        size_t slash = path.find("/");
        std::string scale_str = path.substr(8, slash - 8);
        web_ui_util::ParseScaleFactor(scale_str, &scale_factor);
        prefix_length = slash + 1;
    }
    // TODO : Change GetFavicon to support combination of IconType.
    handle = favicon_service->GetRawFavicon(
        GURL(path.substr(prefix_length)),
        history::FAVICON,
        pixel_size,
        scale_factor,
        &cancelable_consumer_,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this)));
  } else {
    GURL url;
    if (path.size() > 5 && path.substr(0, 5) == "size/") {
      size_t slash = path.find("/", 5);
      size_t scale_delimiter = path.find("@", 5);
      std::string size = path.substr(5, slash - 5);
      pixel_size = atoi(size.c_str());
      CHECK(pixel_size == 32 || pixel_size == 16) <<
          "only 32x32 and 16x16 icons are supported";
      // Optional scale factor.
      if (scale_delimiter != std::string::npos && scale_delimiter < slash) {
        std::string scale_str = path.substr(scale_delimiter + 1,
                                            slash - scale_delimiter - 1);
        web_ui_util::ParseScaleFactor(scale_str, &scale_factor);
      }
      url = GURL(path.substr(slash + 1));
    } else {
      // URL requests prefixed with "origin/" are converted to a form with an
      // empty path and a valid scheme. (e.g., example.com -->
      // http://example.com/ or http://example.com/a --> http://example.com/)
      if (path.size() > 7 && path.substr(0, 7) == "origin/") {
        std::string originalUrl = path.substr(7);

        // If the original URL does not specify a scheme (e.g., example.com
        // instead of http://example.com), add "http://" as a default.
        if (!GURL(originalUrl).has_scheme())
          originalUrl = "http://" + originalUrl;

        // Strip the path beyond the top-level domain.
        url = GURL(originalUrl).GetOrigin();
      } else {
        url = GURL(path);
      }
    }

    // Intercept requests for prepopulated pages.
    for (size_t i = 0; i < arraysize(history::kPrepopulatedPages); i++) {
      if (url.spec() ==
          l10n_util::GetStringUTF8(history::kPrepopulatedPages[i].url_id)) {
        SendResponse(request_id,
            ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
                history::kPrepopulatedPages[i].favicon_id,
                scale_factor));
        return;
      }
    }

    handle = favicon_service->GetRawFaviconForURL(
        FaviconService::FaviconForURLParams(
            profile_,
            url,
            icon_types_,
            pixel_size,
            &cancelable_consumer_),
        scale_factor,
        base::Bind(&FaviconSource::OnFaviconDataAvailable,
                   base::Unretained(this)));
  }

  // Attach the ChromeURLDataManager request ID to the history request.
  cancelable_consumer_.SetClientData(favicon_service,
                                     handle,
                                     IconRequest(request_id,
                                                 pixel_size,
                                                 scale_factor));
}

std::string FaviconSource::GetMimeType(const std::string&) const {
  // We need to explicitly return a mime type, otherwise if the user tries to
  // drag the image they get no extension.
  return "image/png";
}

bool FaviconSource::ShouldReplaceExistingSource() const {
  // Leave the existing DataSource in place, otherwise we'll drop any pending
  // requests on the floor.
  return false;
}

void FaviconSource::OnFaviconDataAvailable(
    FaviconService::Handle request_handle,
    const history::FaviconBitmapResult& bitmap_result) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  const IconRequest& request =
      cancelable_consumer_.GetClientData(favicon_service,
                                         request_handle);

  if (bitmap_result.is_valid()) {
    // Forward the data along to the networking system.
    SendResponse(request.request_id, bitmap_result.bitmap_data);
  } else {
    SendDefaultResponse(request);
  }
}

void FaviconSource::SendDefaultResponse(const IconRequest& icon_request) {
  base::RefCountedMemory* bytes = NULL;
  ui::ScaleFactor scale_factor = icon_request.scale_factor;

  if (icon_request.pixel_size == 32) {
    if (!default_favicon_large_.get()) {
      default_favicon_large_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_LARGE_FAVICON, scale_factor);
    }
    bytes = default_favicon_large_;
  } else {
    if (!default_favicon_.get()) {
      default_favicon_ =
          ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_DEFAULT_FAVICON, scale_factor);
    }
    bytes = default_favicon_;
  }
  SendResponse(icon_request.request_id, bytes);
}
