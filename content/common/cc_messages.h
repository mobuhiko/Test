// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IPC Messages sent between compositor instances.

#include "content/common/content_export.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"

#ifndef CONTENT_COMMON_CC_MESSAGES_H_
#define CONTENT_COMMON_CC_MESSAGES_H_

namespace WebKit {
class WebData;
class WebFilterOperations;
class WebTransformationMatrix;
}

namespace IPC {

template <>
struct ParamTraits<WebKit::WebData> {
  typedef WebKit::WebData param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebFilterOperation> {
  typedef WebKit::WebFilterOperation param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebFilterOperations> {
  typedef WebKit::WebFilterOperations param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<WebKit::WebTransformationMatrix> {
  typedef WebKit::WebTransformationMatrix param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, PickleIterator* iter, param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_CC_MESSAGES_H_

// Multiply-included message file, hence no include guard.

#define IPC_MESSAGE_START CCMsgStart
#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS(WebKit::WebFilterOperation::FilterType)
