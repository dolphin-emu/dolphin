package org.dolphinemu.dolphinemu.folderbrowser;

import java.io.File;

/**
 * Represents an item in the folder browser list.
 */
public final class FolderBrowserItem implements Comparable<FolderBrowserItem>
{
    private final String name;
    private final String subtitle;
    private final String path;
    private final boolean isValid;
    private final File underlyingFile;
    
    /**
     * Constructor
     * 
     * @param name     The name of the file/folder represented by this item.
     * @param subtitle The subtitle of this FolderBrowserItem to display.
     * @param path     The path of the file/folder represented by this item.
     * @param isValid  Whether or not this item represents a file type that can be handled.
     */
    public FolderBrowserItem(String name, String subtitle, String path, boolean isValid)
    {
        this.name = name;
        this.subtitle = subtitle;
        this.path = path;
        this.isValid = isValid;
        this.underlyingFile = new File(path);
    }
    
    /**
     * Constructor. Initializes a FolderBrowserItem with an empty subtitle.
     * 
     * @param ctx      Context this FolderBrowserItem is being used in.
     * @param name     The name of the file/folder represented by this item.
     * @param path     The path of the file/folder represented by this item.
     * @param isValid  Whether or not this item represents a file type that can be handled.
     */
    public FolderBrowserItem(String name, String path, boolean isValid)
    {
        this.name = name;
        this.subtitle = "";
        this.path = path;
        this.isValid = isValid;
        this.underlyingFile = new File(path);
    }
    
    /**
     * Gets the name of the file/folder represented by this FolderBrowserItem.
     * 
     * @return the name of the file/folder represented by this FolderBrowserItem.
     */
    public String getName()
    {
        return name;
    }
    
    /**
     * Gets the subtitle text of this FolderBrowserItem.
     * 
     * @return the subtitle text of this FolderBrowserItem.
     */
    public String getSubtitle()
    {
        return subtitle;
    }
    
    /**
     * Gets the path of the file/folder represented by this FolderBrowserItem.
     * 
     * @return the path of the file/folder represented by this FolderBrowserItem.
     */
    public String getPath()
    {
        return path;
    }
    
    /**
     * Gets whether or not the file represented
     * by this FolderBrowserItem is supported
     * and can be handled correctly.
     * 
     * @return whether or not the file represented
     * by this FolderBrowserItem is supported
     * and can be handled correctly.
     */
    public boolean isValid()
    {
        return isValid;
    }
    
    /**
     * Gets the {@link File} representation of the underlying file/folder
     * represented by this FolderBrowserItem.
     * 
     * @return the {@link File} representation of the underlying file/folder
     * represented by this FolderBrowserItem.
     */
    public File getUnderlyingFile()
    {
        return underlyingFile;
    }
    
    /**
     * Gets whether or not this FolderBrowserItem represents a directory.
     * 
     * @return true if this FolderBrowserItem represents a directory, false otherwise.
     */
    public boolean isDirectory()
    {
        return underlyingFile.isDirectory();
    }
    
    public int compareTo(FolderBrowserItem other)
    {
        if(this.name != null)
            return this.name.toLowerCase().compareTo(other.getName().toLowerCase()); 
        else 
            throw new IllegalArgumentException();
    }
}
