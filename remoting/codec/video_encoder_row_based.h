// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_VIDEO_ENCODER_ROW_BASED_H_
#define REMOTING_CODEC_VIDEO_ENCODER_ROW_BASED_H_

#include "remoting/codec/video_encoder.h"
#include "remoting/proto/video.pb.h"
#include "third_party/skia/include/core/SkRect.h"

namespace remoting {

class Compressor;

// VideoEncoderRowBased implements a VideoEncoder using zlib or verbatim
// compression. Zlib-based encoder must be created using
// CreateZlibEncoder(), verbatim encoder is created with
// CreateVerbatimEncoder().
//
// Compressor is reset before encoding each rectangle, so that each
// rectangle can be decoded independently.
class VideoEncoderRowBased : public VideoEncoder {
 public:
  static VideoEncoderRowBased* CreateZlibEncoder();
  static VideoEncoderRowBased* CreateZlibEncoder(int packet_size);
  static VideoEncoderRowBased* CreateVerbatimEncoder();
  static VideoEncoderRowBased* CreateVerbatimEncoder(int packet_size);

  virtual ~VideoEncoderRowBased();

  virtual void Encode(
      scoped_refptr<CaptureData> capture_data,
      bool key_frame,
      const DataAvailableCallback& data_available_callback) OVERRIDE;

 private:
  VideoEncoderRowBased(Compressor* compressor,
                       VideoPacketFormat::Encoding encoding);
  VideoEncoderRowBased(Compressor* compressor,
                       VideoPacketFormat::Encoding encoding,
                       int packet_size);

  // Encode a single dirty rect using compressor.
  void EncodeRect(const SkIRect& rect, bool last);

  // Marks a packet as the first in a series of rectangle updates.
  void PrepareUpdateStart(const SkIRect& rect, VideoPacket* packet);

  // Retrieves a pointer to the output buffer in |update| used for storing the
  // encoded rectangle data.  Will resize the buffer to |size|.
  uint8* GetOutputBuffer(VideoPacket* packet, size_t size);

  // Submit |message| to |callback_|.
  void SubmitMessage(VideoPacket* packet, size_t rect_index);

  // The encoding of the incoming stream.
  VideoPacketFormat::Encoding encoding_;

  scoped_ptr<Compressor> compressor_;

  scoped_refptr<CaptureData> capture_data_;
  DataAvailableCallback callback_;

  // The most recent screen size.
  SkISize screen_size_;

  int packet_size_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_VIDEO_ENCODER_ROW_BASED_H_
