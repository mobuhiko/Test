// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/pickle.h"
#include "base/values.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "chrome/common/extensions/permissions/socket_permission.h"
#include "chrome/common/extensions/permissions/socket_permission_data.h"
#include "ipc/ipc_message.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::SocketPermissionData;

namespace {

std::string Parse(const std::string& permission) {
  SocketPermissionData data;
  CHECK(data.Parse(permission)) << "Parse permission \"" << permission
    << "\" failed.";
  return data.GetAsString();
}

}  // namespace

namespace extensions {

class SocketPermissionTest : public testing::Test {
};

TEST(SocketPermissionTest, General) {
  SocketPermissionData data1, data2;

  CHECK(data1.Parse("tcp-connect"));
  CHECK(data2.Parse("tcp-connect"));

  EXPECT_TRUE(data1 == data2);
  EXPECT_FALSE(data1 < data2);

  CHECK(data1.Parse("tcp-connect"));
  CHECK(data2.Parse("tcp-connect:www.example.com"));

  EXPECT_FALSE(data1 == data2);
  EXPECT_TRUE(data1 < data2);
}

TEST(SocketPermissionTest, Parse) {
  SocketPermissionData data;

  EXPECT_FALSE(data.Parse(""));
  EXPECT_FALSE(data.Parse("*"));
  EXPECT_FALSE(data.Parse("\00\00*"));
  EXPECT_FALSE(data.Parse("\01*"));
  EXPECT_FALSE(data.Parse("tcp-connect:www.example.com:-1"));
  EXPECT_FALSE(data.Parse("tcp-connect:www.example.com:65536"));
  EXPECT_FALSE(data.Parse("tcp-connect:::"));
  EXPECT_FALSE(data.Parse("tcp-connect::0"));
  EXPECT_FALSE(data.Parse("tcp-connect:  www.exmaple.com:  99  "));
  EXPECT_FALSE(data.Parse("tcp-connect:*.exmaple.com :99"));
  EXPECT_FALSE(data.Parse("tcp-connect:*.exmaple.com: 99"));
  EXPECT_FALSE(data.Parse("tcp-connect:*.exmaple.com:99 "));
  EXPECT_FALSE(data.Parse("tcp-connect:\t*.exmaple.com:99"));
  EXPECT_FALSE(data.Parse("tcp-connect:\n*.exmaple.com:99"));

  EXPECT_EQ(Parse("tcp-connect"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect:"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen:"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind:"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to:"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect::"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen::"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind::"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to::"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect:*"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen:*"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind:*"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to:*"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect:*:"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen:*:"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind:*:"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to:*:"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect::*"), "tcp-connect:*:*");
  EXPECT_EQ(Parse("tcp-listen::*"), "tcp-listen:*:*");
  EXPECT_EQ(Parse("udp-bind::*"), "udp-bind:*:*");
  EXPECT_EQ(Parse("udp-send-to::*"), "udp-send-to:*:*");

  EXPECT_EQ(Parse("tcp-connect:www.example.com"),
      "tcp-connect:www.example.com:*");
  EXPECT_EQ(Parse("tcp-listen:www.example.com"),
      "tcp-listen:www.example.com:*");
  EXPECT_EQ(Parse("udp-bind:www.example.com"),
      "udp-bind:www.example.com:*");
  EXPECT_EQ(Parse("udp-send-to:www.example.com"),
      "udp-send-to:www.example.com:*");
  EXPECT_EQ(Parse("udp-send-to:wWW.ExAmPlE.cOm"),
      "udp-send-to:www.example.com:*");

  EXPECT_EQ(Parse("tcp-connect:.example.com"),
      "tcp-connect:*.example.com:*");
  EXPECT_EQ(Parse("tcp-listen:.example.com"),
      "tcp-listen:*.example.com:*");
  EXPECT_EQ(Parse("udp-bind:.example.com"),
      "udp-bind:*.example.com:*");
  EXPECT_EQ(Parse("udp-send-to:.example.com"),
      "udp-send-to:*.example.com:*");

  EXPECT_EQ(Parse("tcp-connect:*.example.com"),
      "tcp-connect:*.example.com:*");
  EXPECT_EQ(Parse("tcp-listen:*.example.com"),
      "tcp-listen:*.example.com:*");
  EXPECT_EQ(Parse("udp-bind:*.example.com"),
      "udp-bind:*.example.com:*");
  EXPECT_EQ(Parse("udp-send-to:*.example.com"),
      "udp-send-to:*.example.com:*");

  EXPECT_EQ(Parse("tcp-connect::99"), "tcp-connect:*:99");
  EXPECT_EQ(Parse("tcp-listen::99"), "tcp-listen:*:99");
  EXPECT_EQ(Parse("udp-bind::99"), "udp-bind:*:99");
  EXPECT_EQ(Parse("udp-send-to::99"), "udp-send-to:*:99");

  EXPECT_EQ(Parse("tcp-connect:www.example.com"),
      "tcp-connect:www.example.com:*");

  EXPECT_EQ(Parse("tcp-connect:*.example.com:99"),
      "tcp-connect:*.example.com:99");
}

TEST(SocketPermissionTest, Match) {
  SocketPermissionData data;

  CHECK(data.Parse("tcp-connect"));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.example.com", 80));

  CHECK(data.Parse("udp-send-to::8800"));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::TCP_CONNECT, "www.example.com", 80));

  CHECK(data.Parse("udp-send-to:*.example.com:8800"));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "SMTP.example.com", 8800));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.google.com", 8800));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "wwwexample.com", 8800));

  CHECK(data.Parse("udp-send-to:*.ExAmPlE.cOm:8800"));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.example.com", 8800));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "smtp.example.com", 8800));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "SMTP.example.com", 8800));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.google.com", 8800));

  CHECK(data.Parse("udp-bind::8800"));
  EXPECT_TRUE(data.Match(
        SocketPermissionData::UDP_BIND, "127.0.0.1", 8800));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_BIND, "127.0.0.1", 8888));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::UDP_SEND_TO, "www.google.com", 8800));

  // Do not wildcard part of ip address.
  CHECK(data.Parse("tcp-connect:*.168.0.1:8800"));
  EXPECT_FALSE(data.Match(
        SocketPermissionData::TCP_CONNECT, "192.168.0.1", 8800));
}

