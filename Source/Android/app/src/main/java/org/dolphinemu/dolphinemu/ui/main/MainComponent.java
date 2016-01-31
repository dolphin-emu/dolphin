package org.dolphinemu.dolphinemu.ui.main;

import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;
import org.dolphinemu.dolphinemu.ui.main.tv.TvMainActivity;

import dagger.Subcomponent;

@ActivityScoped
@Subcomponent(
		modules = {MainModule.class})
public interface MainComponent
{
	/**
	 * Crucial: Injection targets must be strongly typed. Passing an interface to an inject() method results in a
	 * no-op.
	 */
	void inject(MainActivity activity);

	void inject(TvMainActivity activity);
}
