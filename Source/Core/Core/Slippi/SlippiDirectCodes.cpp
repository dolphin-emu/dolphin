#include "SlippiRustExtensions.h"

#include "SlippiDirectCodes.h"

DirectCodeKind mapCode(uint8_t code_kind) {
	return code_kind == SlippiDirectCodes::DIRECT ?
		DirectCodeKind::DirectCodes :
		DirectCodeKind::TeamsCodes;
}

SlippiDirectCodes::SlippiDirectCodes(uintptr_t rs_exi_device_ptr, uint8_t code_kind)
{
	slprs_exi_device_ptr = rs_exi_device_ptr;
	kind = code_kind;
}

SlippiDirectCodes::~SlippiDirectCodes() {}

void SlippiDirectCodes::AddOrUpdateCode(std::string code)
{
	slprs_user_direct_codes_add_or_update(slprs_exi_device_ptr, mapCode(kind), code.c_str());
}

std::string SlippiDirectCodes::get(int index)
{
	char *code = slprs_user_direct_codes_get_code_at_index(slprs_exi_device_ptr, mapCode(kind), index);

	// To be safe, just do an extra copy into a full C++ string type - i.e, the ownership
	// that we're passing out from behind this method is clear.
	std::string connectCode = std::string(code);

	// Since the C string was allocated on the Rust side, we need to free it using that allocator.
	slprs_user_direct_codes_free_code(code);

	return connectCode;
}

int SlippiDirectCodes::length()
{
	return slprs_user_direct_codes_get_length(slprs_exi_device_ptr, mapCode(kind));
}
