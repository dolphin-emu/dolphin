package org.dolphinemu.dolphinemu.services;

import android.app.IntentService;
import android.content.Intent;

public class USBPermService extends IntentService {
	public USBPermService()
	{
		super("USBPermService");
	}

	@Override
	protected void onHandleIntent(Intent intent) {}
}
