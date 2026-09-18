// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"

#include "VideoCommon/AbstractGfx.h"
#include "VideoCommon/ConstantManager.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoCommon.h"

class AbstractFramebuffer;
class AbstractPipeline;
class AbstractShader;
class AbstractTexture;
class NativeVertexFormat;

namespace VideoCommon
{
// The number of texture units a recorded draw can have bound.
constexpr u32 NUM_RECORDED_TEXTURES = 8;

// Most vertex and index data one recording will hold. A frame that draws more than this is one
// that would be far too slow to replay several times over anyway, so the recording is abandoned
// rather than allowed to grow without limit. Three recordings are live at once.
//
// This is a bandwidth ceiling rather than a memory one: the arenas only grow to what a frame
// actually uses, so a normal frame of a few megabytes costs a few megabytes whatever this says.
// What it bounds is the upload a replay costs, which is this times the sub-frame count every
// displayed frame. The console can in principle transform its way to a few tens of megabytes in a
// frame at its peak rate, so a ceiling near that would turn the heaviest real scenes into the ones
// that silently stop generating.
constexpr size_t MAX_RECORDED_GEOMETRY_BYTES = 128 * 1024 * 1024;

// Most render-to-texture results one recording will reproduce. Copies past the limit are left out,
// and draws that sample them fall back on the emulated GPU's own copy, which is to say they hold
// still between real frames rather than break.
//
// A target is only made for a slot a copy actually lands in, so this is a ceiling rather than a
// reservation: a game making a dozen copies a frame pays for a dozen whatever this says. Each one
// is the size of what the game copied out, held for as long as the feature is on, so a game that
// really does use every slot at a high internal resolution can hold a good deal of memory here.
constexpr u32 MAX_RECORDED_COPIES = 256;

// Most video interface fields one real frame is ever taken to be shown across. A frame held for
// longer than this is a game that has stalled rather than one running at a low rate.
constexpr u32 MAX_FIELDS_PER_FRAME = 8;

// Ceiling on the sub-frames one displayed frame will average together, whatever is configured.
constexpr u32 MAX_GENERATED_FRAMES_PER_DISPLAY_FRAME = 128;

// Most images one video interface field will hand to the display.
//
// Not bounded by how many backbuffers a backend keeps in flight, which was the earlier reasoning.
// Triple buffering sustains an unlimited *rate* of presents; it only has to keep the pipeline full,
// not hold one image per present within a field. What this actually has to cover is the ratio
// between the display's refresh rate and the console's field rate, and a cap of three put the
// ceiling at a hundred and eighty hertz: on a two hundred and forty hertz panel the fourth refresh
// of every field showed a repeat, and the presents stopped dividing the field evenly. Eight covers
// four hundred and eighty hertz against sixty, which is past anything sold today.
constexpr u32 MAX_PRESENTS_PER_FIELD = 8;

// A single recorded draw call, holding everything needed to submit it again from a replay.
//
// Note that the pipeline is a bare pointer into the shader cache, and the textures are held by
// shared_ptr. Both are only valid until the emulated GPU moves on to the next frame, which is why
// a recording is always replayed before returning from the present it was captured for.
struct RecordedDraw
{
  const AbstractPipeline* pipeline = nullptr;
  NativeVertexFormat* vertex_format = nullptr;
  PrimitiveType primitive_type = PrimitiveType::Triangles;

  // Ranges into the recording's vertex and index arenas.
  u32 vertex_data_offset = 0;
  u32 vertex_count = 0;
  u32 vertex_stride = 0;
  u32 index_data_offset = 0;
  u32 index_count = 0;

  // A hash of the vertex data, which is what says that two draws in two frames are the same piece
  // of geometry rather than merely the same shape of one.
  //
  // This is the only thing that identifies a skinned character. Its bones move but its mesh does
  // not, so the vertices, and in particular the per-vertex matrix indices baked into them, are the
  // same bytes every frame and are different bytes from any other character's. Nothing in the
  // transform constants can tell two of them apart, because on this hardware a skinned draw does
  // not use the shared position matrix at all.
  u64 vertex_hash = 0;

  // Indices into the recording's constant pools.
  u32 vertex_constants = 0;
  u32 pixel_constants = 0;
  u32 geometry_constants = 0;

