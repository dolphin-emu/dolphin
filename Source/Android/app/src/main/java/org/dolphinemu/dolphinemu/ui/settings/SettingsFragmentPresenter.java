package org.dolphinemu.dolphinemu.ui.settings;

import android.text.TextUtils;

import android.os.Bundle;

import org.dolphinemu.dolphinemu.NativeLibrary;
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
import org.dolphinemu.dolphinemu.model.settings.view.StringSingleChoiceSetting;
import org.dolphinemu.dolphinemu.model.settings.view.SubmenuSetting;
import org.dolphinemu.dolphinemu.services.DirectoryInitializationService;
import org.dolphinemu.dolphinemu.utils.EGLHelper;
import org.dolphinemu.dolphinemu.utils.Log;
import org.dolphinemu.dolphinemu.utils.SettingsFile;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;

public final class SettingsFragmentPresenter
{
	private SettingsFragmentView mView;

	public static final String ARG_CONTROLLER_TYPE = "controller_type";
	private MenuTag mMenuTag;
	private String mGameID;

	private ArrayList<HashMap<String, SettingSection>> mSettings;
	private ArrayList<SettingsItem> mSettingsList;

	private int mControllerNumber;
	private int mControllerType;

	public SettingsFragmentPresenter(SettingsFragmentView view)
	{
		mView = view;
	}

	public void onCreate(MenuTag menuTag, String gameId, Bundle extras)
	{
		mGameID = gameId;

        this.mMenuTag = menuTag;
        if (menuTag.isGCPadMenu() || menuTag.isWiimoteExtensionMenu())
        {
            mControllerNumber = menuTag.getSubType();
            mControllerType = extras.getInt(ARG_CONTROLLER_TYPE);
        }
        else if (menuTag.isWiimoteMenu())
        {
            mControllerNumber = menuTag.getSubType();
        }
        else
        {
            mMenuTag = menuTag;
        }
	}

