// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;

import org.dolphinemu.dolphinemu.model.GameFile;

import java.io.FileOutputStream;

public final class CoverHelper
{
  public static String buildGameTDBUrl(GameFile game, String region)
  {
    String baseUrl = "https://art.gametdb.com/wii/cover/%s/%s.png";
    return String.format(baseUrl, region, game.getGameTdbId());
  }

  public static String getRegion(GameFile game)
  {
    String region;
    switch (game.getRegion())
    {
      case 0: // NTSC_J
        region = "JA";
        break;
      case 1: // NTSC_U
        region = "US";
        break;
      case 4: // NTSC_K
        region = "KO";
        break;
      case 2: // PAL
        switch (game.getCountry())
        {
          case 3: // Australia
            region = "AU";
            break;
          case 4: // France
            region = "FR";
            break;
          case 5: // Germany
            region = "DE";
            break;
          case 6: // Italy
            region = "IT";
            break;
          case 8: // Netherlands
            region = "NL";
            break;
          case 9: // Russia
            region = "RU";
            break;
          case 10: // Spain
            region = "ES";
            break;
          case 0: // Europe
          default:
            region = "EN";
            break;
        }
        break;
      case 3: // Unknown
      default:
        region = "EN";
        break;
    }
    return region;
  }

  public static void saveCover(Bitmap cover, String path)
  {
    try
    {
      FileOutputStream out = new FileOutputStream(path);
      cover.compress(Bitmap.CompressFormat.PNG, 100, out);
      out.close();
    }
    catch (Exception ignored)
    {
    }
  }
}