  AbstractGfx::ViewportAndScissor viewport_and_scissor{};

  std::array<RcTcacheEntry, NUM_RECORDED_TEXTURES> textures{};
  std::array<SamplerState, NUM_RECORDED_TEXTURES> samplers{};

  // Zero where the unit samples the texture held in `textures`, and otherwise one past the index
  // of the copy earlier in this frame that produced it.
  //
  // A replay makes its own copy of everything the game renders to a texture, because the emulated
  // one only ever holds the scene at the moment the emulated GPU drew it. Binding through this is
  // what makes a reflection, a shadow map or a heat haze follow the sub-frame it belongs to
  // instead of being pinned to the last real frame.
  std::array<u32, NUM_RECORDED_TEXTURES> texture_copy_slots{};
};

// A clear of the EFB, replayed to put the same background behind the generated geometry.
struct RecordedClear
{
  // Native EFB coordinates, converted at replay time the same way the emulated clear converts
  // them, so that the region scales with the internal resolution.
  MathUtil::Rectangle<int> rect;
  bool color_enable = false;
  bool alpha_enable = false;
  bool z_enable = false;
  u32 color = 0;
  u32 z = 0;
  PixelFormat pixel_format{};
};

// One of the game's render-to-texture results, captured so the replay can produce its own.
//
// This is the same utility draw the texture cache makes: a full screen triangle through a copy
// pipeline that reads the EFB and writes the destination, with the source rectangle, gamma, copy
// filter and clamping all arriving in one uniform block. Recording the block whole rather than the
// arguments it was built from keeps this indifferent to what the copy shader happens to want.
struct RecordedCopy
{
  const AbstractPipeline* pipeline = nullptr;
  bool from_depth = false;
  bool linear_filter = false;

  // Range into the recording's copy uniform arena.
  u32 uniform_offset = 0;
  u32 uniform_size = 0;

  // What the destination looked like, so the replay's own target can be made to match.
  TextureConfig destination_config;

  // Holds the emulated destination alive for as long as the recording lasts. Draws are matched to
  // the copy that fed them by the entry's address, which only means anything while it exists.
  RcTcacheEntry destination;
};

// What kind of work a command in a recording is, and where the rest of it lives.
//
// Between them these cover everything the emulated GPU does that the picture is built out of.
// Copies out to memory are left alone, having no effect on what is drawn. The copy that makes the
// external framebuffer is kept separately, in RecordedFrame::xfb_copy, because it is what a
// finished frame goes through rather than something a draw samples. Two things are genuinely not
// covered: pokes of individual EFB
// pixels, and the reinterpretation the EFB goes through when a game changes its pixel format part
// way into a frame. Both are rare and both only lose their own contribution rather than breaking
// the frame around them.
enum class RecordedCommandType
{
  Clear,
  Draw,
  Copy,
};

struct RecordedCommand
{
  RecordedCommandType type;
  u32 index;
};

// Everything captured for a single emulated frame, in the order the emulated GPU did it.
//
// The whole frame is kept, not just the pass that ends up on screen. Games build the visible image
// out of several passes: a shadow map, a reflection, a downsampled copy of the scene to distort.
// Replaying only the last of them would leave every one of those effects frozen on whatever the
// emulated GPU last put in the texture, which shows up as a heat haze that judders while the scene
// behind it is smooth.
//
// Constants are pooled rather than stored per draw, because a frame can contain thousands of draws
// but only a handful of distinct constant blocks: the emulated GPU only makes them dirty when the
// game actually changes the corresponding state.
struct RecordedFrame
{
  // The copy that turned the EFB into the visible image, kept so a generated frame can be put
  // through the same one.
  //
  // This copy is not like the others: it is the last thing that happens to a real frame before it
  // is shown, and it is where the pixel engine's gamma, the deflicker filter and the clamping are
  // applied. Handing the display a generated frame that has not been through it means the two
  // alternate between two tone curves and two sharpnesses, which on a gamma corrected title is a
  // brightness difference of more than two to one at field rate.
  //
  // Deliberately kept out of `copies` and given no copy slot, so that it cannot take part in the
  // texture matching that decides which draws correspond.
  RecordedCopy xfb_copy;
  bool has_xfb_copy = false;