	public void onViewCreated(ArrayList<HashMap<String, SettingSection>> settings)
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
		mSettings.get(setting.getFile()).get(setting.getSection()).putSetting(setting);
	}

	public void loadDefaultSettings()
	{
		loadSettingsList();
	}

	public void setSettings(ArrayList<HashMap<String, SettingSection>> settings)
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
		if (!TextUtils.isEmpty(mGameID))
		{
			mView.getActivity().setTitle("Game Settings: " + mGameID);
		}
		ArrayList<SettingsItem> sl = new ArrayList<>();

		switch (mMenuTag)
		{
            case CONFIG:
                addConfigSettings(sl);
                break;

            case CONFIG_GENERAL:
				addGeneralSettings(sl);
				break;

            case CONFIG_INTERFACE:
                addInterfaceSettings(sl);
                break;

			case GRAPHICS:
				addGraphicsSettings(sl);
				break;

			case GCPAD_TYPE:
				addGcPadSettings(sl);
				break;

			case WIIMOTE:
				addWiimoteSettings(sl);
				break;

			case ENHANCEMENTS:
				addEnhanceSettings(sl);
				break;

			case HACKS:
				addHackSettings(sl);
				break;

			case GCPAD_1:
			case GCPAD_2:
			case GCPAD_3:
			case GCPAD_4:
				addGcPadSubSettings(sl, mControllerNumber, mControllerType);
				break;

			case WIIMOTE_1:
			case WIIMOTE_2:
			case WIIMOTE_3:
			case WIIMOTE_4:
				addWiimoteSubSettings(sl, mControllerNumber);
				break;

			case WIIMOTE_EXTENSION_1:
			case WIIMOTE_EXTENSION_2:
			case WIIMOTE_EXTENSION_3:
			case WIIMOTE_EXTENSION_4:
				addExtensionTypeSettings(sl, mControllerNumber, mControllerType);
				break;

			case STEREOSCOPY:
				addStereoSettings(sl);
				break;

			default:
				mView.showToastMessage("Unimplemented menu");
				return;
		}

		mSettingsList = sl;
		mView.showSettingsList(mSettingsList);
	}

	private void addConfigSettings(ArrayList<SettingsItem> sl)
	{
		sl.add(new SubmenuSetting(null, null, R.string.general_submenu, 0, MenuTag.CONFIG_GENERAL));
		sl.add(new SubmenuSetting(null, null, R.string.interface_submenu, 0, MenuTag.CONFIG_INTERFACE));
	}

	private void addGeneralSettings(ArrayList<SettingsItem> sl)
	{
		Setting cpuCore = null;
		Setting dualCore = null;
		Setting overclockEnable = null;
		Setting overclock = null;
		Setting speedLimit = null;
		Setting slotADevice = null;
		Setting slotBDevice = null;
		Setting continuousScan = null;
		Setting wiimoteSpeaker = null;
		Setting audioStretch = null;

		if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty())
		{
			cpuCore = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_CPU_CORE);
			dualCore = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_DUAL_CORE);
			overclockEnable = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_ENABLE);
			overclock = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_OVERCLOCK_PERCENT);
            speedLimit = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_SPEED_LIMIT);
			slotADevice = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_SLOT_A_DEVICE);
			slotBDevice = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_SLOT_B_DEVICE);
			continuousScan = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_WIIMOTE_SCAN);
			wiimoteSpeaker = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_WIIMOTE_SPEAKER);
			audioStretch = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_AUDIO_STRETCH);
		}
		else
		{
			mView.passSettingsToActivity(mSettings);
		}

		// TODO: Having different emuCoresEntries/emuCoresValues for each architecture is annoying.
		// The proper solution would be to have one emuCoresEntries and one emuCoresValues
		// and exclude the values that aren't present in PowerPC::AvailableCPUCores().
		int defaultCpuCore = NativeLibrary.DefaultCPUCore();
		int emuCoresEntries;
		int emuCoresValues;
		if (defaultCpuCore == 1)  // x86-64
		{
			emuCoresEntries = R.array.emuCoresEntriesX86_64;
			emuCoresValues = R.array.emuCoresValuesX86_64;
		}
		else if (defaultCpuCore == 4)  // AArch64
		{
			emuCoresEntries = R.array.emuCoresEntriesARM64;
			emuCoresValues = R.array.emuCoresValuesARM64;
		}
		else
		{
			emuCoresEntries = R.array.emuCoresEntriesGeneric;
			emuCoresValues = R.array.emuCoresValuesGeneric;
		}
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_CPU_CORE, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.cpu_core, 0, emuCoresEntries, emuCoresValues, defaultCpuCore, cpuCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DUAL_CORE, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.dual_core, R.string.dual_core_description, true, dualCore));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_OVERCLOCK_ENABLE, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.overclock_enable, R.string.overclock_enable_description, false, overclockEnable));
		sl.add(new SliderSetting(SettingsFile.KEY_OVERCLOCK_PERCENT, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.overclock_title, R.string.overclock_title_description, 400, "%", 100, overclock));
        sl.add(new SliderSetting(SettingsFile.KEY_SPEED_LIMIT, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.speed_limit, 0, 200, "%", 100, speedLimit));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_SLOT_A_DEVICE, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.slot_a_device, 0, R.array.slotDeviceEntries, R.array.slotDeviceValues, 8, slotADevice));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_SLOT_B_DEVICE, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.slot_b_device, 0, R.array.slotDeviceEntries, R.array.slotDeviceValues, 255, slotBDevice));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SCAN, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.wiimote_scanning, R.string.wiimote_scanning_description, true, continuousScan));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_WIIMOTE_SPEAKER, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.wiimote_speaker, R.string.wiimote_speaker_description, true, wiimoteSpeaker));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_AUDIO_STRETCH, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.audio_stretch, R.string.audio_stretch_description, false, audioStretch));
	}

	private void addInterfaceSettings(ArrayList<SettingsItem> sl)
	{
		Setting usePanicHandlers = null;
		Setting onScreenDisplayMessages = null;

		if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty())
		{
			usePanicHandlers = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_INTERFACE).getSetting(SettingsFile.KEY_USE_PANIC_HANDLERS);
			onScreenDisplayMessages = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_INTERFACE).getSetting(SettingsFile.KEY_OSD_MESSAGES);
		}
		sl.add(new CheckBoxSetting(SettingsFile.KEY_USE_PANIC_HANDLERS, SettingsFile.SECTION_INI_INTERFACE, SettingsFile.SETTINGS_DOLPHIN, R.string.panic_handlers, R.string.panic_handlers_description, true, usePanicHandlers));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_OSD_MESSAGES, SettingsFile.SECTION_INI_INTERFACE, SettingsFile.SETTINGS_DOLPHIN, R.string.osd_messages, R.string.osd_messages_description, true, onScreenDisplayMessages));
	}

	private void addGcPadSettings(ArrayList<SettingsItem> sl)
	{
		if (!mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty())
		{
			for (int i = 0; i < 4; i++)
			{
				// TODO This controller_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
				Setting gcPadSetting = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_GCPAD_TYPE + i);
				sl.add(new SingleChoiceSetting(SettingsFile.KEY_GCPAD_TYPE + i, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.controller_0 + i, 0, R.array.gcpadTypeEntries, R.array.gcpadTypeValues, 0, gcPadSetting, MenuTag.getGCPadMenuTag(i)));
			}
		}
	}

	private void addWiimoteSettings(ArrayList<SettingsItem> sl)
	{
		if (!mSettings.get(SettingsFile.SETTINGS_WIIMOTE).isEmpty())
		{
			for (int i = 1; i <= 4; i++)
			{
				// TODO This wiimote_0 + i business is quite the hack. It should work, but only if the definitions are kept together and in order.
				Setting wiimoteSetting = mSettings.get(SettingsFile.SETTINGS_WIIMOTE).get(SettingsFile.SECTION_WIIMOTE + i).getSetting(SettingsFile.KEY_WIIMOTE_TYPE);
				sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_TYPE, SettingsFile.SECTION_WIIMOTE + i, SettingsFile.SETTINGS_WIIMOTE, R.string.wiimote_0 + i - 1, 0, R.array.wiimoteTypeEntries, R.array.wiimoteTypeValues, 0, wiimoteSetting, MenuTag.getWiimoteMenuTag(i)));
			}
		}
	}

	private void addGraphicsSettings(ArrayList<SettingsItem> sl)
	{
		IntSetting videoBackend = new IntSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, getVideoBackendValue());
		Setting showFps = null;
		Setting shaderCompilationMode = null;
		Setting waitForShaders = null;
		Setting aspectRatio = null;

		if (!mSettings.get(SettingsFile.SETTINGS_GFX).isEmpty())
		{
			showFps = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_SHOW_FPS);
			shaderCompilationMode = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_SHADER_COMPILATION_MODE);
			waitForShaders = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_WAIT_FOR_SHADERS);
			aspectRatio = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_ASPECT_RATIO);
		}
		else
		{
			mView.passSettingsToActivity(mSettings);
		}

		if (mSettings.get(SettingsFile.SETTINGS_DOLPHIN).isEmpty())
		{
			mView.passSettingsToActivity(mSettings);
		}

		sl.add(new HeaderSetting(null, null, R.string.graphics_general, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_VIDEO_BACKEND_INDEX, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.video_backend, R.string.video_backend_description, R.array.videoBackendEntries, R.array.videoBackendValues, 0, videoBackend));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_SHOW_FPS, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.show_fps, R.string.show_fps_description, false, showFps));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_SHADER_COMPILATION_MODE, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.shader_compilation_mode, R.string.shader_compilation_mode_description, R.array.shaderCompilationModeEntries, R.array.shaderCompilationModeValues, 0, shaderCompilationMode));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_WAIT_FOR_SHADERS, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.wait_for_shaders, 0, false, waitForShaders));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_ASPECT_RATIO, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.aspect_ratio, R.string.aspect_ratio_description, R.array.aspectRatioEntries, R.array.aspectRatioValues, 0, aspectRatio));

		sl.add(new HeaderSetting(null, null, R.string.graphics_enhancements_and_hacks, 0));
		sl.add(new SubmenuSetting(null, null, R.string.enhancements_submenu, 0, MenuTag.ENHANCEMENTS));
		sl.add(new SubmenuSetting(null, null, R.string.hacks_submenu, 0, MenuTag.HACKS));
	}

	private void addEnhanceSettings(ArrayList<SettingsItem> sl)
	{
		Setting resolution = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_INTERNAL_RES);
		Setting fsaa = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_FSAA);
		Setting anisotropic = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_ANISOTROPY);
		Setting shader = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_POST_SHADER);
		Setting efbScaledCopy = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_SCALED_EFB);
		Setting perPixel = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_PER_PIXEL);
		Setting forceFilter = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_ENHANCEMENTS).getSetting(SettingsFile.KEY_FORCE_FILTERING);
		Setting disableFog = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_DISABLE_FOG);
		Setting disableCopyFilter = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_DISABLE_COPY_FILTER);
        Setting force24BitColor = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_FORCE_24_BIT_COLOR);

		sl.add(new SingleChoiceSetting(SettingsFile.KEY_INTERNAL_RES, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.internal_resolution, R.string.internal_resolution_description, R.array.internalResolutionEntries, R.array.internalResolutionValues, 1, resolution));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_FSAA, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.FSAA, R.string.FSAA_description, R.array.FSAAEntries, R.array.FSAAValues, 0, fsaa));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_ANISOTROPY, SettingsFile.SECTION_GFX_ENHANCEMENTS, SettingsFile.SETTINGS_GFX, R.string.anisotropic_filtering, R.string.anisotropic_filtering_description, R.array.anisotropicFilteringEntries, R.array.anisotropicFilteringValues, 0, anisotropic));

		IntSetting stereoModeValue = (IntSetting) mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_STEREOSCOPY).getSetting(SettingsFile.KEY_STEREO_MODE);
		int anaglyphMode = 3;
		String subDir = stereoModeValue != null && stereoModeValue.getValue() == anaglyphMode ? "Anaglyph" : null;
		String[] shaderListEntries = getShaderList(subDir);
		String[] shaderListValues = new String[shaderListEntries.length];
		System.arraycopy(shaderListEntries, 0, shaderListValues, 0, shaderListEntries.length);
		shaderListValues[0] = "";
		sl.add(new StringSingleChoiceSetting(SettingsFile.KEY_POST_SHADER, SettingsFile.SECTION_GFX_ENHANCEMENTS, SettingsFile.SETTINGS_GFX, R.string.post_processing_shader, R.string.post_processing_shader_description, shaderListEntries, shaderListValues, "", shader));

		sl.add(new CheckBoxSetting(SettingsFile.KEY_SCALED_EFB, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.scaled_efb_copy, R.string.scaled_efb_copy_description, true, efbScaledCopy));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_PER_PIXEL, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.per_pixel_lighting, R.string.per_pixel_lighting_description, false, perPixel));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_FORCE_FILTERING, SettingsFile.SECTION_GFX_ENHANCEMENTS, SettingsFile.SETTINGS_GFX, R.string.force_texture_filtering, R.string.force_texture_filtering_description, false, forceFilter));
        sl.add(new CheckBoxSetting(SettingsFile.KEY_FORCE_24_BIT_COLOR, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.force_24bit_color, R.string.force_24bit_color_description, true, force24BitColor));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DISABLE_FOG, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.disable_fog, R.string.disable_fog_description, false, disableFog));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_DISABLE_COPY_FILTER, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.disable_copy_filter, R.string.disable_copy_filter_description, false, disableCopyFilter));

		 /*
		 Check if we support stereo
		 If we support desktop GL then we must support at least OpenGL 3.2
		 If we only support OpenGLES then we need both OpenGLES 3.1 and AEP
		 */
		EGLHelper helper = new EGLHelper(EGLHelper.EGL_OPENGL_ES2_BIT);

		if ((helper.supportsOpenGL() && helper.GetVersion() >= 320) ||
				(helper.supportsGLES3() && helper.GetVersion() >= 310 && helper.SupportsExtension("GL_ANDROID_extension_pack_es31a")))
		{
			sl.add(new SubmenuSetting(SettingsFile.KEY_STEREO_MODE, null, R.string.stereoscopy_submenu, R.string.stereoscopy_submenu_description, MenuTag.STEREOSCOPY));
		}
	}

	private String[] getShaderList(String subDir)
	{
		try
		{
			String shadersPath = DirectoryInitializationService.getDolphinInternalDirectory() + "/Shaders";
			if(!TextUtils.isEmpty(subDir)) {
				shadersPath += "/" + subDir;
			}

			File file = new File(shadersPath);
			File[] shaderFiles = file.listFiles();
			if (shaderFiles != null)
			{
				String[] result = new String[shaderFiles.length + 1];
				result[0] = "Off";
				for (int i = 0; i < shaderFiles.length; i++)
				{
					String name = shaderFiles[i].getName();
					int extensionIndex = name.indexOf(".glsl");
					if (extensionIndex > 0)
					{
						name = name.substring(0, extensionIndex);
					}
					result[i+1] = name;
				}

				return result;
			}
		}
		catch (Exception ex)
		{
			Log.debug("[Settings] Unable to find shader files");
			// return empty list
		}

		return new String[]{};
	}

	private void addHackSettings(ArrayList<SettingsItem> sl)
	{
		boolean skipEFBValue = getInvertedBooleanValue(SettingsFile.SETTINGS_GFX, SettingsFile.SECTION_GFX_HACKS, SettingsFile.KEY_SKIP_EFB, false);
		boolean ignoreFormatValue = getInvertedBooleanValue(SettingsFile.SETTINGS_GFX, SettingsFile.SECTION_GFX_HACKS, SettingsFile.KEY_IGNORE_FORMAT, true);

		BooleanSetting skipEFB = new BooleanSetting(SettingsFile.KEY_SKIP_EFB, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, skipEFBValue);
		BooleanSetting ignoreFormat = new BooleanSetting(SettingsFile.KEY_IGNORE_FORMAT, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, ignoreFormatValue);
		Setting efbToTexture = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_EFB_TEXTURE);
		Setting texCacheAccuracy = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_TEXCACHE_ACCURACY);
		Setting gpuTextureDecoding = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_SETTINGS).getSetting(SettingsFile.KEY_GPU_TEXTURE_DECODING);
		Setting xfbToTexture = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_XFB_TEXTURE);
		Setting immediateXfb = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_IMMEDIATE_XFB);
		Setting fastDepth = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_GFX_HACKS).getSetting(SettingsFile.KEY_FAST_DEPTH);

		sl.add(new HeaderSetting(null, null, R.string.embedded_frame_buffer, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_SKIP_EFB, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.skip_efb_access, R.string.skip_efb_access_description, false, skipEFB));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_IGNORE_FORMAT, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.ignore_format_changes, R.string.ignore_format_changes_description, true, ignoreFormat));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_EFB_TEXTURE, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.efb_copy_method, R.string.efb_copy_method_description, true, efbToTexture));

		sl.add(new HeaderSetting(null, null, R.string.texture_cache, 0));
		sl.add(new SingleChoiceSetting(SettingsFile.KEY_TEXCACHE_ACCURACY, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.texture_cache_accuracy, R.string.texture_cache_accuracy_description, R.array.textureCacheAccuracyEntries, R.array.textureCacheAccuracyValues, 128, texCacheAccuracy));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_GPU_TEXTURE_DECODING, SettingsFile.SECTION_GFX_SETTINGS, SettingsFile.SETTINGS_GFX, R.string.gpu_texture_decoding, R.string.gpu_texture_decoding_description, false, gpuTextureDecoding));

		sl.add(new HeaderSetting(null, null, R.string.external_frame_buffer, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_XFB_TEXTURE, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.xfb_copy_method, R.string.xfb_copy_method_description, true, xfbToTexture));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_IMMEDIATE_XFB, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.immediate_xfb, R.string.immediate_xfb_description, false, immediateXfb));

		sl.add(new HeaderSetting(null, null, R.string.other, 0));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_FAST_DEPTH, SettingsFile.SECTION_GFX_HACKS, SettingsFile.SETTINGS_GFX, R.string.fast_depth_calculation, R.string.fast_depth_calculation_description, true, fastDepth));
	}

	private void addStereoSettings(ArrayList<SettingsItem> sl)
	{
		Setting stereoModeValue = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_STEREOSCOPY).getSetting(SettingsFile.KEY_STEREO_MODE);
		Setting stereoDepth = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_STEREOSCOPY).getSetting(SettingsFile.KEY_STEREO_DEPTH);
		Setting convergence = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_STEREOSCOPY).getSetting(SettingsFile.KEY_STEREO_CONV);
		Setting swapEyes = mSettings.get(SettingsFile.SETTINGS_GFX).get(SettingsFile.SECTION_STEREOSCOPY).getSetting(SettingsFile.KEY_STEREO_SWAP);

		sl.add(new SingleChoiceSetting(SettingsFile.KEY_STEREO_MODE, SettingsFile.SECTION_STEREOSCOPY, SettingsFile.SETTINGS_GFX, R.string.stereoscopy_mode, R.string.stereoscopy_mode_description, R.array.stereoscopyEntries, R.array.stereoscopyValues, 0, stereoModeValue));
		sl.add(new SliderSetting(SettingsFile.KEY_STEREO_DEPTH, SettingsFile.SECTION_STEREOSCOPY, SettingsFile.SETTINGS_GFX, R.string.stereoscopy_depth, R.string.stereoscopy_depth_description, 100, "%", 20, stereoDepth));
		sl.add(new SliderSetting(SettingsFile.KEY_STEREO_CONV, SettingsFile.SECTION_STEREOSCOPY, SettingsFile.SETTINGS_GFX, R.string.stereoscopy_convergence, R.string.stereoscopy_convergence_description, 200, "%", 0, convergence));
		sl.add(new CheckBoxSetting(SettingsFile.KEY_STEREO_SWAP, SettingsFile.SECTION_STEREOSCOPY, SettingsFile.SETTINGS_GFX, R.string.stereoscopy_swap_eyes, R.string.stereoscopy_swap_eyes_description, false, swapEyes));
	}

	private void addGcPadSubSettings(ArrayList<SettingsItem> sl, int gcPadNumber, int gcPadType)
	{
		if (gcPadType == 1) // Emulated
		{
			Setting bindA = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber);
			Setting bindB = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber);
			Setting bindX = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber);
			Setting bindY = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber);
			Setting bindZ = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber);
			Setting bindStart = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber);
			Setting bindControlUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber);
			Setting bindControlDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber);
			Setting bindControlLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber);
			Setting bindControlRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber);
			Setting bindCUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber);
			Setting bindCDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber);
			Setting bindCLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber);
			Setting bindCRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber);
			Setting bindTriggerL = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber);
			Setting bindTriggerR = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber);
			Setting bindDPadUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber);
			Setting bindDPadDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber);
			Setting bindDPadLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber);
			Setting bindDPadRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber);

			sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_A + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_a, bindA));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_B + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_b, bindB));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_X + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_x, bindX));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Y + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_y, bindY));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_Z + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_z, bindZ));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_START + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_start, bindStart));

			sl.add(new HeaderSetting(null, null, R.string.controller_control, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindControlUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindControlDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindControlLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_CONTROL_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindControlRight));

			sl.add(new HeaderSetting(null, null, R.string.controller_c, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindCUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindCDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindCLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_C_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindCRight));

			sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_L + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.trigger_left, bindTriggerL));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_TRIGGER_R + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.trigger_right, bindTriggerR));

			sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_UP + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindDPadUp));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_DOWN + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindDPadDown));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_LEFT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindDPadLeft));
			sl.add(new InputBindingSetting(SettingsFile.KEY_GCBIND_DPAD_RIGHT + gcPadNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindDPadRight));
		}
		else // Adapter
		{
			Setting rumble = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber);
			Setting bongos = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber);

			sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_RUMBLE + gcPadNumber, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.gc_adapter_rumble, R.string.gc_adapter_rumble_description, false, rumble));
			sl.add(new CheckBoxSetting(SettingsFile.KEY_GCADAPTER_BONGOS + gcPadNumber, SettingsFile.SECTION_INI_CORE, SettingsFile.SETTINGS_DOLPHIN, R.string.gc_adapter_bongos, R.string.gc_adapter_bongos_description, false, bongos));
		}
	}

	private void addWiimoteSubSettings(ArrayList<SettingsItem> sl, int wiimoteNumber)
	{
		// Bindings use controller numbers 4-7 (0-3 are GameCube), but the extension setting uses 1-4.
		IntSetting extension = new IntSetting(SettingsFile.KEY_WIIMOTE_EXTENSION, SettingsFile.SECTION_WIIMOTE + wiimoteNumber, SettingsFile.SETTINGS_WIIMOTE, getExtensionValue(wiimoteNumber), MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber));
		Setting bindA = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber);
		Setting bindB = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber);
		Setting bind1 = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber);
		Setting bind2 = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber);
		Setting bindMinus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber);
		Setting bindPlus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber);
		Setting bindHome = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber);
		Setting bindIRUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber);
		Setting bindIRDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber);
		Setting bindIRLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber);
		Setting bindIRRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber);
		Setting bindIRForward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber);
		Setting bindIRBackward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber);
		Setting bindIRHide = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber);
		Setting bindSwingUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber);
		Setting bindSwingDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber);
		Setting bindSwingLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber);
		Setting bindSwingRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber);
		Setting bindSwingForward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber);
		Setting bindSwingBackward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber);
		Setting bindTiltForward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber);
		Setting bindTiltBackward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber);
		Setting bindTiltLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber);
		Setting bindTiltRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber);
		Setting bindTiltModifier = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber);
		Setting bindShakeX = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber);
		Setting bindShakeY = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber);
		Setting bindShakeZ = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber);
		Setting bindDPadUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber);
		Setting bindDPadDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber);
		Setting bindDPadLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber);
		Setting bindDPadRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber);

		sl.add(new SingleChoiceSetting(SettingsFile.KEY_WIIMOTE_EXTENSION, SettingsFile.SECTION_WIIMOTE + (wiimoteNumber - 3), SettingsFile.SETTINGS_WIIMOTE, R.string.wiimote_extensions, R.string.wiimote_extensions_description, R.array.wiimoteExtensionsEntries, R.array.wiimoteExtensionsValues, 0, extension, MenuTag.getWiimoteExtensionMenuTag(wiimoteNumber)));

		sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_A + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_a, bindA));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_B + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_b, bindB));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_1 + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_one, bind1));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_2 + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_two, bind2));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_minus, bindMinus));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_plus, bindPlus));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_HOME + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_home, bindHome));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_ir, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindIRUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindIRDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindIRLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindIRRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_forward, bindIRForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_backward, bindIRBackward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_IR_HIDE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.ir_hide, bindIRHide));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindSwingUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindSwingDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindSwingLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindSwingRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_forward, bindSwingForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SWING_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_backward, bindSwingBackward));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_forward, bindTiltForward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_backward, bindTiltBackward));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindTiltLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindTiltRight));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TILT_MODIFIER + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.tilt_modifier, bindTiltModifier));

		sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_x, bindShakeX));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_y, bindShakeY));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_SHAKE_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_z, bindShakeZ));

		sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindDPadUp));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindDPadDown));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindDPadLeft));
		sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DPAD_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindDPadRight));
	}

	private void addExtensionTypeSettings(ArrayList<SettingsItem> sl, int wiimoteNumber, int extentionType)
	{
		switch (extentionType)
		{
			case 1: // Nunchuk
				Setting bindC = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber);
				Setting bindZ = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber);
				Setting bindUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber);
				Setting bindDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber);
				Setting bindLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber);
				Setting bindRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber);
				Setting bindSwingUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber);
				Setting bindSwingDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber);
				Setting bindSwingLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber);
				Setting bindSwingRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber);
				Setting bindSwingForward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber);
				Setting bindSwingBackward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber);
				Setting bindTiltForward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber);
				Setting bindTiltBackward = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber);
				Setting bindTiltLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber);
				Setting bindTiltRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber);
				Setting bindTiltModifier = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber);
				Setting bindShakeX = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber);
				Setting bindShakeY = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber);
				Setting bindShakeZ = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_C + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.nunchuk_button_c, bindC));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_z, bindZ));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindRight));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_swing, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindSwingUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindSwingDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindSwingLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindSwingRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_forward, bindSwingForward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SWING_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_backward, bindSwingBackward));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_tilt, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_FORWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_forward, bindTiltForward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_BACKWARD + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_backward, bindTiltBackward));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindTiltLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindTiltRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_TILT_MODIFIER + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.tilt_modifier, bindTiltModifier));

				sl.add(new HeaderSetting(null, null, R.string.wiimote_shake, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_x, bindShakeX));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_y, bindShakeY));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_NUNCHUK_SHAKE_Z + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.shake_z, bindShakeZ));
				break;
			case 2: // Classic
				Setting bindA = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber);
				Setting bindB = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber);
				Setting bindX = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber);
				Setting bindY = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber);
				Setting bindZL = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber);
				Setting bindZR = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber);
				Setting bindMinus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber);
				Setting bindPlus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber);
				Setting bindHome = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber);
				Setting bindLeftUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber);
				Setting bindLeftDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber);
				Setting bindLeftLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber);
				Setting bindLeftRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber);
				Setting bindRightUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber);
				Setting bindRightDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber);
				Setting bindRightLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber);
				Setting bindRightRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber);
				Setting bindL = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber);
				Setting bindR = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber);
				Setting bindDpadUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber);
				Setting bindDpadDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber);
				Setting bindDpadLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber);
				Setting bindDpadRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_A + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_a, bindA));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_B + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_b, bindB));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_X + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_x, bindX));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_Y + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_y, bindY));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZL + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.classic_button_zl, bindZL));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_ZR + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.classic_button_zr, bindZR));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_minus, bindMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_plus, bindPlus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_HOME + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_home, bindHome));

				sl.add(new HeaderSetting(null, null, R.string.classic_leftstick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindLeftUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindLeftDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindLeftLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_LEFT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindLeftRight));

				sl.add(new HeaderSetting(null, null, R.string.classic_rightstick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindRightUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindRightDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindRightLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_RIGHT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindRightRight));

				sl.add(new HeaderSetting(null, null, R.string.controller_trig, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_L + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.trigger_left, bindR));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_TRIGGER_R + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.trigger_right, bindL));

				sl.add(new HeaderSetting(null, null, R.string.controller_dpad, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindDpadUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindDpadDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindDpadLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_CLASSIC_DPAD_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindDpadRight));
				break;
			case 3: // Guitar
				Setting bindFretGreen = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber);
				Setting bindFretRed = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber);
				Setting bindFretYellow = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber);
				Setting bindFretBlue = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber);
				Setting bindFretOrange = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber);
				Setting bindStrumUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber);
				Setting bindStrumDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber);
				Setting bindGuitarMinus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber);
				Setting bindGuitarPlus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber);
				Setting bindGuitarUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber);
				Setting bindGuitarDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber);
				Setting bindGuitarLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber);
				Setting bindGuitarRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber);
				Setting bindWhammyBar = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.guitar_frets, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_GREEN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_green, bindFretGreen));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_RED + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_red, bindFretRed));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_YELLOW + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_yellow, bindFretYellow));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_BLUE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_blue, bindFretBlue));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_FRET_ORANGE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_orange, bindFretOrange));

				sl.add(new HeaderSetting(null, null, R.string.guitar_strum, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindStrumUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STRUM_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindStrumDown));

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_minus, bindGuitarMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_plus, bindGuitarPlus));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindGuitarUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindGuitarDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindGuitarLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindGuitarRight));

				sl.add(new HeaderSetting(null, null, R.string.guitar_whammy, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_GUITAR_WHAMMY_BAR + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindWhammyBar));
				break;
			case 4: // Drums
				Setting bindPadRed = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber);
				Setting bindPadYellow = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber);
				Setting bindPadBlue = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber);
				Setting bindPadGreen = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber);
				Setting bindPadOrange = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber);
				Setting bindPadBass = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber);
				Setting bindDrumsUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber);
				Setting bindDrumsDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber);
				Setting bindDrumsLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber);
				Setting bindDrumsRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber);
				Setting bindDrumsMinus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber);
				Setting bindDrumsPlus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.drums_pads, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_RED + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_red, bindPadRed));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_YELLOW + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_yellow, bindPadYellow));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BLUE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_blue, bindPadBlue));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_GREEN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_green, bindPadGreen));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_ORANGE + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_orange, bindPadOrange));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PAD_BASS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.drums_pad_bass, bindPadBass));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindDrumsUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindDrumsDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindDrumsLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindDrumsRight));

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_minus, bindDrumsMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_DRUMS_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_plus, bindDrumsPlus));
				break;
			case 5: // Turntable
				Setting bindGreenLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber);
				Setting bindRedLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber);
				Setting bindBlueLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber);
				Setting bindGreenRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber);
				Setting bindRedRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber);
				Setting bindBlueRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber);
				Setting bindTurntableMinus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber);
				Setting bindTurntablePlus = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber);
				Setting bindEuphoria = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber);
				Setting bindTurntableLeftLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber);
				Setting bindTurntableLeftRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber);
				Setting bindTurntableRightLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber);
				Setting bindTurntableRightRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber);
				Setting bindTurntableUp = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber);
				Setting bindTurntableDown = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber);
				Setting bindTurntableLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber);
				Setting bindTurntableRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber);
				Setting bindEffectDial = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber);
				Setting bindCrossfadeLeft = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber);
				Setting bindCrossfadeRight = mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_BINDINGS).getSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber);

				sl.add(new HeaderSetting(null, null, R.string.generic_buttons, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_green_left, bindGreenLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_red_left, bindRedLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_blue_left, bindBlueLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_GREEN_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_green_right, bindGreenRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RED_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_red_right, bindRedRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_BLUE_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_blue_right, bindBlueRight));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_MINUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_minus, bindTurntableMinus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_PLUS + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.button_plus, bindTurntablePlus));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EUPHORIA + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_button_euphoria, bindEuphoria));

				sl.add(new HeaderSetting(null, null, R.string.turntable_table_left, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindTurntableLeftLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_LEFT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindTurntableLeftRight));

				sl.add(new HeaderSetting(null, null, R.string.turntable_table_right, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindTurntableRightLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_RIGHT_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindTurntableRightRight));

				sl.add(new HeaderSetting(null, null, R.string.generic_stick, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_UP + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_up, bindTurntableUp));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_DOWN + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_down, bindTurntableDown));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindTurntableLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_STICK_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindTurntableRight));

				sl.add(new HeaderSetting(null, null, R.string.turntable_effect, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_EFFECT_DIAL + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.turntable_effect_dial, bindEffectDial));

				sl.add(new HeaderSetting(null, null, R.string.turntable_crossfade, 0));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_LEFT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_left, bindCrossfadeLeft));
				sl.add(new InputBindingSetting(SettingsFile.KEY_WIIBIND_TURNTABLE_CROSSFADE_RIGHT + wiimoteNumber, SettingsFile.SECTION_BINDINGS, SettingsFile.SETTINGS_DOLPHIN, R.string.generic_right, bindCrossfadeRight));
				break;
		}
	}

	private boolean getInvertedBooleanValue(int file, String section, String key, boolean defaultValue)
	{
		try
		{
			return !((BooleanSetting) mSettings.get(file).get(section).getSetting(key)).getValue();
		}
		catch (NullPointerException ex)
		{
			return defaultValue;
		}
	}

	private int getVideoBackendValue()
	{
		int videoBackendValue;

		try
		{
			String videoBackend = ((StringSetting)mSettings.get(SettingsFile.SETTINGS_DOLPHIN).get(SettingsFile.SECTION_INI_CORE).getSetting(SettingsFile.KEY_VIDEO_BACKEND)).getValue();
			if (videoBackend.equals("OGL"))
			{
				videoBackendValue = 0;
			}
			else if (videoBackend.equals("Vulkan"))
			{
				videoBackendValue = 1;
			}
			else if (videoBackend.equals("Software Renderer"))
			{
				videoBackendValue = 2;
			}
			else if (videoBackend.equals("Null"))
			{
				videoBackendValue = 3;
			}
			else
			{
				videoBackendValue = 0;
			}
		}
		catch (NullPointerException ex)
		{
			videoBackendValue = 0;
		}

		return videoBackendValue;
	}

	private int getExtensionValue(int wiimoteNumber)
	{
		int extensionValue;

		try
		{
			String extension = ((StringSetting)mSettings.get(SettingsFile.SETTINGS_WIIMOTE).get(SettingsFile.SECTION_WIIMOTE + wiimoteNumber).getSetting(SettingsFile.KEY_WIIMOTE_EXTENSION)).getValue();
			if (extension.equals("None"))
			{
				extensionValue = 0;
			}
			else if (extension.equals("Nunchuk"))
			{
				extensionValue = 1;
			}
			else if (extension.equals("Classic"))
			{
				extensionValue = 2;
			}
			else if (extension.equals("Guitar"))
			{
				extensionValue = 3;
			}
			else if (extension.equals("Drums"))
			{
				extensionValue = 4;
			}
			else if (extension.equals("Turntable"))
			{
				extensionValue = 5;
			}
			else
			{
				extensionValue = 0;
			}
		}
		catch (NullPointerException ex)
		{
			extensionValue = 0;
		}

		return extensionValue;
	}
}
