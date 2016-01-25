package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.model.settings.view.HeaderSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SettingsItem;
import org.dolphinemu.dolphinemu.model.settings.view.SingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SliderSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.utils.EGLHelper;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsFragmentPresenter
{
	private SettingsFragmentView mView;

	private String mMenuTag;

	private HashMap<String, SettingSection> mSettings;
	private ArrayList<SettingsItem> mSettingsList;

	public SettingsFragmentPresenter(SettingsFragmentView view)
	{
		mView = view;
	}

	public void onCreate(String menuTag)
	{
		mMenuTag = menuTag;
	}

	public void onViewCreated(HashMap<String, SettingSection> settings)
	{
		setSettings(settings);
	}

	/**
	 * If the screen is rotated, the Activity will forget the settings map. This fragment
	 * won't, though; so rather than have the Activity reload from disk, have the fragment pass
	 * the settings map back to the Activity.
	 */
	public void onAttach()
	{
		if (mSettings != null)
		{
			mView.passSettingsToActivity(mSettings);
		}
	}

	public void putSetting(Setting setting)
	{
		mSettings.get(setting.getSection()).putSetting(setting.getKey(), setting);
	}

	public void loadDefaultSettings()
	{
		loadSettingsList();
	}

	public void setSettings(HashMap<String, SettingSection> settings)
	{
		if (mSettingsList == null && settings != null)
		{
			mSettings = settings;

			loadSettingsList();
		}
		else
		{
			mView.showSettingsList(mSettingsList);
		}
	}

	private void loadSettingsList()
	{
		ArrayList<SettingsItem> sl = new ArrayList<>();

		switch (mMenuTag)
		{
			case SettingsFile.FILE_NAME_DOLPHIN:
				addCoreSettings(sl);
				break;

			case SettingsFile.FILE_NAME_GFX:
				addGraphicsSettings(sl);
				break;

			case SettingsFile.SECTION_GFX_ENHANCEMENTS:
				addEnhanceSettings(sl);
				break;

			case SettingsFile.SECTION_GFX_HACKS:
				addHackSettings(sl);
				break;

			default:
				mView.showToastMessage("Unimplemented menu.");
				return;
		}

		mSettingsList = sl;
		mView.showSettingsList(mSettingsList);
	}

	private void addCoreSettings(ArrayList<SettingsItem> sl)
	{
		Setting cpuCore = null;
		Setting dualCore = null;
		Setting overclockEnable = null;
		Setting overclock = null;

		if (mSettings != null)
		{
			cpuCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_CPU_CORE);
			dualCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_DUAL_CORE);
			overclockEnable = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_ENABLE);
			overclock = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_PERCENT);
		}
		else
		{
			mSettings = new HashMap<>();
			mSettings.put(SettingsFile.SECTION_CORE, new SettingSection(SettingsFile.SECTION_CORE));

			mView.passSettingsToActivity(mSettings);
		}

		// TODO Set default value for cpuCore based on arch.
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_CPU_CORE, SettingsFile.SECTION_CORE, R.string.cpu_core, 0, R.array.string_emu_cores, R.array.int_emu_cores, 4, cpuCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DUAL_CORE, SettingsFile.SECTION_CORE, R.string.dual_core, R.string.dual_core_descrip, true, dualCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_OVERCLOCK_ENABLE, SettingsFile.SECTION_CORE, R.string.overclock_enable, R.string.overclock_enable_description, false, overclockEnable));
		sl.add(new SliderSetting(SettingsFile.KEY_OVERCLOCK_PERCENT, SettingsFile.SECTION_CORE, R.string.overclock_title, 0, 400, "%", 100, overclock));

	}

	private void addGraphicsSettings(ArrayList<SettingsItem> sl)
	{
		Setting showFps = null;

		if (mSettings != null)
		{
			showFps = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_SHOW_FPS);
		}
		else
		{
			mSettings = new HashMap<>();

			mSettings.put(SettingsFile.SECTION_GFX_SETTINGS, new SettingSection(SettingsFile.SECTION_GFX_SETTINGS));
			mSettings.put(SettingsFile.SECTION_GFX_ENHANCEMENTS, new SettingSection(SettingsFile.SECTION_GFX_ENHANCEMENTS));
			mSettings.put(SettingsFile.SECTION_GFX_HACKS, new SettingSection(SettingsFile.SECTION_GFX_HACKS));

			mView.passSettingsToActivity(mSettings);
		}

		sl.add(new CheckBoxSetting(SettingsFile.KEY_SHOW_FPS, SettingsFile.SECTION_GFX_SETTINGS, R.string.show_fps, 0, true, showFps));

		sl.add(new SubmenuSetting(null, null, R.string.enhancements, 0, SettingsFile.SECTION_GFX_ENHANCEMENTS));
		sl.add(new SubmenuSetting(null, null, R.string.hacks, 0, SettingsFile.SECTION_GFX_HACKS));
	}

	private void addEnhanceSettings(ArrayList<SettingsItem> sl)
	{
		Setting resolution = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_INTERNAL_RES);
		Setting fsaa = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_FSAA);
		Setting anisotropic = mSettings.get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_ANISOTROPY);
		Setting efbScaledCopy = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_SCALED_EFB);
		Setting perPixel = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_PER_PIXEL);
		Setting forceFilter = mSettings.get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_FORCE_FILTERING);
		Setting disableFog = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_DISABLE_FOG);

		sl.add(new SingleChoiceSetting(SettingsFile.KEY_INTERNAL_RES, SettingsFile.SECTION_GFX_SETTINGS, R.string.internal_resolution, R.string.internal_resolution_descrip, R.array.internalResolutionEntries, R.array.internalResolutionValues, 0, resolution));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_FSAA, SettingsFile.SECTION_GFX_SETTINGS, R.string.FSAA, R.string.FSAA_descrip, R.array.FSAAEntries, R.array.FSAAValues, 0, fsaa));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_ANISOTROPY, SettingsFile.SECTION_GFX_ENHANCEMENTS, R.string.anisotropic_filtering, R.string.anisotropic_filtering_descrip, R.array.anisotropicFilteringEntries, R.array.anisotropicFilteringValues, 0, anisotropic));

		// TODO
