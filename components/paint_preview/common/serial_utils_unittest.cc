// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

TEST(PaintPreviewSerialUtils, TestMakeEmptyPicture) {
  sk_sp<SkPicture> pic = MakeEmptyPicture();
  ASSERT_NE(pic, nullptr);
  auto data = pic->serialize();
  ASSERT_NE(data, nullptr);
  EXPECT_GE(data->size(), 0U);
}

TEST(PaintPreviewSerialUtils, TestPictureProcs) {
  auto pic = MakeEmptyPicture();
  uint32_t content_id = pic->uniqueID();
  const uint64_t kFrameGuid = 2;
  PictureSerializationContext picture_ctx;
  EXPECT_TRUE(
      picture_ctx.insert(std::make_pair(content_id, kFrameGuid)).second);

  DeserializationContext deserial_ctx;
  EXPECT_TRUE(deserial_ctx.insert(std::make_pair(content_id, pic)).second);

  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);

  SkSerialProcs serial_procs = MakeSerialProcs(&picture_ctx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);

  SkDeserialProcs deserial_procs = MakeDeserialProcs(&deserial_ctx);
  EXPECT_EQ(deserial_procs.fPictureCtx, &deserial_ctx);

  // Check that serializing then deserialize the picture works.
  sk_sp<SkData> serial_pic_data =
      serial_procs.fPictureProc(pic.get(), serial_procs.fPictureCtx);
  sk_sp<SkPicture> deserial_pic = deserial_procs.fPictureProc(
      serial_pic_data->data(), serial_pic_data->size(),
      deserial_procs.fPictureCtx);
  EXPECT_EQ(deserial_pic->uniqueID(), content_id);
}

TEST(PaintPreviewSerialUtils, TestSerialPictureNotInMap) {
  PictureSerializationContext picture_ctx;

  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);

  SkSerialProcs serial_procs = MakeSerialProcs(&picture_ctx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);

  auto pic = MakeEmptyPicture();
  EXPECT_EQ(serial_procs.fPictureProc(pic.get(), serial_procs.fPictureCtx),
            nullptr);
}

TEST(PaintPreviewSerialUtils, TestDeserialPictureNotInMap) {
  uint32_t empty_content_id = 5;
  DeserializationContext deserial_ctx;
  EXPECT_TRUE(
      deserial_ctx.insert(std::make_pair(empty_content_id, nullptr)).second);
  SkDeserialProcs deserial_procs = MakeDeserialProcs(&deserial_ctx);
  EXPECT_EQ(deserial_procs.fPictureCtx, &deserial_ctx);

  sk_sp<SkPicture> deserial_pic =
      deserial_procs.fPictureProc(nullptr, 0U, deserial_procs.fPictureCtx);
  EXPECT_NE(deserial_pic, nullptr);  // Produce empty pic rather than nullptr.

  uint32_t missing_content_id = 5;
  deserial_pic = deserial_procs.fPictureProc(&missing_content_id,
                                             sizeof(missing_content_id),
                                             deserial_procs.fPictureCtx);
  EXPECT_NE(deserial_pic, nullptr);  // Produce empty pic rather than nullptr.

  deserial_pic = deserial_procs.fPictureProc(
      &empty_content_id, sizeof(empty_content_id), deserial_procs.fPictureCtx);
  EXPECT_NE(deserial_pic, nullptr);  // Produce empty pic rather than nullptr.
}

TEST(PaintPreviewSerialUtils, TestSerialTypeface) {
  PictureSerializationContext picture_ctx;

  auto typeface = SkTypeface::MakeDefault();
  TypefaceUsageMap usage_map;
  std::unique_ptr<GlyphUsage> usage =
      std::make_unique<SparseGlyphUsage>(typeface->countGlyphs());
  usage->Set(0);
  usage->Set('a');
  usage->Set('b');
  EXPECT_TRUE(
      usage_map.insert(std::make_pair(typeface->uniqueID(), std::move(usage)))
          .second);
  TypefaceSerializationContext typeface_ctx(&usage_map);

  SkSerialProcs serial_procs = MakeSerialProcs(&picture_ctx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);

  EXPECT_NE(
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx),
      nullptr);
  EXPECT_GT(typeface_ctx.finished.count(typeface->uniqueID()), 0U);
}

TEST(PaintPreviewSerialUtils, TestSerialNoTypefaceInMap) {
  PictureSerializationContext picture_ctx;

  auto typeface = SkTypeface::MakeDefault();
  TypefaceUsageMap usage_map;
  TypefaceSerializationContext typeface_ctx(&usage_map);

  SkSerialProcs serial_procs = MakeSerialProcs(&picture_ctx, &typeface_ctx);
  EXPECT_EQ(serial_procs.fPictureCtx, &picture_ctx);
  EXPECT_EQ(serial_procs.fTypefaceCtx, &typeface_ctx);

  EXPECT_NE(
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx),
      nullptr);
  EXPECT_GT(typeface_ctx.finished.count(typeface->uniqueID()), 0U);
  // Still serialize font metadata for lookup if the font is serialized again.
  EXPECT_NE(
      serial_procs.fTypefaceProc(typeface.get(), serial_procs.fTypefaceCtx),
      nullptr);
}

}  // namespace paint_preview
