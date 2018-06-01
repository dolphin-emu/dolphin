package org.dolphinemu.dolphinemu.utils;

import android.graphics.Bitmap;
import android.net.Uri;
import android.widget.ImageView;

import com.squareup.picasso.Picasso;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.GameFile;

import java.io.File;
import java.net.URI;

public class PicassoUtils {
	public static void loadGameBanner(ImageView imageView, GameFile gameFile) {
		File screenshotFile = new File(URI.create(gameFile.getScreenshotPath()));
		if (screenshotFile.exists()) {
			// Fill in the view contents.
			Picasso.with(imageView.getContext())
					.load(gameFile.getScreenshotPath())
					.fit()
					.centerCrop()
					.noFade()
					.noPlaceholder()
					.config(Bitmap.Config.RGB_565)
					.error(R.drawable.no_banner)
					.into(imageView);
		} else {
			Picasso picassoInstance = new Picasso.Builder(imageView.getContext())
					.addRequestHandler(new GameBannerRequestHandler(gameFile))
					.build();

			picassoInstance
					.load(Uri.parse("iso:/" + gameFile.getPath()))
					.fit()
					.noFade()
					.noPlaceholder()
					.config(Bitmap.Config.RGB_565)
					.error(R.drawable.no_banner)
					.into(imageView);
		}


	}
}