  std::vector<RecordedCommand> commands;
  std::vector<RecordedClear> clears;
  std::vector<RecordedDraw> draws;
  std::vector<RecordedCopy> copies;

  std::vector<u8> vertex_data;
  std::vector<u16> index_data;
  std::vector<u8> copy_uniform_data;
  std::vector<VertexShaderConstants> vertex_constants;
  std::vector<PixelShaderConstants> pixel_constants;
  std::vector<GeometryShaderConstants> geometry_constants;

  void Reset();
  bool IsUsable() const { return !draws.empty(); }
};

// The render targets a replay writes its own copies of the game's render-to-texture results into.
//
// One per copy in the recording, made to match what the emulated copy wrote to and reused for
// every sub-frame after that. They cannot be the texture cache's own entries: the replay runs
// several times over between two emulated frames, and writing into those would leave the emulated
// GPU's next frame sampling a copy of an interpolated one.
class ReplayTargets
{
public:
  ReplayTargets();
  ~ReplayTargets();

  // The target for `slot`, made or remade if what is there does not match `config`. Null if it
  // could not be created, which the caller should treat as the copy not having happened.
  AbstractFramebuffer* GetFramebuffer(u32 slot, const TextureConfig& config);

  // The texture behind a slot that GetFramebuffer() has already been called for, or null.
  AbstractTexture* GetTexture(u32 slot) const;

  // Gives every target back. Safe to call with no graphics backend left.
  void Release();

private:
  struct Target
  {
    std::unique_ptr<AbstractTexture> texture;
    std::unique_ptr<AbstractFramebuffer> framebuffer;
  };

  std::vector<Target> m_targets;
};

// What makes a texture the same texture in two consecutive frames.
//
// Deliberately not the cache entry, and deliberately not its hash. The cache keys an entry on the
// contents of the texture, so a game animating one -- a portal cycling through frames, a video
// playing on a screen, scrolling water -- gets a fresh entry with a fresh hash every time the
// pixels change. Identifying by either would say the portal is a different object from the portal,
// every frame, forever: it would never correspond to itself, and the thing in the scene that most
// obviously moves would be the one thing never interpolated.
//
// What holds still while an animation plays is the slot the texture occupies: where it is in
// memory, how much of it there is, how it is laid out and how it is encoded.
struct TextureIdentity
{
  u32 addr = 0;
  u32 size_in_bytes = 0;
  u32 memory_stride = 0;
  TextureAndTLUTFormat format;

