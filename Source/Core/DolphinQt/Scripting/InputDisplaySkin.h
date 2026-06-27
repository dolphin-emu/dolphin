// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QHash>
#include <QMap>
#include <QString>
#include <QVector>

#include "Common/CommonTypes.h"

struct GCSkinButton
{
  QString key;
  float x = 0, y = 0;
  QString filled, pressed;
  QString color_key;
};

struct GCSkinStick
{
  QString key_x, key_y;
  float x = 0, y = 0;
  QString gate, gate_color;
  QString knob, knob_color;
  float travel = 20.0f;
};

struct GCSkinDpad
{
  float x = 0, y = 0;
  QString gate, gate_color;
  QMap<QString, QString> pressed;
};

struct GCSkinTrigger
{
  QString key, axis;
  QString base;
  float bx = 0, by = 0, bw = 0, bh = 0;
  float fill_y = 0, fill_h = 0;
  float analog_x = 0, analog_w = 0;
  float digital_x = 0, digital_w = 0;
  float divider_x = 0;
  bool dir_right = false;
};

// A conditional layer drawn over the controller: a full-window scrim and/or a tinted image.
// `when` names the state that activates it ("disconnected", or a button key like "Start").
struct GCSkinOverlay
{
  QString when;
  u32 fill = 0;          // ARGB scrim color; alpha 0 means no scrim
  float opacity = 1.0f;  // layer-wide multiplier applied to scrim and image
  QString image;
  u32 tint = 0;          // ARGB image tint; 0 means draw the image untinted
  float scale = 1.0f;
  QString align;         // "center" (default) or "fill"
};

struct GCSkin
{
  QString tex_dir;
  u32 background = 0xCC000000;
  float scale = 0.5f;
  float offset_x = 0, offset_y = 0;
  int design_w = 522, design_h = 304;
  int tex_size = 128;
  QHash<QString, u32> colors;
  QVector<GCSkinButton> buttons;
  QVector<GCSkinStick> sticks;
  GCSkinDpad dpad;
  bool has_dpad = false;
  QVector<GCSkinTrigger> triggers;
  QVector<GCSkinOverlay> overlays;
  bool gba_mode = false;

  int Width() const;
  int Height() const;
  bool IsValid() const { return !tex_dir.isEmpty(); }

  // Prefers writable user InputDisplay/ dir; falls back to the bundled sys_dir copy.
  static GCSkin Load(const QString& sys_dir, const QString& skin_name = QStringLiteral("gamecube"));
};
