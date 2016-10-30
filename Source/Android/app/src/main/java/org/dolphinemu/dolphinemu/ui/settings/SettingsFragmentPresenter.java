package org.dolphinemu.dolphinemu.ui.settings;

import org.dolphinemu.dolphinemu.R;
import org.dolphinemu.dolphinemu.model.settings.BooleanSetting;
import org.dolphinemu.dolphinemu.model.settings.IntSetting;
import org.dolphinemu.dolphinemu.model.settings.Setting;
import org.dolphinemu.dolphinemu.model.settings.SettingSection;
import org.dolphinemu.dolphinemu.model.settings.StringSetting;
import org.dolphinemu.dolphinemu.model.settings.view.CheckBoxSetting;
import org.dolphinemu.dolphinemu.model.settings.view.HeaderSetting;
import org.dolphinemu.dolphinemu.model.settings.view.InputBindingSetting;
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

	private int mControllerNumber;
	private int mControllerType;

	public SettingsFragmentPresenter(SettingsFragmentView view)
	{
		mView = view;
	}

	public void onCreate(String menuTag)
	{
		if (menuTag.startsWith(SettingsFile.KEY_GCPAD_TYPE))
		{
			mMenuTag = SettingsFile.KEY_GCPAD_TYPE;
			mControllerNumber = Character.getNumericValue(menuTag.charAt(menuTag.length() - 2));
			mControllerType = Character.getNumericValue(menuTag.charAt(menuTag.length() - 1));
		}
		else if (menuTag.startsWith(SettingsFile.SECTION_WIIMOTE) && !menuTag.equals(SettingsFile.FILE_NAME_WIIMOTE))
		{
			mMenuTag = SettingsFile.SECTION_WIIMOTE;
			mControllerNumber = Character.getNumericValue(menuTag.charAt(menuTag.length() - 1)) + 3;
		}
		else if (menuTag.startsWith(SettingsFile.KEY_WIIMOTE_EXTENSION))
		{
			mMenuTag = SettingsFile.KEY_WIIMOTE_EXTENSION;
			mControllerNumber = Character.getNumericValue(menuTag.charAt(menuTag.length() - 2)) + 3;
			mControllerType = Character.getNumericValue(menuTag.charAt(menuTag.length() - 1));
		}
		else
		{
			mMenuTag = menuTag;
		}
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

			case SettingsFile.FILE_NAME_GCPAD:
				addGcPadSettings(sl);
				break;

			case SettingsFile.FILE_NAME_WIIMOTE:
				addWiimoteSettings(sl);
				break;

			case SettingsFile.SECTION_GFX_ENHANCEMENTS:
				addEnhanceSettings(sl);
				break;

			case SettingsFile.SECTION_GFX_HACKS:
				addHackSettings(sl);
				break;

			case SettingsFile.KEY_GCPAD_TYPE:
				addGcPadSubSettings(sl, mControllerNumber, mControllerType);
				break;

			case SettingsFile.SECTION_WIIMOTE:
				addWiimoteSubSettings(sl, mControllerNumber);
				break;

			case SettingsFile.KEY_WIIMOTE_EXTENSION:
				addExtensionSubSettings(sl, mControllerNumber, mControllerType);
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
		IntSetting videoBackend = new IntSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, SettingsFile.SECTION_CORE, getVideoBackendValue());
		Setting continuousScan = null;
		Setting wiimoteSpeaker = null;

		if (mSettings != null)
		{
			cpuCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_CPU_CORE);
			dualCore = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_DUAL_CORE);
			overclockEnable = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_ENABLE);
			overclock = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_PERCENT);
			continuousScan = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_WIIMOTE_SCAN);
			wiimoteSpeaker = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_WIIMOTE_SPEAKER);
		}
		else
		{
			mSettings = new HashMap<>();
			mSettings.put(SettingsFile.SECTION_CORE, new SettingSection(SettingsFile.SECTION_CORE));

			mView.passSettingsToActivity(mSettings);
		}

		// TODO Set default value for cpuCore based on arch.
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_CPU_CORE, SettingsFile.SECTION_CORE, R.string.cpu_core, 0, R.array.emuCoresEntries, R.array.emuCoresValues, 4, cpuCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DUAL_CORE, SettingsFile.SECTION_CORE, R.string.dual_core, R.string.dual_core_descrip, true, dualCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_OVERCLOCK_ENABLE, SettingsFile.SECTION_CORE, R.string.overclock_enable, R.string.overclock_enable_description, false, overclockEnable));
		sl.add(new SliderSetting(SettingsFile.KEY_OVERCLOCK_PERCENT, SettingsFile.SECTION_CORE, R.string.overclock_title, 0, 400, "%", 100, overclock));

		// While it doesn't seem appropriate to store the video backend with the CPU settings, the video
		// backend is stored in the main INI, which can't be changed by the graphics settings page, so
		// we have to put it here.
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, SettingsFile.SECTION_CORE, R.string.video_backend, R.string.video_backend_descrip,
		       R.array.videoBackendEntries, R.array.videoBackendValues, 0, videoBackend));

		sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SCAN, SettingsFile.SECTION_CORE, R.string.wiimote_scanning, R.string.wiimote_scanning_description, true, continuousScan));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SPEAKER, SettingsFile.SECTION_CORE, R.string.wiimote_speaker, R.string.wiimote_speaker_description, true, wiimoteSpeaker));
	}

	private void addGcPadSettings(ArrayList<SettingsItem> sl)
	{
		if (mSettings != null)
		{
			for (int i = 0; i < 4; i++)
			{
				// TODO This controller_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
				Setting gcPadSetting = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_GCPAD_TYPE + i);
				sl.add(new SingleChoiceSetting(SettingsFile.KEY_GCPAD_TYPE + i, SettingsFile.SECTION_CORE, R.string.controller_0 + i, 0, R.array.gcpadTypeEntries, R.array.gcpadTypeValues, 0, gcPadSetting));
			}
		}
	}

	private void addWiimoteSettings(ArrayList<SettingsItem> sl)
	{
		if (mSettings != null)
		{
			for (int i = 1; i <= 4; i++)
			{
				// TODO This wiimote_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
				Setting wiimoteSetting = mSettings.get(SettingsFile.SECTION_WIIMOTE + i).getSetting(SettingsFile.KEY_WIIMOTE_TYPE);
				sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_TYPE, SettingsFile.SECTION_WIIMOTE + i, R.string.wiimote_0 + i - 1, 0, R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, 0, wiimoteSetting));
			}
		}
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
		Setting texCacheAccuracy = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_TEXCACHE_ACCURACY);
		IntSetting xfb = new IntSetting(SettingsFile.KEY_XFB, SettingsFile.SECTION_GFX_HACKS, xfbValue);
		Setting fastDepth = mSettings.get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_FAST_DEPTH);
		Setting aspectRatio = mSettings.get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_ASPECT_RATIO);

		sl.add(new HeaderSetting(null, null, R.string.embedded_frame_buffer, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_SKIP_EFB, SettingsFile.SECTION_GFX_HACKS, R.string.skip_efb_access, R.string.skip_efb_access_descrip, false, skipEFB));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_IGNORE_FORMAT, SettingsFile.SECTION_GFX_HACKS, R.string.ignore_format_changes, R.string.ignore_format_changes_descrip, false, ignoreFormat));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_EFB_TEXTURE, SettingsFile.SECTION_GFX_HACKS, R.string.efb_copy_method, R.string.efb_copy_method_descrip, true, efbToTexture));

		sl.add(new HeaderSetting(null, null, R.string.texture_cache, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_TEXCACHE_ACCURACY, SettingsFile.SECTION_GFX_SETTINGS, R.string.texture_cache_accuracy, R.string.texture_cache_accuracy_descrip, R.array.textureCacheAccuracyEntries, R.array.textureCacheAccuracyValues, 128, texCacheAccuracy));

		sl.add(new HeaderSetting(null, null, R.string.external_frame_buffer, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_XFB_METHOD, SettingsFile.SECTION_GFX_HACKS, R.string.external_frame_buffer, R.string.external_frame_buffer_descrip, R.array.externalFrameBufferEntries, R.array.externalFrameBufferValues, 0, xfb));

		sl.add(new HeaderSetting(null, null, R.string.other, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_FAST_DEPTH, SettingsFile.SECTION_GFX_HACKS, R.string.fast_depth_calculation, R.string.fast_depth_calculation_descrip, true, fastDepth));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_ASPECT_RATIO, SettingsFile.SECTION_GFX_SETTINGS, R.string.aspect_ratio, R.string.aspect_ratio_descrip, R.array.aspectRatioEntries, R.array.aspectRatioValues, 0, aspectRatio));
	}

	private void addGcPadSubSettings(ArrayList<SettingsItem> sl, int gcPadNumber, int gcPadType)
	{
		if (gcPadType == 1) // Emulated
		{
			Setting bindA = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber);
			Setting bindB = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber);
			Setting bindX = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber);
			Setting bindY = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber);
			Setting bindZ = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber);
			Setting bindStart = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber);
			Setting bindControlUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber);
			Setting bindControlDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber);
			Setting bindControlLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber);
			Setting bindControlRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber);
			Setting bindCUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber);
			Setting bindCDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber);
			Setting bindCLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber);
			Setting bindCRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber);
			Setting bindTriggerL = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber);
			Setting bindTriggerR = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber);
			Setting bindDPadUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber);
			Setting bindDPadDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber);
			Setting bindDPadLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber);
			Setting bindDPadRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber);

			sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_a, bindA));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_b, bindB));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_x, bindX));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_y, bindY));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_z, bindZ));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.button_start, bindStart));

			sl.add(new HeaderSetting(null, null, R.string.controller_control, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindControlUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindControlDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindControlLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindControlRight));

			sl.add(new HeaderSetting(null, null, R.string.controller_c, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindCUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindCDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindCLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindCRight));

			sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.trigger_left, bindTriggerL));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.trigger_right, bindTriggerR));

			sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindDPadUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindDPadDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindDPadLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindDPadRight));
		}
		else // Adapter
		{
			Setting rumble = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber);
			Setting bongos = mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber);

			sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber, SettingsFile.SECTION_CORE, R.string.gc_adapter_rumble, R.string.gc_adapter_rumble_description, false, rumble));
			sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber, SettingsFile.SECTION_CORE, R.string.gc_adapter_bongos, R.string.gc_adapter_bongos_description, false, bongos));
		}
	}

	private void addWiimoteSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
	{
		Setting extension = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIMOTE_EXTENSION + (wiimoteNumber - 3));
		Setting bindA = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber);
		Setting bindB = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber);
		Setting bind1 = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber);
		Setting bind2 = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber);
		Setting bindMinus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber);
		Setting bindPlus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber);
		Setting bindHome = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber);
		Setting bindIRUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber);
		Setting bindIRDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber);
		Setting bindIRLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber);
		Setting bindIRRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber);
		Setting bindIRForward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber);
		Setting bindIRBackward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber);
		Setting bindIRHide = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber);
		Setting bindSwingUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber);
		Setting bindSwingDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber);
		Setting bindSwingLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber);
		Setting bindSwingRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber);
		Setting bindSwingForward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber);
		Setting bindSwingBackward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber);
		Setting bindTiltForward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber);
		Setting bindTiltBackward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber);
		Setting bindTiltLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber);
		Setting bindTiltRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber);
		Setting bindTiltModifier = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber);
		Setting bindShakeX = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber);
		Setting bindShakeY = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber);
		Setting bindShakeZ = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber);
		Setting bindDPadUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber);
		Setting bindDPadDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber);
		Setting bindDPadLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber);
		Setting bindDPadRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber);

		sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_EXTENSION + (wiimoteNumber - 3), SettingsFile.SECTION_BINDINGS, R.string.wiimote_extensions, R.string.wiimote_extensions_descrip, R.array.wiimoteExtensionsEntries, R.array.wiimoteExtensionsValues, 0, extension));

		sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_a, bindA));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_b, bindB));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_one, bind1));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_two, bind2));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_minus, bindMinus));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_plus, bindPlus));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_home, bindHome));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_ir, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindIRUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindIRDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindIRLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindIRRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_forward, bindIRForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_backward, bindIRBackward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.ir_hide, bindIRHide));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindSwingUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindSwingDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindSwingLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindSwingRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_forward, bindSwingForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_backward, bindSwingBackward));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_forward, bindTiltForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_backward, bindTiltBackward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindTiltLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindTiltRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.tilt_modifier, bindTiltModifier));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_x, bindShakeX));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_y, bindShakeY));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_z, bindShakeZ));

		sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindDPadUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindDPadDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindDPadLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindDPadRight));
	}

	private void addExtensionSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber, int extentionType)
	{
		switch (extentionType)
		{
			case 1: // Nunchuk
				Setting bindC = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber);
				Setting bindZ = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber);
				Setting bindUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber);
				Setting bindDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber);
				Setting bindLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber);
				Setting bindRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber);
				Setting bindSwingUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber);
				Setting bindSwingDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber);
				Setting bindSwingLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber);
				Setting bindSwingRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber);
				Setting bindSwingForward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber);
				Setting bindSwingBackward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber);
				Setting bindTiltForward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber);
				Setting bindTiltBackward = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber);
				Setting bindTiltLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber);
				Setting bindTiltRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber);
				Setting bindTiltModifier = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber);
				Setting bindShakeX = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber);
				Setting bindShakeY = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber);
				Setting bindShakeZ = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.nunchuk_button_c, bindC));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_z, bindZ));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindRight));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindSwingUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindSwingDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindSwingLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindSwingRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_forward, bindSwingForward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_backward, bindSwingBackward));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_forward, bindTiltForward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_backward, bindTiltBackward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindTiltLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindTiltRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.tilt_modifier, bindTiltModifier));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_x, bindShakeX));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_y, bindShakeY));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.shake_z, bindShakeZ));
				break;
			case 2: // Classic
				Setting bindA = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber);
				Setting bindB = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber);
				Setting bindX = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber);
				Setting bindY = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber);
				Setting bindZL = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber);
				Setting bindZR = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber);
				Setting bindMinus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber);
				Setting bindPlus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber);
				Setting bindHome = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber);
				Setting bindLeftUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber);
				Setting bindLeftDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber);
				Setting bindLeftLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber);
				Setting bindLeftRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber);
				Setting bindRightUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber);
				Setting bindRightDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber);
				Setting bindRightLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber);
				Setting bindRightRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber);
				Setting bindL = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber);
				Setting bindR = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber);
				Setting bindDpadUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber);
				Setting bindDpadDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber);
				Setting bindDpadLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber);
				Setting bindDpadRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_a, bindA));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_b, bindB));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_x, bindX));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_y, bindY));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.classic_button_zl, bindZL));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.classic_button_zr, bindZR));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_minus, bindMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_plus, bindPlus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_home, bindHome));

				sl.add(new HeaderSetting(null, null, R.string.classic_leftstick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindLeftUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindLeftDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindLeftLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindLeftRight));

				sl.add(new HeaderSetting(null, null, R.string.classic_rightstick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindRightUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindRightDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindRightLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindRightRight));

				sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.trigger_left, bindR));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.trigger_right, bindL));

				sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindDpadUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindDpadDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindDpadLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindDpadRight));
				break;
			case 3: // Guitar
				Setting bindFretGreen = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber);
				Setting bindFretRed = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber);
				Setting bindFretYellow = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber);
				Setting bindFretBlue = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber);
				Setting bindFretOrange = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber);
				Setting bindStrumUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber);
				Setting bindStrumDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber);
				Setting bindGuitarMinus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber);
				Setting bindGuitarPlus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber);
				Setting bindGuitarUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber);
				Setting bindGuitarDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber);
				Setting bindGuitarLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber);
				Setting bindGuitarRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber);
				Setting bindWhammyBar = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.guitar_frets, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_green, bindFretGreen));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_red, bindFretRed));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_yellow, bindFretYellow));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_blue, bindFretBlue));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_orange, bindFretOrange));

				sl.add(new HeaderSetting(null, null, R.string.guitar_strum, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindStrumUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindStrumDown));

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_minus, bindGuitarMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_plus, bindGuitarPlus));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindGuitarUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindGuitarDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindGuitarLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindGuitarRight));

				sl.add(new HeaderSetting(null, null, R.string.guitar_whammy, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindWhammyBar));
				break;
			case 4: // Drums
				Setting bindPadRed = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber);
				Setting bindPadYellow = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber);
				Setting bindPadBlue = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber);
				Setting bindPadGreen = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber);
				Setting bindPadOrange = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber);
				Setting bindPadBass = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber);
				Setting bindDrumsUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber);
				Setting bindDrumsDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber);
				Setting bindDrumsLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber);
				Setting bindDrumsRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber);
				Setting bindDrumsMinus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber);
				Setting bindDrumsPlus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.drums_pads, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_red, bindPadRed));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_yellow, bindPadYellow));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_blue, bindPadBlue));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_green, bindPadGreen));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_orange, bindPadOrange));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.drums_pad_bass, bindPadBass));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindDrumsUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindDrumsDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindDrumsLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindDrumsRight));

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_minus, bindDrumsMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_plus, bindDrumsPlus));
				break;
			case 5: // Turntable
				Setting bindGreenLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber);
				Setting bindRedLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber);
				Setting bindBlueLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber);
				Setting bindGreenRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber);
				Setting bindRedRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber);
				Setting bindBlueRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber);
				Setting bindTurntableMinus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber);
				Setting bindTurntablePlus = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber);
				Setting bindEuphoria = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber);
				Setting bindTurntableLeftLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber);
				Setting bindTurntableLeftRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber);
				Setting bindTurntableRightLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber);
				Setting bindTurntableRightRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber);
				Setting bindTurntableUp = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber);
				Setting bindTurntableDown = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber);
				Setting bindTurntableLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber);
				Setting bindTurntableRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber);
				Setting bindEffectDial = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber);
				Setting bindCrossfadeLeft = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber);
				Setting bindCrossfadeRight = mSettings.get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_green_left, bindGreenLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_red_left, bindRedLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_blue_left, bindBlueLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_green_right, bindGreenRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_red_right, bindRedRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_blue_right, bindBlueRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_minus, bindTurntableMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.button_plus, bindTurntablePlus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_button_euphoria, bindEuphoria));

				sl.add(new HeaderSetting(null, null, R.string.turntable_table_left, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindTurntableLeftLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindTurntableLeftRight));

				sl.add(new HeaderSetting(null, null, R.string.turntable_table_right, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindTurntableRightLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindTurntableRightRight));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_up, bindTurntableUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_down, bindTurntableDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindTurntableLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindTurntableRight));

				sl.add(new HeaderSetting(null, null, R.string.turntable_effect, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.turntable_effect_dial, bindEffectDial));

				sl.add(new HeaderSetting(null, null, R.string.turntable_crossfade, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_left, bindCrossfadeLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, R.string.generic_right, bindCrossfadeRight));
				break;
		}
	}

	private int getVideoBackendValue()
	{
		int videoBackendValue;

		try
		{
			String videoBackend = ((StringSetting)mSettings.get(SettingsFile.SECTION_CORE).getSetting(SettingsFile.KEY_VIDEO_BACKEND)).getValue();
			if (videoBackend.equals("OGL"))
				videoBackendValue = 0;
			else if (videoBackend.equals("Vulkan"))
				videoBackendValue = 1;
			else if (videoBackend.equals("Software"))
				videoBackendValue = 2;
			else if (videoBackend.equals("Null"))
				videoBackendValue = 3;
			else
				videoBackendValue = 0;
		}
		catch (NullPointerException ex)
		{
			videoBackendValue = 0;
		}

		return videoBackendValue;
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
