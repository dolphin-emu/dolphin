// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/NonCopyable.h"

class PCAP;

// An interface used to capture and log structured data about internal DSP
// data transfers.
//
// Note: these calls are done within the DSP emulator in critical paths and at
// a high frequency. Implementations must try and avoid blocking for too long.
class DSPCaptureLogger
{
public:
	virtual ~DSPCaptureLogger() {}

	// Accesses (reads or writes) to memory mapped registers (external
	// interface, also known as IFX). These are always 16 bits accesses.
	virtual void LogIFXRead(u16 address, u16 read_value) = 0;
	virtual void LogIFXWrite(u16 address, u16 written_value) = 0;

	// DMAs to/from main memory from/to DSP memory. We let the interpretation
	// of the "control" field to the layer that analyze the logs and do not
	// perform our own interpretation of it. Thus there is only one call which
	// is used for DRAM/IRAM in any direction (to/from DSP).
	//
	// Length is expressed in bytes, not DSP words.
	virtual void LogDMA(u16 control, u32 gc_address, u16 dsp_address,
	                    u16 length, const u8* data) = 0;
};

// A dummy implementation of a capture logger that does nothing. This is the
// default implementation used by the DSP emulator.
//
// Can also be inherited from if you want to only override part of the methods.
class DefaultDSPCaptureLogger : public DSPCaptureLogger
{
public:
	void LogIFXRead(u16 address, u16 read_value) override {}
	void LogIFXWrite(u16 address, u16 written_value) override {}
	void LogDMA(u16 control, u32 gc_address, u16 dsp_address,
	                    u16 length, const u8* data) override {}
};

// A capture logger implementation that logs to PCAP files in a custom
// packet-based format.
class PCAPDSPCaptureLogger final : public DSPCaptureLogger, NonCopyable
{
public:
	// Automatically creates a writeable file (truncate existing file).
	PCAPDSPCaptureLogger(const std::string& pcap_filename);
	// Takes ownership of pcap.
	PCAPDSPCaptureLogger(PCAP* pcap);
	PCAPDSPCaptureLogger(std::unique_ptr<PCAP>&& pcap);

	void LogIFXRead(u16 address, u16 read_value) override
	{
		LogIFXAccess(true, address, read_value);
	}
	void LogIFXWrite(u16 address, u16 written_value) override
	{
		LogIFXAccess(false, address, written_value);
	}
	void LogDMA(u16 control, u32 gc_address, u16 dsp_address,
	                    u16 length, const u8* data) override;

private:
	void LogIFXAccess(bool read, u16 address, u16 value);

	std::unique_ptr<PCAP> m_pcap;
};
