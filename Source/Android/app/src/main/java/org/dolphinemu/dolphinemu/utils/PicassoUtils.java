package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;

import java.io.File;
import java.net.URI;

public class PicassoUtils {
	public static void loadGameBanner(ImageView imageView, String screenshotPath, String gamePath) {
		File file = new File(URI.create(screenshotPath));
		if (file.exists()) {
			// Fill in the view contents.
			Picasso.with(imageView.getContext())
					.load(screenshotPath)
					.fit()
					.centerCrop()
					.noFade()
					.noPlaceholder()
					.config(Bitmap.Config.RGB_565)
					.error(R.drawable.no_banner)
					.into(imageView);
		} else {
			Picasso picassoInstance = new Picasso.Builder(imageView.getContext())
					.addRequestHandler(new GameBannerRequestHandler())
					.build();

			picassoInstance
					.load(Uri.parse("iso:/" + gamePath))
					.fit()
					.noFade()
					.noPlaceholder()
					.config(Bitmap.Config.RGB_565)
					.error(R.drawable.no_banner)
					.into(imageView);
		}


	}
}
