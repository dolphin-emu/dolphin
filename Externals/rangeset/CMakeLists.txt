add_library(RangeSet::RangeSet INTERFACE IMPORTED GLOBAL)
set_target_properties(RangeSet::RangeSet PROPERTIES
	INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_LIST_DIR}/include
)
