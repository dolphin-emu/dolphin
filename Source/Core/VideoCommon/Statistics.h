// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

struct Statistics
{
  int numPixelShadersCreated;
  int numPixelShadersAlive;
  int numVertexShadersCreated;
  int numVertexShadersAlive;

  int numTexturesCreated;
  int numTexturesUploaded;
  int numTexturesAlive;

  int numVertexLoaders;

  std::array<float, 6> proj;
  std::array<float, 16> gproj;
  std::array<float, 16> g2proj;

  struct ThisFrame
  {
    int numBPLoads;
    int numCPLoads;
    int numXFLoads;

    int numBPLoadsInDL;
    int numCPLoadsInDL;
    int numXFLoadsInDL;

    int numPrims;
    int numDLPrims;
    int numShaderChanges;

    int numPrimitiveJoins;
    int numDrawCalls;

    int numDListsCalled;

    int bytesVertexStreamed;
    int bytesIndexStreamed;
    int bytesUniformStreamed;

    int numTrianglesClipped;
    int numTrianglesIn;
    int numTrianglesRejected;
    int numTrianglesCulled;
    int numDrawnObjects;
    int rasterizedPixels;
    int numTrianglesDrawn;
    int numVerticesLoaded;
    int tevPixelsIn;
    int tevPixelsOut;

    int numEFBPeeks;
    int numEFBPokes;
  };
  ThisFrame thisFrame;
  void ResetFrame();
  static void SwapDL();
  static void Display();
  static void DisplayProj();
};

extern Statistics stats;

#define STATISTICS

#ifdef STATISTICS
#define INCSTAT(a) (a)++;
#define DECSTAT(a) (a)--;
#define ADDSTAT(a, b) (a) += (b);
#define SETSTAT(a, x) (a) = (int)(x);
#else
#define INCSTAT(a) ;
#define ADDSTAT(a, b) ;
#define SETSTAT(a, x) ;
#endif
