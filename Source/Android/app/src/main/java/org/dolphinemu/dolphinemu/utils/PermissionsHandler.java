package org.dolphinemu.dolphinemu.utils;

import android.annotation.TargetApi;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Build;
import android.support.v4.app.FragmentActivity;
import android.support.v4.content.ContextCompat;
import android.widget.Toast;

import org.dolphinemu.dolphinemu.R;

import static android.Manifest.permission.WRITE_EXTERNAL_STORAGE;

public class PermissionsHandler {
	public static final int REQUEST_CODE_WRITE_PERMISSION = 500;

	@TargetApi(Build.VERSION_CODES.M)
	public static boolean checkWritePermission(final FragmentActivity activity) {
		if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.M) {
			return true;
		}

		int hasWritePermission = ContextCompat.checkSelfPermission(activity, WRITE_EXTERNAL_STORAGE);

		if (hasWritePermission != PackageManager.PERMISSION_GRANTED) {
			if (activity.shouldShowRequestPermissionRationale(WRITE_EXTERNAL_STORAGE)) {
				showMessageOKCancel(activity, activity.getString(R.string.write_permission_needed),
						new DialogInterface.OnClickListener() {
							@Override
							public void onClick(DialogInterface dialog, int which) {
								activity.requestPermissions(new String[] {WRITE_EXTERNAL_STORAGE},
										REQUEST_CODE_WRITE_PERMISSION);
							}
						});
				return false;
			}

			activity.requestPermissions(new String[] {WRITE_EXTERNAL_STORAGE},
					REQUEST_CODE_WRITE_PERMISSION);
			return false;
		}

		return true;
	}

	public static boolean hasWriteAccess(FragmentActivity activity) {
		if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			int hasWritePermission = ContextCompat.checkSelfPermission(activity, WRITE_EXTERNAL_STORAGE);
			return hasWritePermission == PackageManager.PERMISSION_GRANTED;
		}

		return true;
	}

	private static void showMessageOKCancel(final FragmentActivity activity, String message, DialogInterface.OnClickListener okListener) {
		new AlertDialog.Builder(activity)
				.setMessage(message)
				.setPositiveButton(android.R.string.ok, okListener)
				.setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialogInterface, int i) {
						Toast.makeText(activity, R.string.write_permission_needed, Toast.LENGTH_SHORT)
								.show();
					}
				})
				.create()
				.show();
	}
}
