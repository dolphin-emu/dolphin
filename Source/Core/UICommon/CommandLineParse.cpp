// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <OptionParser.h>

#include "Common/Common.h"
#include "UICommon/CommandLineParse.h"

namespace CommandLineParse
{
std::unique_ptr<optparse::OptionParser> CreateParser(ParserOptions options)
{
  auto parser = std::make_unique<optparse::OptionParser>();
  parser->usage("usage: %prog [options]... [FILE]...").version(scm_rev_str);

  parser->add_option("--version").action("version").help("Print version and exit");

  parser->add_option("-u", "--user").action("store").help("User folder path");
  parser->add_option("-m", "--movie").action("store").help("Play a movie file");
  parser->add_option("-e", "--exec")
      .action("store")
      .metavar("<file>")
      .type("string")
      .help("Load the specified file");

  if (options == ParserOptions::IncludeGUIOptions)
  {
    parser->add_option("-d", "--debugger").action("store_true").help("Opens the debuger");
    parser->add_option("-l", "--logger").action("store_true").help("Opens the logger");
    parser->add_option("-b", "--batch").action("store_true").help("Exit Dolphin with emulation");
    parser->add_option("-c", "--confirm").action("store_true").help("Set Confirm on Stop");
  }

  // XXX: These two are setting configuration options
  parser->add_option("-v", "--video_backend").action("store").help("Specify a video backend");
  parser->add_option("-a", "--audio_emulation")
      .choices({"HLE", "LLE"})
      .help("Choose audio emulation from [%choices]");

  // VR (BTW: I hacked the parser so long options also work with a single "-", these should only have one "-")
  // -vr option like in Oculus Rift Source engine games and Doom 3 BFG
  parser->add_option("--vr").action("store_true").help("Force Virtual Reality on");
  // -steamvr option to use SteamVR instead of Oculus
  parser->add_option("--steamvr").action("store_true").help("Use SteamVR instead of Oculus, and force VR on");
  // -oculus option to use Oculus instead of SteamVR, and to force virtual reality on
  parser->add_option("--oculus").action("store_true").help("Use Oculus instead of SteamVR, and force VR on");
  parser->add_option("--onehmd").action("store_true").help("Only use a single HMD if multiple are present");
  // -force-d3d11 and -force-ogl options like in Oculus Rift unity demos
  // TODO: modify this parser to allow this. Note, wxwidgets had to be modified to allow this
  parser->add_option("--force-d3d11").action("store_true").help("Force use of Direct3D 11 backend");
  parser->add_option("--force-opengl").action("store_true").help("Force use of OpenGL backend");
  parser->add_option("--bruteforce").action("store").help("Return value for brute forcing Action Replay culling codes (needs save state 1 and map file)");

  return parser;
}

optparse::Values& ParseArguments(optparse::OptionParser* parser, int argc, char** argv)
{
  return parser->parse_args(argc, argv);
}
}
