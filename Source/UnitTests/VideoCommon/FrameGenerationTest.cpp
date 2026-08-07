// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cmath>

#include <gtest/gtest.h>

#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/FrameGeneration.h"

namespace
{
// A position matrix holding nothing but a translation, which is the simplest thing that is still a
// plausible transform: rows 0-2 of posnormalmatrix are the 3x4 position matrix the vertex shader
// multiplies by, and its last column is where the object is.
VertexShaderConstants MakeTranslated(float x, float y, float z)
{
  VertexShaderConstants constants{};
  for (size_t row = 0; row < 3; ++row)
    constants.posnormalmatrix[row][row] = 1.0f;

  constants.posnormalmatrix[0][3] = x;
  constants.posnormalmatrix[1][3] = y;
  constants.posnormalmatrix[2][3] = z;
  return constants;
}
}  // namespace

TEST(FrameGeneration, TransformDistanceIsSquaredDistance)
{
  const auto a = MakeTranslated(0.0f, 0.0f, 0.0f);
  const auto b = MakeTranslated(3.0f, 4.0f, 0.0f);

  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(a, a), 0.0f);
  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(a, b), 25.0f);

  // Symmetric, since it is a distance and the matching code compares candidates in either order.
  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(a, b), VideoCommon::TransformDistance(b, a));
}

TEST(FrameGeneration, TransformMagnitudeScalesWithTheTransform)
{
  // Three ones on the diagonal.
  EXPECT_FLOAT_EQ(VideoCommon::TransformMagnitude(MakeTranslated(0.0f, 0.0f, 0.0f)), 3.0f);

  // Plus the translation, squared.
  EXPECT_FLOAT_EQ(VideoCommon::TransformMagnitude(MakeTranslated(3.0f, 4.0f, 0.0f)), 3.0f + 25.0f);
}

// The property the correspondence rejection threshold rests on: the same absolute movement has to
// read as small when the object is far from the camera and large when it is close, or one threshold
// cannot serve games built at different world scales.
TEST(FrameGeneration, RelativeDistanceFallsOffWithDistanceFromCamera)
{
  constexpr float MOVEMENT = 5.0f;

  const auto near_before = MakeTranslated(0.0f, 0.0f, 10.0f);
  const auto near_after = MakeTranslated(MOVEMENT, 0.0f, 10.0f);
  const auto far_before = MakeTranslated(0.0f, 0.0f, 10000.0f);
  const auto far_after = MakeTranslated(MOVEMENT, 0.0f, 10000.0f);

  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(near_before, near_after),
                  VideoCommon::TransformDistance(far_before, far_after));

  const float near_relative = VideoCommon::TransformDistance(near_before, near_after) /
                              VideoCommon::TransformMagnitude(near_after);
  const float far_relative = VideoCommon::TransformDistance(far_before, far_after) /
                             VideoCommon::TransformMagnitude(far_after);
  EXPECT_GT(near_relative, far_relative);
}

// The correspondence threshold is a fraction of TransformMagnitude, and what that fraction buys is
// a rate of camera rotation. Rotating the view by theta sweeps an object at distance d by about
// d*theta while changing the rotation block by about theta times its own size, so both parts move
// in proportion to themselves and the ratio comes out as theta squared -- with the distance and the
// object's own scale cancelling. That invariance is the whole reason one number can serve every
// scene, so it is worth pinning down.
TEST(FrameGeneration, RelativeDistanceUnderRotationIsIndependentOfDistance)
{
  constexpr float THETA = 0.2f;

  const auto rotated_at = [](float distance) {
    // An object sitting straight ahead, and the same object after the camera has yawed by THETA.
    VertexShaderConstants before = MakeTranslated(0.0f, 0.0f, distance);
    VertexShaderConstants after{};
    after.posnormalmatrix[0][0] = std::cos(THETA);
    after.posnormalmatrix[0][2] = std::sin(THETA);
    after.posnormalmatrix[1][1] = 1.0f;
    after.posnormalmatrix[2][0] = -std::sin(THETA);
    after.posnormalmatrix[2][2] = std::cos(THETA);
    after.posnormalmatrix[0][3] = std::sin(THETA) * distance;
    after.posnormalmatrix[1][3] = 0.0f;
    after.posnormalmatrix[2][3] = std::cos(THETA) * distance;

    return VideoCommon::TransformDistance(before, after) / VideoCommon::TransformMagnitude(after);
  };

  // Near and far give the same answer, and that answer is theta squared.
  EXPECT_NEAR(rotated_at(10.0f), rotated_at(10000.0f), 1e-3f);
  EXPECT_NEAR(rotated_at(10000.0f), THETA * THETA, 1e-3f);
}

TEST(FrameGeneration, InterpolateTransformsHitsBothEnds)
{
  const auto from = MakeTranslated(0.0f, 0.0f, 0.0f);
  const auto to = MakeTranslated(10.0f, 20.0f, 30.0f);
  VertexShaderConstants out{};

  // A phase of one reproduces the newer frame and a phase of zero the older one. The replay relies
  // on this: it is what makes the generated frames a continuation of the real ones rather than a
  // separate sequence that happens to sit near them.
  VideoCommon::InterpolateTransforms(from, to, 1.0f, &out);
  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(out, to), 0.0f);

  VideoCommon::InterpolateTransforms(from, to, 0.0f, &out);
  EXPECT_FLOAT_EQ(VideoCommon::TransformDistance(out, from), 0.0f);
}