  bool operator==(const TextureIdentity& other) const = default;
};

// The above, read off a cache entry.
TextureIdentity IdentifyTexture(const TCacheEntry& entry);

// Why two recorded draws are or are not the same draw. The reason is only used to attribute the
// failure on the statistics overlay: when a game's overlays or its video playback stop being
// interpolated, this says which test is turning them away instead of leaving it to be guessed at.
enum class DrawCorrespondence
{
  Yes,
  // Pipeline, vertex format, primitive type or the vertex and index counts differ, so the two are
  // not even drawing the same amount of the same kind of thing.
  DifferentShape,
  // The same kind of draw, sampling something else. See TextureIdentity.
  DifferentTexture,
};

DrawCorrespondence CompareDraws(const RecordedDraw& a, const RecordedDraw& b);

// Returns whether two recorded draws are the same draw in two consecutive frames, and so whether
// their transforms may be interpolated against each other.
//
// Games submit their scene in a very regular order, so matching on draw index and then confirming
// with a signature is both cheap and reliable. A draw that fails to correspond is rendered at its
// current-frame transform instead of being interpolated, which is what makes geometry that pops
// into existence stay put rather than fly in from wherever the previous draw happened to be.
bool DrawsCorrespond(const RecordedDraw& a, const RecordedDraw& b);

// How far apart two draws' transforms are, as the squared distance between their position
// matrices.
//
// The signature of a draw says what is being drawn, never which one: a scene full of the same tree
// submits draws that are identical in pipeline, vertex format, counts and textures, and differ
// only here. Two draws are the same object when this is small, because an object moves a little
// between frames while another instance of the same mesh is somewhere else entirely.
float TransformDistance(const VertexShaderConstants& a, const VertexShaderConstants& b);

// The size of a draw's own transform, in the same units TransformDistance is measured in.
//
// What counts as "far apart" depends entirely on the scene: a position matrix carries the object's
// distance from the camera in its translation column, and games work in world scales that differ by
// orders of magnitude. Comparing a distance against this rather than against a fixed number asks
// whether an object moved a lot *for what it is*, which is the same question in every game.
float TransformMagnitude(const VertexShaderConstants& constants);

// Writes into `out` the transform state of `to`, with every matrix interpolated from `from`
// towards `to` by `phase`. Everything that is not a transform is taken from `to` unchanged.
//
// Only the transform matrices are interpolated. On this hardware that covers the overwhelming
// majority of visible motion: the camera and rigid objects come from the position matrices, and
// skeletal animation is hardware skinning that indexes the same position matrix memory per vertex.
// Geometry the game animates by rewriting vertex positions in RAM is not covered, and holds still
// between real frames.
void InterpolateTransforms(const VertexShaderConstants& from, const VertexShaderConstants& to,
                           float phase, VertexShaderConstants* out);

// A draw with no counterpart is drawn at the transform this frame gave it, with no interpolation.
//
// An earlier version estimated the motion the scene had in common and carried unmatched draws back
// along it. That is right in principle -- a draw's position matrix is the view times the model, so
// anything static changes by the view alone -- but the estimate has to be made from draws whose
// position matrix means something, and nothing here can tell those apart: overlays contribute an
// exact identity, skinned draws contribute a matrix their shader never reads, and a shadow or
// reflection pass contributes some other view's motion entirely. The estimate landed on identity
// whenever the overlays outvoted the world, which puts an unmatched draw a whole frame ahead of the
// scene, and it landed somewhere else the next frame, which is worse. See §"If you reintroduce
// global motion later" in FRAMEGEN_AUDIT.md for what it would take to do properly.

// Captures the draw calls that make up the visible scene, so they can be submitted again at
// intermediate points in time to produce the frames in between two emulated ones.
//
// Two recordings are kept at once: the one being built for the frame currently being drawn, and
// the completed one from the frame before it. A generated frame replays the newer recording with
// every transform interpolated back towards the older one.
class FrameRecorder
{
public:
  FrameRecorder();
  ~FrameRecorder();

  bool IsRecording() const { return m_recording; }

  // Opens a recording for a new frame.
  void BeginFrame();

  // Notes a clear of the EFB, in the order it happened relative to the draws around it.
  void RecordClear(const RecordedClear& clear);

  // Notes a copy of the EFB into a texture, which is how the emulated GPU ends one pass and makes
  // its result available to the next. `uniforms` is the block the copy pipeline was given, kept
  // verbatim.
  void RecordCopy(const AbstractPipeline* pipeline, bool from_depth, bool linear_filter,
                  const void* uniforms, u32 uniform_size, const RcTcacheEntry& destination);

  void RecordDraw(const AbstractPipeline* pipeline, NativeVertexFormat* vertex_format,
                  PrimitiveType primitive_type, const u8* vertex_data, u32 vertex_count,
                  u32 vertex_stride, const u16* index_data, u32 index_count,
                  const VertexShaderConstants& vertex_constants,
                  const PixelShaderConstants& pixel_constants,
                  const GeometryShaderConstants& geometry_constants,
                  const std::array<RcTcacheEntry, NUM_RECORDED_TEXTURES>& textures,
                  const std::array<SamplerState, NUM_RECORDED_TEXTURES>& samplers,
                  BitSet32 used_textures);

  // Notes the copy that produced the visible image, so a generated frame can be put through the
  // same gamma, filtering and clamping the real one was.
  void RecordXFBCopy(const AbstractPipeline* pipeline, bool linear_filter, const void* uniforms,
                     u32 uniform_size, const TextureConfig& destination_config);

  // Closes the recording for the current frame and makes it the newest completed one.
  void EndFrame();

  // Drops both recordings. Called whenever something happens that could leave the recorded
  // pipelines or textures dangling, such as a config change or a shader cache reload.
  void Invalidate();

