package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;

public class RoundTransform implements com.squareup.picasso.Transformation {

  private final int cornerRadius;

  // Radius is corner rounding in dp
  public RoundTransform(int cornerRadius) {
    this.cornerRadius = cornerRadius;
  }

  @Override
  public Bitmap transform(Bitmap source) {
    final Paint paint = new Paint();
    paint.setAntiAlias(true);
    paint.setShader(new BitmapShader(source, Shader.TileMode.CLAMP,
      Shader.TileMode.CLAMP));

    Bitmap output = Bitmap.createBitmap(source.getWidth(),
      source.getHeight(), Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(output);
    canvas.drawRoundRect(new RectF(0, 0, source.getWidth(),
      source.getHeight()), cornerRadius, cornerRadius, paint);

    if (source != output)
      source.recycle();

    return output;
  }

  @Override
  public String key() {
    return "roundcorner";
  }

}