TEST(FrameGeneration, InterpolateTransformsIsLinearInPhase)
{
  const auto from = MakeTranslated(0.0f, 0.0f, 0.0f);
  const auto to = MakeTranslated(10.0f, 20.0f, 30.0f);
  VertexShaderConstants out{};

  VideoCommon::InterpolateTransforms(from, to, 0.25f, &out);
  EXPECT_FLOAT_EQ(out.posnormalmatrix[0][3], 2.5f);
  EXPECT_FLOAT_EQ(out.posnormalmatrix[1][3], 5.0f);
  EXPECT_FLOAT_EQ(out.posnormalmatrix[2][3], 7.5f);
}

TEST(FrameGeneration, InterpolateTransformsKeepsLightDirectionsOnTheUnitSphere)
{
  VertexShaderConstants from{};
  VertexShaderConstants to{};
  VertexShaderConstants out{};

  // Two unit directions ninety degrees apart. Interpolating them without renormalising leaves the
  // half way point shorter than either, which the shaders read as the light being weaker there.
  from.lights[0].dir = {1.0f, 0.0f, 0.0f, 0.0f};
  to.lights[0].dir = {0.0f, 1.0f, 0.0f, 0.0f};

  VideoCommon::InterpolateTransforms(from, to, 0.5f, &out);

  const auto& dir = out.lights[0].dir;
  const float length_squared = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
  EXPECT_NEAR(length_squared, 1.0f, 1e-5f);
}

namespace
{
VideoCommon::RecordedDraw MakeDraw()
{
  VideoCommon::RecordedDraw draw;
  // Any non-null value: DrawsCorrespond only ever compares these for equality.
  draw.pipeline = reinterpret_cast<const AbstractPipeline*>(0x1000);
  draw.vertex_format = reinterpret_cast<NativeVertexFormat*>(0x2000);
  draw.primitive_type = PrimitiveType::Triangles;
  draw.vertex_count = 12;
  draw.index_count = 18;
  return draw;
}
}  // namespace

TEST(FrameGeneration, DrawsCorrespondNeedsTheSameShapeOfDraw)
{
  const auto a = MakeDraw();
  EXPECT_TRUE(VideoCommon::DrawsCorrespond(a, a));

  auto other_pipeline = MakeDraw();
  other_pipeline.pipeline = reinterpret_cast<const AbstractPipeline*>(0x1001);
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, other_pipeline));

  auto other_format = MakeDraw();
  other_format.vertex_format = reinterpret_cast<NativeVertexFormat*>(0x2001);
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, other_format));

  auto other_primitive = MakeDraw();
  other_primitive.primitive_type = PrimitiveType::Lines;
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, other_primitive));

  auto other_vertex_count = MakeDraw();
  other_vertex_count.vertex_count = 13;
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, other_vertex_count));

  auto other_index_count = MakeDraw();
  other_index_count.index_count = 19;
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, other_index_count));
}

// The property that lets an animated texture keep corresponding to itself. A portal cycling through
// frames, a video playing on a screen or scrolling water streams new pixels into the same place
// every frame; the texture cache keys entries on content, so each of those is a new entry with a
// new hash. Identifying by anything that moves with the pixels means the object never matches
// itself.
TEST(FrameGeneration, TextureIdentityIgnoresContent)
{
  const VideoCommon::TextureIdentity frame_one{
      .addr = 0x8042'0000, .size_in_bytes = 2048, .memory_stride = 64};
  VideoCommon::TextureIdentity frame_two = frame_one;

  // Same slot, different pixels: still the same texture.
  EXPECT_EQ(frame_one, frame_two);

  // Anything that describes the slot itself does distinguish them.
  frame_two.addr = 0x8042'1000;
  EXPECT_NE(frame_one, frame_two);

  frame_two = frame_one;
  frame_two.size_in_bytes = 4096;
  EXPECT_NE(frame_one, frame_two);

  frame_two = frame_one;
  frame_two.memory_stride = 128;
  EXPECT_NE(frame_one, frame_two);

  frame_two = frame_one;
  frame_two.format = TextureAndTLUTFormat(TextureFormat::RGBA8, TLUTFormat::IA8);
  EXPECT_NE(frame_one, frame_two);
}

TEST(FrameGeneration, DrawsCorrespondIgnoresVertexHash)
{
  // The hash is a tie-break between candidates that already correspond, never a gate on whether
  // they do. Geometry a game animates by rewriting vertices changes its hash every frame while
  // remaining the same object.
  auto a = MakeDraw();
  auto b = MakeDraw();
  a.vertex_hash = 1;
  b.vertex_hash = 2;
  EXPECT_TRUE(VideoCommon::DrawsCorrespond(a, b));
}

TEST(FrameGeneration, DrawsCorrespondMatchesRenderTargetsByCopySlot)
{
  // A texture the game rendered this frame is a fresh cache entry every frame, so what identifies
  // it across frames is which copy produced it, not where that copy landed.
  auto a = MakeDraw();
  auto b = MakeDraw();
  a.texture_copy_slots[0] = 3;
  b.texture_copy_slots[0] = 3;
  EXPECT_TRUE(VideoCommon::DrawsCorrespond(a, b));

  b.texture_copy_slots[0] = 4;
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, b));

  // A draw reading a copy does not correspond to one reading an ordinary texture.
  b.texture_copy_slots[0] = 0;
  EXPECT_FALSE(VideoCommon::DrawsCorrespond(a, b));
}