  // The two most recently completed recordings. Deliberately not the one being recorded into: the
  // emulated GPU is often part way through the next frame by the time a generated frame is drawn,
  // and half of a frame is not something that can be shown.
  const RecordedFrame& GetCurrentFrame() const { return m_frames[m_completed]; }
  const RecordedFrame& GetPreviousFrame() const { return m_frames[Older(m_completed)]; }

  // Whether real frames have been arriving at a rate steady enough to interpolate across, rather
  // than the long irregular gaps of a loading screen, a pause or fast forward.
  bool HasSteadyFrameInterval() const;

  // Whether there are two usable recordings to interpolate between.
  bool CanInterpolate() const;

  // Counts completed recordings. What it counts is not interesting; that it changes exactly when
  // the pair being interpolated between changes is, because that is how a generated frame knows
  // it is the first one of a new pair rather than another one of the last.
  u64 GetFrameCounter() const { return m_frame_counter; }

private:
  // Appends `value` to `pool`, reusing the entry already on the end of it when the two are
  // identical. Consecutive draws overwhelmingly share their constants, because the emulated GPU
  // only makes a block dirty when the game actually changes the state behind it.
  template <typename T>
  static u32 PoolConstants(std::vector<T>* pool, const T& value);

  // Three recordings are kept rather than two. The two most recently completed ones are what a
  // generated frame is interpolated between, and the emulated GPU carries on into the frame after
  // them the whole time that is happening, so it needs a third slot of its own to record into.
  static constexpr u32 NUM_RECORDINGS = 3;
  static u32 Older(u32 index) { return (index + NUM_RECORDINGS - 1) % NUM_RECORDINGS; }
  static u32 Newer(u32 index) { return (index + 1) % NUM_RECORDINGS; }

  std::array<RecordedFrame, NUM_RECORDINGS> m_frames;

  // A clear that arrived with no recording open, held over to open the next one.
  //
  // The clear that matters most is the one a game issues along with the copy that ends the frame,
  // and that copy is what closes the recording: by the time the clear runs there is nothing left to
  // record into, and it belongs to the frame about to start rather than the one just finished
  // anyway. Without carrying it, no recording holds a clear at all, and every generated frame is
  // drawn over black while every real one is drawn over whatever the game asked for.
  RecordedClear m_pending_clear;
  bool m_has_pending_clear = false;

  // Which copy in the frame being recorded produced each texture cache entry, so that a draw
  // sampling one can be pointed at the replay's copy instead. Only entries the current recording
  // wrote to are in here, and the recording holds a reference to every one of them, so an address
  // cannot come back as a different entry while the frame is open.
  std::unordered_map<const TCacheEntry*, u32> m_copy_slots;

  // When the last recording was closed and how long the one before it took, which is all that is
  // needed to tell a steady frame rate from a loading screen.
  TimePoint m_last_frame_time{};
  DT m_frame_interval{};

  u64 m_frame_counter = 0;
  u32 m_current = 0;
  u32 m_completed = 0;
  bool m_recording = false;

  // Set when a recording was given up on part way through. The slot still has to be completed at
  // the end of the frame, or nothing would advance and the replay would hold the previous pair.
  bool m_abandoned = false;
};

// Draws the frames in between the ones the emulated GPU produces, and averages them into the
// single image the display is about to be given.
//
// The sub-frames of one field are samples of the scene spread across exactly the slice of time
// that field is on screen for, and consecutive fields' slices butt up against each other with
// nothing dropped in between. That makes the averaging a shutter rather than a separate feature: an
// exposure of exactly one field, which is what a camera pointed at the same scene would have
// gathered.
//
// The exposure is weighted as a ramp rather than flat, so it is not the box filter whose nulls
// would sit exactly on the judder frequencies; a ramp readmits a good part of that energy in
// exchange for a result that leans towards the end of the window rather than its middle. That is a
// latency trade rather than a filtering one, and it is the reason this looks sharper than a flat
// exposure of the same length.
class FrameGenerator
{
public:
  FrameGenerator();
  ~FrameGenerator();

  // Draws the sub-frames this field is due, folding each one into the image the next present will
  // show. How many there are is the count the user asked for; nothing here consults the clock.
  //
  // A field may be presented more than once, in which case `part` says which of `parts` images
  // this is and the exposure covers only that fraction of the field's slice of time.
  //
  // Whatever the bucket held is dropped first: it belongs to the picture that has already been
  // shown, and every field gets a fresh exposure.
  //
  // Must be called with the emulated frame finished and before any utility drawing has started, as
  // the recorded pipelines are emulated ones and need the render state that goes with them.
  void RenderIntoBucket(u32 part, u32 parts);

