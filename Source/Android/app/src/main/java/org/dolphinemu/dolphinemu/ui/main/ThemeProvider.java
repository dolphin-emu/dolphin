package org.dolphinemu.dolphinemu.ui.main;

public interface ThemeProvider
{
  /**
   * Provides theme ID by overriding an activity's 'setTheme' method and returning that result
   */
  int getThemeId();
}
