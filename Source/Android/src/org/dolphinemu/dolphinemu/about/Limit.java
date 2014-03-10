/**
 * Copyright 2014 Dolphin Emulator Project
 * Licensed under GPLv2
 * Refer to the license.txt file included.
 */

package org.dolphinemu.dolphinemu.about;

import org.dolphinemu.dolphinemu.utils.EGLHelper;

final class Limit
{
	/**
	 * Possible types a Limit can be.
	 */
	enum Type
	{
		/** Generic string */
		STRING,
		/** Integer constant */
		INTEGER,
		/** Range of integers */
		INTEGER_RANGE,
	}

	/** Name of this limit */
	public final String name;
	/** The GL constant that represents this limit.*/
	public final int glEnum;
	/** The {@link Type} of this limit. */
	public final Type type;

	/**
	 * Constructor
	 * 
	 * @param name   Name of the limit.
	 * @param glEnum The GL constant that represents this limit.
	 * @param type   The {@link Type} of this limit.
	 */
	public Limit(String name, int glEnum, Type type)
	{
		this.name = name;
		this.glEnum = glEnum;
		this.type = type;
	}

	/**
	 * Retrieves the information represented by this limit.
	 * 
	 * @param context {@link EGLHelper} context to retrieve the limit with.
	 * 
	 * @return the information represented by this limit.
	 */
	public String GetValue(EGLHelper context)
	{
		if (type == Type.INTEGER)
		{
			return Integer.toString(context.glGetInteger(glEnum));
		}
		else if (type == Type.INTEGER_RANGE)
		{
			int[] data = new int[2];
			context.getGL().glGetIntegerv(glEnum, data, 0);

			return String.format("[%d, %d]", data[0], data[1]);
		}

		// If neither of the above type, assume it's a string.
		return context.glGetString(glEnum);
	}
}