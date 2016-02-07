package org.dolphinemu.dolphinemu.ui.files;

import org.dolphinemu.dolphinemu.application.scopes.ActivityScoped;

import dagger.Subcomponent;

@ActivityScoped
@Subcomponent(
		modules = {AddDirectoryModule.class}
)
public interface AddDirectoryComponent
{
	/**
	 * Crucial: Injection targets must be strongly typed. Passing an interface to an inject() method results in a
	 * no-op.
	 */
	void inject(AddDirectoryActivity activity);
}
