include_directories(
	../include
)

add_library(semver
	Semver200_comparator.cpp Semver200_parser.cpp Semver200_modifier.cpp
)
