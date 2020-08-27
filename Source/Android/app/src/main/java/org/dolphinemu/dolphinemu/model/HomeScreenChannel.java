package org.dolphinemu.dolphinemu.model;

/**
 * Represents a home screen channel for Android TV api 26+
 */
public class HomeScreenChannel
{

  private long channelId;
  private String name;
  private String description;
  private String appLinkIntentUri;

  public HomeScreenChannel()
  {
  }

  public HomeScreenChannel(String name, String description, String appLinkIntentUri)
  {
    this.name = name;
    this.description = description;
    this.appLinkIntentUri = appLinkIntentUri;
  }

  public long getChannelId()
  {
    return channelId;
  }

  public void setChannelId(long channelId)
  {
    this.channelId = channelId;
  }

  public String getName()
  {
    return name;
  }

  public void setName(String name)
  {
    this.name = name;
  }

  public String getDescription()
  {
    return description;
  }

  public void setDescription(String description)
  {
    this.description = description;
  }

  public String getAppLinkIntentUri()
  {
    return appLinkIntentUri;
  }

  public void setAppLinkIntentUri(String appLinkIntentUri)
  {
    this.appLinkIntentUri = appLinkIntentUri;
  }
}
