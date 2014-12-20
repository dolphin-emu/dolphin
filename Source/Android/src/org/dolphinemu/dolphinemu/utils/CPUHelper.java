package org.dolphinemu.dolphinemu.utils;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

import org.dolphinemu.dolphinemu.R;

import android.content.Context;
import android.os.Build;
import android.util.Log;

/**
 * Utility class for retrieving information
 * from a device's CPU.
 */
public final class CPUHelper
{
	private int revision;
	private int variant;
	private int numCores;
	private String implementerID = "N/A";
	private String part = "N/A";
	private String hardware = "N/A";
	private String processorInfo = "N/A";
	private String features = "N/A";

	private final Context ctx;

	/**
	 * Constructor
	 * 
	 * @param ctx The current {@link Context}.
	 */
	public CPUHelper(Context ctx)
	{
		this.ctx = ctx;

		try
		{
			// TODO: Should do other architectures as well (x86 and MIPS).
			// Can do differentiating between platforms by using
			// android.os.Build.CPU_ABI.
			//
			// CPU_ABI.contains("armeabi") == get ARM info.
			// CPU_ABI.contains("x86") == get x86 info.
			// CPU_ABI.contains("mips") == get MIPS info.
			//
			// However additional testing should be done across devices,
			// I highly doubt /proc/cpuinfo retains the same formatting
			// on different architectures.
			//
			// If push comes to shove, we can simply spit out the cpuinfo
			// contents. I would like to avoid this if possible, however.

			if (Build.CPU_ABI.contains("arm"))
			{
				getARMInfo();
			}
			else
			{
				Log.e("CPUHelper", "CPU architecture not supported yet.");
			}
		}
		catch (IOException ioe)
		{
			Log.e("CPUHelper", ioe.getMessage());
		}
	}

	/**
	 * Gets the revision number of the CPU.
	 * 
	 * @return the revision number of the CPU.
	 */
	public int getRevision()
	{
		return revision;
	}


	/**
	 * Gets the CPU variant number.
	 * 
	 * @return the CPU variant number.
	 */
	public int getVariant()
	{
		return variant;
	}

	/**
	 * Gets the total number of cores in the CPU.
	 * 
	 * @return the total number of cores in the CPU.
	 */
	public int getNumCores()
	{
		return numCores;
	}

	/**
	 * Gets the name of the implementer of the CPU.
	 * 
	 * @return the name of the implementer of the CPU.
	 */
	public String getImplementer()
	{
		return implementerID;
	}

	/**
	 * Gets the specific processor type of the CPU.
	 * 
	 * @return the specific processor type.
	 */
	public String getProcessorType()
	{
		return part;
	}

	/**
	 * Gets the internal name of the hardware.
	 * 
	 * @return the internal name of the hardware.
	 */
	public String getHardware()
	{
		return hardware;
	}

	/**
	 * Get the processor info string.
	 * 
	 * @return the processor info string.
	 */
	public String getProcessorInfo()
	{
		return processorInfo;
	}

	/**
	 * Gets the features supported by the CPU.
	 * 
	 * @return the features supported by the CPU.
	 */
	public String getFeatures()
	{
		return features;
	}

	/**
	 * Whether or not this CPU is using the ARM architecture.
	 * 
	 * @return true if this CPU uses the ARM architecture; false otherwise.
	 */
	public static boolean isARM()
	{
		return Build.CPU_ABI.contains("arm");
	}

	/**
	 * Whether or not this CPU is using the ARM64 architecture.
	 *
	 * @return true if this CPU uses the ARM64 architecture; false otherwise.
	 */
	public static boolean isARM64() { return Build.CPU_ABI.contains("arm64"); }

	/**
	 * Whether or not this CPU is using the x86 architecture.
	 * 
	 * @return true if this CPU uses the x86 architecture; false otherwise.
	 */
	public static boolean isX86()
	{
		return Build.CPU_ABI.contains("x86");
	}

	/**
	 * Whether or not this CPU is using the MIPS architecture.
	 * 
	 * @return true if this CPU uses the MIPS architecture; false otherwise.
	 */
	public static boolean isMIPS()
	{
		return Build.CPU_ABI.contains("mips");
	}

