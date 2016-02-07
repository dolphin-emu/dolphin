package org.dolphinemu.dolphinemu.ui.platform;


import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;

import dagger.Subcomponent;

@FragmentScoped
@Subcomponent(modules = {PlatformGamesModule.class})
public interface PlatformGamesComponent
{
	/**
	 * Crucial: Injection targets must be strongly typed. Passing an interface to an inject() method results in a
	 * no-op.
	 */
	void inject(PlatformGamesFragment fragment);
}
