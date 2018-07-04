package org.dolphinemu.dolphinemu.ui.settings;

public enum MenuTag
{
    CONFIG("config"),
    CONFIG_GENERAL("config_general"),
    CONFIG_INTERFACE("config_interface"),
    CONFIG_GAME_CUBE("config_gamecube"),
    CONFIG_WII("config_wii"),
    WIIMOTE("wiimote"),
    WIIMOTE_EXTENSION("wiimote_extension"),
    GCPAD_TYPE("gc_pad_type"),
    GRAPHICS("graphics"),
    HACKS("hacks"),
    ENHANCEMENTS("enhancements"),
    STEREOSCOPY("stereoscopy"),
    GCPAD_1("gcpad", 0),
    GCPAD_2("gcpad", 1),
    GCPAD_3("gcpad", 2),
    GCPAD_4("gcpad", 3),
    WIIMOTE_1("wiimote", 1),
    WIIMOTE_2("wiimote", 2),
    WIIMOTE_3("wiimote", 3),
    WIIMOTE_4("wiimote", 4),
    WIIMOTE_EXTENSION_1("wiimote_extension", 1),
    WIIMOTE_EXTENSION_2("wiimote_extension", 2),
    WIIMOTE_EXTENSION_3("wiimote_extension", 3),
    WIIMOTE_EXTENSION_4("wiimote_extension", 4);

    private String tag;
    private int subType = -1;

    MenuTag(String tag)
    {
        this.tag = tag;
    }

    MenuTag(String tag, int subtype)
    {
        this.tag = tag;
        this.subType = subtype;
    }

    @Override
    public String toString()
    {
        if (subType != -1)
        {
            return tag + subType;
        }

        return tag;
    }

    public String getTag()
    {
        return tag;
    }

    public int getSubType()
    {
        return subType;
    }

    public boolean isGCPadMenu()
    {
        return this == GCPAD_1 || this == GCPAD_2 || this == GCPAD_3 || this == GCPAD_4;
    }

    public boolean isWiimoteMenu()
    {
        return this == WIIMOTE_1 || this == WIIMOTE_2 || this == WIIMOTE_3 || this == WIIMOTE_4;
    }

    public boolean isWiimoteExtensionMenu()
    {
        return this == WIIMOTE_EXTENSION_1 || this == WIIMOTE_EXTENSION_2
                || this == WIIMOTE_EXTENSION_3 || this == WIIMOTE_EXTENSION_4;
    }

    public static MenuTag getGCPadMenuTag(int subtype)
    {
        return getMenuTag("gcpad", subtype);
    }

    public static MenuTag getWiimoteMenuTag(int subtype)
    {
        return getMenuTag("wiimote", subtype);
    }

    public static MenuTag getWiimoteExtensionMenuTag(int subtype)
    {
        return getMenuTag("wiimote_extension", subtype);
    }

    private static MenuTag getMenuTag(String tag, int subtype)
    {
        for (MenuTag menuTag : MenuTag.values())
        {
            if (menuTag.tag.equals(tag) && menuTag.subType == subtype) return menuTag;
        }

        throw new IllegalArgumentException("You are asking for a menu that is not available or " +
                "passing a wrong subtype");
    }
}