//		Setting shader = mSettings.get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_POST_SHADER)
//		sl.add(new SingleChoiceSetting(.getKey(), , R.string., R.string._descrip, R.array., R.array.));

		sl.add(new CheckBoxSetting(SettingsFile.KEY_SCALED_EFB, SettingsFile.SECTION_GFX_HACKS, R.string.scaled_efb_copy, R.string.scaled_efb_copy_descrip, true, efbScaledCopy));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_PER_PIXEL, SettingsFile.SECTION_GFX_SETTINGS, R.string.per_pixel_lighting, R.string.per_pixel_lighting_descrip, false, perPixel));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_FORCE_FILTERING, SettingsFile.SECTION_GFX_ENHANCEMENTS, R.string.force_texture_filtering, R.string.force_texture_filtering_descrip, false, forceFilter));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DISABLE_FOG, SettingsFile.SECTION_GFX_SETTINGS, R.string.disable_fog, R.string.disable_fog_descrip, false, disableFog));

		 /*
		 Check if we support stereo
		 If we support desktop GL then we must support at least OpenGL 3.2
		 If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
		 */
		EGLHelper helper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

		if ((helper.supportsOpenGL() && helper.GetVersion() >= 320) ||
				(helper.supportsGLES3() && helper.GetVersion() >= 310 && helper.SupportsExtension("GL_ANDROID_extension_pack_es31a")))
		{
			sl.add(new SubmenuSetting(null, null, R.string.stereoscopy, 0, SettingsFile.SECTION_STEREOSCOPY));
		}
	}

	private void addHackSettings(ArrayList<SettingsItem> sl)
	{
		int xfbValue = getXfbValue();

		Setting skipEFB = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_SKIP_EFB);
		Setting ignoreFormat = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_IGNORE_FORMAT);
		Setting efbToTexture = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_EFB_TEXTURE);
		Setting texCacheAccuracy = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_TEXCACHE_ACCURACY);
		IntSetting xfb = new IntSetting(SettingsFile.KEY_XFB, SettingsFile.SECTION_GFX_HACKS, xfbValue);
		Setting fastDepth = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_FAST_DEPTH);
		Setting aspectRatio = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_ASPECT_RATIO);

		sl.add(new HeaderSetting(null, null, R.string.embedded_frame_buffer, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_SKIP_EFB, SettingsFile.SECTION_GFX_HACKS, R.string.skip_efb_access, R.string.skip_efb_access_descrip, false, skipEFB));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_IGNORE_FORMAT, SettingsFile.SECTION_GFX_HACKS, R.string.ignore_format_changes, R.string.ignore_format_changes_descrip, false, ignoreFormat));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_EFB_TEXTURE, SettingsFile.SECTION_GFX_HACKS, R.string.efb_copy_method, R.string.efb_copy_method_descrip, true, efbToTexture));

		sl.add(new HeaderSetting(null, null, R.string.texture_cache, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_TEXCACHE_ACCURACY, SettingsFile.SECTION_GFX_HACKS, R.string.texture_cache_accuracy, R.string.texture_cache_accuracy_descrip, R.array.textureCacheAccuracyEntries, R.array.textureCacheAccuracyValues, 128, texCacheAccuracy));

		sl.add(new HeaderSetting(null, null, R.string.external_frame_buffer, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_XFB_METHOD, SettingsFile.SECTION_GFX_HACKS, R.string.external_frame_buffer, R.string.external_frame_buffer_descrip, R.array.externalFrameBufferEntries, R.array.externalFrameBufferValues, 0, xfb));

		sl.add(new HeaderSetting(null, null, R.string.other, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_FAST_DEPTH, SettingsFile.SECTION_GFX_HACKS, R.string.fast_depth_calculation, R.string.fast_depth_calculation_descrip, true, fastDepth));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_ASPECT_RATIO, SettingsFile.SECTION_GFX_HACKS, R.string.aspect_ratio, R.string.aspect_ratio_descrip, R.array.aspectRatioEntries, R.array.aspectRatioValues, 0, aspectRatio));
	}

	private int getXfbValue()
	{
		int xfbValue;

		try
		{
			boolean usingXFB = ((BooleanSetting) mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_XFB)).getValue();
			boolean usingRealXFB = ((BooleanSetting) mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_XFB_REAL)).getValue();

			if (!usingXFB)
			{
				xfbValue = 0;
			}
			else if (!usingRealXFB)
			{
				xfbValue = 1;
			}
			else
			{
				xfbValue = 2;
			}
		}
		catch (NullPointerException ex)
		{
			xfbValue = 0;
		}

		return xfbValue;
	}
}

