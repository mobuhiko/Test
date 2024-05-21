// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PEPPER_BROKER_OBSERVER_H_
#define CHROME_BROWSER_PEPPER_BROKER_OBSERVER_H_

#include "chrome/browser/tab_contents/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"

class PepperBrokerObserver : public content::WebContentsObserver,
                             public WebContentsUserData<PepperBrokerObserver> {
 public:
  virtual ~PepperBrokerObserver();

 private:
  explicit PepperBrokerObserver(content::WebContents* web_contents);
  static int kUserDataKey;
  friend class WebContentsUserData<PepperBrokerObserver>;

  virtual bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(PepperBrokerObserver);
};

#endif  // CHROME_BROWSER_PEPPER_BROKER_OBSERVER_H_
