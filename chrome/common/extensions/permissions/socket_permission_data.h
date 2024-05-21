// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_

#include <string>

namespace extensions {

// A pattern that can be used to match socket permission.
//   <socket-permission-pattern>
//          := <op> |
//             <op> ':' <host> |
//             <op> ':' ':' <port> |
//             <op> ':' <host> ':' <port>
//   <op>   := 'tcp-connect' |
//             'tcp-listen' |
//             'udp-bind' |
//             'udp-send-to'
//   <host> := '*' |
//             '*.' <anychar except '/' and '*'>+ |
//             <anychar except '/' and '*'>+
//   <port> := '*' |
//             <port number between 0 and 65535>)
class SocketPermissionData {
 public:
  enum OperationType {
    NONE = 0,
    TCP_CONNECT,
    TCP_LISTEN,
    UDP_BIND,
    UDP_SEND_TO,
  };

  enum HostType {
    ANY_HOST,
    HOSTS_IN_DOMAINS,
    SPECIFIC_HOSTS,
  };

  SocketPermissionData();
  ~SocketPermissionData();

  // operators <, == are needed by container std::set and algorithms
  // std::set_includes and std::set_differences.
  bool operator<(const SocketPermissionData& rhs) const;
  bool operator==(const SocketPermissionData& rhs) const;

  bool Match(OperationType type, const std::string& host, int port) const;

  bool Parse(const std::string& permission);

  HostType GetHostType() const;
  const std::string GetHost() const;

  const std::string& GetAsString() const;

 private:
  void Reset();

  OperationType type_;
  std::string host_;
  bool match_subdomains_;
  int port_;
  mutable std::string spec_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
