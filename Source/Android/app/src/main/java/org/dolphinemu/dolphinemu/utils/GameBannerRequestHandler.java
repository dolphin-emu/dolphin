package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;

import com.squareup.picasso.Picasso;
import com.squareup.picasso.Request;
import com.squareup.picasso.RequestHandler;

import org.dolphinemu.dolphinemu.NativeLibrary;

import java.io.IOException;
import java.nio.IntBuffer;

public class GameBannerRequestHandler extends RequestHandler {
	@Override
	public boolean canHandleRequest(Request data) {
		return "iso".equals(data.uri.getScheme());
	}

	@Override
	public Result load(Request request, int networkPolicy) throws IOException {
		String url = request.uri.getHost() + request.uri.getPath();
		int[] vector = NativeLibrary.GetBanner(url);
		int width = 96;
		int height = 32;
		Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
		bitmap.setPixels(vector, 0, width, 0, 0, width, height);
		return new Result(bitmap, Picasso.LoadedFrom.DISK);
	}
}