	// Retrieves information for ARM CPUs.
	private void getARMInfo() throws IOException
	{
		File info = new File("/proc/cpuinfo");
		if (info.exists())
		{
			BufferedReader br = new BufferedReader(new FileReader(info));

			String line;
			while ((line = br.readLine()) != null)
			{
				if (line.contains("Processor\t:"))
				{
					this.processorInfo = parseLine(line);
				}
				else if (line.contains("Hardware\t:"))
				{
					this.hardware = parseLine(line);
				}
				else if (line.contains("Features\t:"))
				{
					this.features = parseLine(line);
				}
				else if (line.contains("CPU implementer\t:"))
				{
					this.implementerID = parseArmID(Integer.decode(parseLine(line)));
				}
				else if (line.contains("CPU part\t:"))
				{
					this.part = parseArmPartNumber(Integer.decode(parseLine(line)));
				}
				else if (line.contains("CPU revision\t:"))
				{
					this.revision = Integer.decode(parseLine(line));
				}
				else if (line.contains("CPU variant\t:"))
				{
					this.variant = Integer.decode(parseLine(line));
				}
				else if (line.contains("processor\t:")) // Lower case indicates a specific core number
				{
					this.numCores++;
				}
			}

			br.close();
		}
	}

	// Basic function for parsing cpuinfo format strings.
	// cpuinfo format strings consist of [label:info] parts.
	// We only want to retrieve the info portion so we split
	// them using ':' as a delimeter.
	private String parseLine(String line)
	{
		String[] temp = line.split(":");
		if (temp.length != 2)
			return "N/A";

		return temp[1].trim();
	}

	// Parses an ARM CPU ID.
	private String parseArmID(int id)
	{
		switch (id)
		{
			case 0x41:
				return "ARM Limited";
			
			case 0x44:
				return "Digital Equipment Corporation";
		
			case 0x4D:
				return "Freescale Semiconductor Inc.";

			case 0x4E:
				return "Nvidia Corporation";

			case 0x51:
				return "Qualcomm Inc.";

			case 0x56:
				return "Marvell Semiconductor Inc.";

			case 0x69:
				return "Intel Corporation";

			default:
				return "N/A";
		}
	}

	// Parses the ARM CPU Part number.
	private String parseArmPartNumber(int partNum)
	{
		switch (partNum)
		{
			// Qualcomm part numbers.
			case 0x00F:
				return "Qualcomm Scorpion";

			case 0x02D:
				return "Qualcomm Dual Scorpion";

			case 0x04D:
				return "Qualcomm Dual Krait";

			case 0x06F:
				return "Qualcomm Quad Krait";

			// Marvell Semiconductor part numbers
			case 0x131:
				return "Marvell Feroceon";

			case 0x581:
				return "Marvell PJ4/PJ4b";

			case 0x584:
				return "Marvell Dual PJ4/PJ4b";

			// Official ARM part numbers.
			case 0x920:
				return "ARM920";

			case 0x922:
				return "ARM922";

			case 0x926:
				return "ARM926";

			case 0x940:
				return "ARM940";

			case 0x946:
				return "ARM946";

			case 0x966:
				return "ARM966";

			case 0x968:
				return "ARM968";

			case 0xB02:
				return "ARM11 MPCore";

			case 0xB36:
				return "ARM1136";

			case 0xB56:
				return "ARM1156";

			case 0xB76:
				return "ARM1176";

			case 0xC05:
				return "ARM Cortex A5";

			case 0xC07:
				return "ARM Cortex-A7 MPCore";

			case 0xC08:
				return "ARM Cortex A8";

			case 0xC09:
				return "ARM Cortex A9";

			case 0xC0C:
				return "ARM Cortex A12";

			case 0xC0F:
				return "ARM Cortex A15";

			case 0xC14:
				return "ARM Cortex R4";

			case 0xC15:
				return "ARM Cortex R5";

			case 0xC20:
				return "ARM Cortex M0";

			case 0xC21:
				return "ARM Cortex M1";

			case 0xC23:
				return "ARM Cortex M3";

			case 0xC24:
				return "ARM Cortex M4";

			case 0xC60:
				return "ARM Cortex M0+";

			case 0xD03:
				return "ARM Cortex A53";

			case 0xD07:
				return "ARM Cortex A57 MPCore";


			default: // Unknown/Not yet added to list.
				return String.format(ctx.getString(R.string.unknown_part_num), partNum);
		}
	}
}
