package org.dolphinemu.dolphinemu.ui.files;

import org.dolphinemu.dolphinemu.application.scopes.FragmentScoped;

import dagger.Subcomponent;

@FragmentScoped
@Subcomponent(
		modules = {FileListModule.class}
)
public interface FileListComponent
{
	/**
	 * Crucial: Injection targets must be strongly typed. Passing an interface to an inject() method results in a
	 * no-op.
	 */
	void inject(FileListFragment fragment);
}