  // The generated frame, ready to stand in for the one the emulated GPU produced.
  //
  // It has been through the game's own copy to the external framebuffer, so it is laid out and
  // toned exactly like the real one and the caller's own source rectangle applies to it unchanged.
  // Null when this display frame has nothing generated to show, in which case the caller should
  // fall back on the frame the emulated GPU produced.
  const AbstractTexture* GetGeneratedXFB() const;

  // How many frames the exposure currently on screen is made of. Zero when nothing is generated.
  u32 GetSampleCount() const { return m_samples; }

  // Releases the render targets. They are made again on demand, so this serves both as the way the
  // feature gives its memory back when switched off and as the way it copes with the EFB changing
  // size or format underneath it.
  void DestroyResources();

private:
  // Creates whatever is missing, and returns whether everything needed is now in place. The
  // targets have to match the EFB, because the pipelines being replayed were compiled against it.
  bool EnsureResources();

  // Folds the frame just drawn into the bucket as one more term of a running mean.
  void Accumulate();

  // Puts the averaged light back into the space the rest of the pipeline expects a frame in.
  void Resolve();

  // Puts the resolved frame through the game's own copy to the external framebuffer. Returns
  // whether it happened; where it did not, the caller has nothing fit to show.
  bool ConvertToXFB(const RecordedFrame& frame);

  std::unique_ptr<AbstractTexture> m_color_texture;
  std::unique_ptr<AbstractTexture> m_depth_texture;
  std::unique_ptr<AbstractFramebuffer> m_framebuffer;
  // Holds the running mean as light rather than as stored values, so it needs the range and the
  // precision that averaging in that space costs.
  std::unique_ptr<AbstractTexture> m_bucket_texture;
  std::unique_ptr<AbstractFramebuffer> m_bucket_framebuffer;
  std::unique_ptr<AbstractShader> m_accumulate_shader;
  std::unique_ptr<AbstractPipeline> m_accumulate_pipeline;

  // The bucket, back in the emulated GPU's own colour space and ready to be copied out.
  std::unique_ptr<AbstractTexture> m_output_texture;
  std::unique_ptr<AbstractFramebuffer> m_output_framebuffer;
  std::unique_ptr<AbstractShader> m_resolve_shader;
  std::unique_ptr<AbstractPipeline> m_resolve_pipeline;

  // What is handed over to be shown: the above put through the game's own copy to the external
  // framebuffer, so a generated frame and a real one differ in what they show rather than in how
  // they are toned. Made to match whatever that copy wrote to, and remade when that changes.
  std::unique_ptr<AbstractTexture> m_xfb_texture;
  std::unique_ptr<AbstractFramebuffer> m_xfb_framebuffer;
  TextureConfig m_xfb_config;

  u32 m_samples = 0;

  // Set when the render targets could not be made, so that the attempt is not repeated on every
  // frame from then on. Cleared by DestroyResources(), which is how switching the feature off and
  // on again gets it to try once more.
  bool m_resources_failed = false;

  // Which slice of the interpolation window this field is the exposure for, arrived at by counting
  // fields rather than by looking at the clock.
  //
  // A real frame is shown for a whole number of fields: two of them for a thirty frame per second
  // game on a sixty hertz video interface, one for a sixty frame per second one. So the fields
  // covering one frame divide its interval into that many equal slices, exactly, and which slice
  // this field is comes from counting how many have gone by since the recording changed. Measuring
  // it instead, from wall clock times that jitter with whatever else the machine is doing, is what
  // made consecutive exposures overlap slightly or leave gaps between them, and at a fixed
  // sub-frame count that unevenness is the only thing left that moves.
  u64 m_last_frame_counter = 0;
  u32 m_fields_this_frame = 0;
  u32 m_fields_per_frame = 1;

  // How far the field has been advanced already, so that the field only counts as gone by once
  // however many images it is presented as.
  u32 m_field_advanced_by = 0;
};
}  // namespace VideoCommon
