// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Scripting/InputDisplaySkin.h"

#include <algorithm>
#include <cmath>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "Common/FileUtil.h"

int GCSkin::Width() const
{
  return std::max(1, (int)std::round(design_w * scale));
}

int GCSkin::Height() const
{
  return std::max(1, (int)std::round(design_h * scale));
}

static u32 ParseColor(const QJsonValue& v, u32 fallback = 0)
{
  if (v.isString())
    return static_cast<u32>(v.toString().toULong(nullptr, 16));
  if (v.isDouble())
    return static_cast<u32>(v.toInteger());
  return fallback;
}

static GCSkin LoadFrom(const QString& dir, const QString& skin_name)
{
  QFile f(dir + QLatin1Char('/') + skin_name + QStringLiteral("/skin.json"));
  if (!f.open(QIODevice::ReadOnly))
    return {};

  const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  if (!doc.isObject())
    return {};

  GCSkin skin;
  const QJsonObject root = doc.object();

  const QString tex_sub = root[QStringLiteral("textures")].toString(QStringLiteral("gamecube"));
  skin.tex_dir = dir + QLatin1Char('/') + tex_sub;
  skin.background = ParseColor(root[QStringLiteral("background")], 0xCC000000u);
  skin.gba_mode = root[QStringLiteral("gba_mode")].toBool(false);

  const QJsonObject layout = root[QStringLiteral("layout")].toObject();
  skin.scale = (float)layout[QStringLiteral("scale")].toDouble(0.5);
  skin.offset_x = (float)layout[QStringLiteral("offset_x")].toDouble(0.0);
  skin.offset_y = (float)layout[QStringLiteral("offset_y")].toDouble(0.0);
  skin.design_w = layout[QStringLiteral("design_w")].toInt(522);
  skin.design_h = layout[QStringLiteral("design_h")].toInt(304);
  skin.tex_size = root[QStringLiteral("tex_size")].toInt(128);

  const QJsonObject colors = root[QStringLiteral("colors")].toObject();
  for (auto it = colors.begin(); it != colors.end(); ++it)
    skin.colors.insert(it.key(), ParseColor(it.value()));

  for (const QJsonValue& bv : root[QStringLiteral("buttons")].toArray())
  {
    const QJsonObject b = bv.toObject();
    GCSkinButton btn;
    btn.key = b[QStringLiteral("key")].toString();
    btn.x = (float)b[QStringLiteral("x")].toDouble();
    btn.y = (float)b[QStringLiteral("y")].toDouble();
    btn.filled = b[QStringLiteral("filled")].toString();
    btn.pressed = b[QStringLiteral("pressed")].toString();
    btn.color_key = b[QStringLiteral("color")].toString();
    skin.buttons.append(btn);
  }

  for (const QJsonValue& sv : root[QStringLiteral("sticks")].toArray())
  {
    const QJsonObject s = sv.toObject();
    GCSkinStick stick;
    stick.key_x = s[QStringLiteral("keyx")].toString();
    stick.key_y = s[QStringLiteral("keyy")].toString();
    stick.x = (float)s[QStringLiteral("x")].toDouble();
    stick.y = (float)s[QStringLiteral("y")].toDouble();
    stick.gate = s[QStringLiteral("gate")].toString();
    stick.gate_color = s[QStringLiteral("gate_color")].toString();
    stick.knob = s[QStringLiteral("knob")].toString();
    stick.knob_color = s[QStringLiteral("knob_color")].toString();
    stick.travel = (float)s[QStringLiteral("travel")].toDouble(20.0);
    skin.sticks.append(stick);
  }

  const QJsonObject dpad_obj = root[QStringLiteral("dpad")].toObject();
  if (!dpad_obj.isEmpty())
  {
    skin.has_dpad = true;
    skin.dpad.x = (float)dpad_obj[QStringLiteral("x")].toDouble();
    skin.dpad.y = (float)dpad_obj[QStringLiteral("y")].toDouble();
    skin.dpad.gate = dpad_obj[QStringLiteral("gate")].toString();
    skin.dpad.gate_color = dpad_obj[QStringLiteral("gate_color")].toString();
    const QJsonObject pressed = dpad_obj[QStringLiteral("pressed")].toObject();
    for (auto it = pressed.begin(); it != pressed.end(); ++it)
      skin.dpad.pressed.insert(it.key(), it.value().toString());
  }

  for (const QJsonValue& tv : root[QStringLiteral("triggers")].toArray())
  {
    const QJsonObject t = tv.toObject();
    GCSkinTrigger trig;
    trig.key = t[QStringLiteral("key")].toString();
    trig.axis = t[QStringLiteral("axis")].toString();
    trig.base = t[QStringLiteral("base")].toString();
    trig.bx = (float)t[QStringLiteral("bx")].toDouble();
    trig.by = (float)t[QStringLiteral("by")].toDouble();
    trig.bw = (float)t[QStringLiteral("bw")].toDouble();
    trig.bh = (float)t[QStringLiteral("bh")].toDouble();
    trig.fill_y = (float)t[QStringLiteral("fill_y")].toDouble();
    trig.fill_h = (float)t[QStringLiteral("fill_h")].toDouble();
    trig.analog_x = (float)t[QStringLiteral("analog_x")].toDouble();
    trig.analog_w = (float)t[QStringLiteral("analog_w")].toDouble();
    trig.digital_x = (float)t[QStringLiteral("digital_x")].toDouble();
    trig.digital_w = (float)t[QStringLiteral("digital_w")].toDouble();
    trig.divider_x = (float)t[QStringLiteral("divider_x")].toDouble();
    trig.dir_right = t[QStringLiteral("dir")].toString() == QStringLiteral("right");
    skin.triggers.append(trig);
  }

  for (const QJsonValue& ov : root[QStringLiteral("overlays")].toArray())
  {
    const QJsonObject o = ov.toObject();
    GCSkinOverlay overlay;
    overlay.when = o[QStringLiteral("when")].toString();
    overlay.fill = ParseColor(o[QStringLiteral("fill")], 0);
    overlay.opacity = (float)o[QStringLiteral("opacity")].toDouble(1.0);
    overlay.image = o[QStringLiteral("image")].toString();
    overlay.tint = ParseColor(o[QStringLiteral("tint")], 0);
    overlay.scale = (float)o[QStringLiteral("scale")].toDouble(1.0);
    overlay.align = o[QStringLiteral("align")].toString(QStringLiteral("center"));
    skin.overlays.append(overlay);
  }

  return skin;
}

GCSkin GCSkin::Load(const QString& sys_dir, const QString& skin_name)
{
  // Prefer writable user dir; fall back to the bundled Sys copy.
  const QString user_dir =
      QString::fromStdString(File::GetUserPath(D_USER_IDX)) + QStringLiteral("InputDisplay");
  GCSkin skin = LoadFrom(user_dir, skin_name);
  if (!skin.IsValid())
    skin = LoadFrom(sys_dir, skin_name);
  return skin;
}