TEST(SocketPermissionTest, IPC) {
  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  {
    IPC::Message m;

    scoped_ptr<APIPermission> permission1(
        permission_info->CreateAPIPermission());
    scoped_ptr<APIPermission> permission2(
        permission_info->CreateAPIPermission());

    permission1->Write(&m);
    PickleIterator iter(m);
    permission2->Read(&m, &iter);

    EXPECT_TRUE(permission1->Equal(permission2.get()));
  }


  {
    IPC::Message m;

    scoped_ptr<APIPermission> permission1(
        permission_info->CreateAPIPermission());
    scoped_ptr<APIPermission> permission2(
        permission_info->CreateAPIPermission());

    scoped_ptr<ListValue> value(new ListValue());
    value->Append(Value::CreateStringValue("tcp-connect:*.example.com:80"));
    value->Append(Value::CreateStringValue("udp-bind::8080"));
    value->Append(Value::CreateStringValue("udp-send-to::8888"));
    CHECK(permission1->FromValue(value.get()));

    EXPECT_FALSE(permission1->Equal(permission2.get()));

    permission1->Write(&m);
    PickleIterator iter(m);
    permission2->Read(&m, &iter);
    EXPECT_TRUE(permission1->Equal(permission2.get()));
  }
}

TEST(SocketPermissionTest, Value) {
  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(APIPermission::kSocket);

  scoped_ptr<APIPermission> permission1(
      permission_info->CreateAPIPermission());
  scoped_ptr<APIPermission> permission2(
      permission_info->CreateAPIPermission());

  scoped_ptr<ListValue> value(new ListValue());
  value->Append(Value::CreateStringValue("tcp-connect:*.example.com:80"));
  value->Append(Value::CreateStringValue("udp-bind::8080"));
  value->Append(Value::CreateStringValue("udp-send-to::8888"));
  CHECK(permission1->FromValue(value.get()));

  EXPECT_FALSE(permission1->Equal(permission2.get()));

  base::Value* vtmp = NULL;
  permission1->ToValue(&vtmp);
  CHECK(vtmp);
  CHECK(permission2->FromValue(vtmp));
  EXPECT_TRUE(permission1->Equal(permission2.get()));

  delete vtmp;
}

}  // namespace extensions
