#pragma once

#include <string>

// This class is currently a shim for the Rust codes interface. We're doing it this way
// to migrate things over without needing to do larger invasive changes.
//
// The remaining methods on here are simply layers that direct the call over to the Rust
// side. A quirk of this is that we're using the EXI device pointer, so this class absolutely
// cannot outlive the EXI device - but we control that and just need to do our due diligence
// when making changes.
class SlippiDirectCodes
{
  public:
    // We can't currently expose `SlippiRustExtensions.h` in header files, so
	  // we export these two types for code clarity and map them in the implementation.
    static const uint8_t DIRECT = 0;
	  static const uint8_t TEAMS = 1;

	  SlippiDirectCodes(uintptr_t rs_exi_device_ptr, uint8_t kind);
	  ~SlippiDirectCodes();

	  std::string get(int index);
	  int length();
	  void AddOrUpdateCode(std::string code);

  protected:
	  // A pointer to a "shadow" EXI Device that lives on the Rust side of things.
	  // Do *not* do any cleanup of this! The EXI device will handle it.
	  uintptr_t slprs_exi_device_ptr;

	  // An internal marker for what kind of codes we're reading/reporting.
	  uint8_t kind;
};
