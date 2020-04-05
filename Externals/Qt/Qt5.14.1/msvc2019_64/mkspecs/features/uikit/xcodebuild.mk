
# We don't want xcodebuild to run in parallel
.NOTPARALLEL:

# Functions
targets = $(foreach target, $(EXPORT_SUBTARGETS), $(target)-$(strip $(1)))
toupper = $(shell echo $1 | tr '[:lower:]' '[:upper:]')
tolower = $(shell echo $1 | tr '[:upper:]' '[:lower:]')
basesdk = $(shell echo $1 | sed 's/[0-9.]*$$//')

# Explicit comma variable
, := ,

# Default targets
first: build
all: build_all

.DEFAULT_GOAL = first

# Top level targets
build: build_first
clean: clean_first
install: install_first
check: check_first
distclean: clean_all

$(EXPORT_SUBTARGETS): % : %-build

# Generic targets
%_first: $(EXPORT_PRE_TARGETDEPS) $(firstword $(call targets, %)) ;
%_all: $(EXPORT_PRE_TARGETDEPS) $(call targets, %) ;

# Actions
%-build: ACTION = build
%-build: xcodebuild-% ;

%-clean: ACTION = clean
%-clean: xcodebuild-% ;

%-install: ACTION = install
%-install: xcodebuild-% ;

# Simulator doesn't support archiving
%-simulator-install: ACTION = build
simulator-install: ACTION = build

# Limit check to a single configuration
%-device-check: check-device ;
%-simulator-check: check-simulator ;

# SDK
%-device: SDK = $(DEVICE_SDK)
%-simulator: SDK = $(SIMULATOR_SDK)

# Configuration
release-%: CONFIGURATION = Release
debug-%: CONFIGURATION = Debug

MAKEFILE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Test and build (device) destinations
ifneq ($(filter check%,$(MAKECMDGOALS)),)
  ifeq ($(DEVICES),)
    $(info Enumerating test destinations (you may override this by setting DEVICES explicitly), please wait...)
    DESTINATIONS_INCLUDE = /tmp/device_destinations.mk
    $(shell $(MAKEFILE_DIR)device_destinations.sh $(TARGET) $(EXPORT_DEVICE_FILTER) > $(DESTINATIONS_INCLUDE))
    include $(DESTINATIONS_INCLUDE)
  endif
endif

%-simulator: DEVICES = $(firstword $(SIMULATOR_DEVICES))
%-device: DEVICES = $(HARDWARE_DEVICES)

GENERIC_DEVICE_DESTINATION := $(EXPORT_GENERIC_DEVICE_DESTINATION)
GENERIC_SIMULATOR_DESTINATION := "id=$(shell $(MAKEFILE_DIR)devices.py $(EXPORT_DEVICE_FILTER) | tail -n 1)"

%-simulator: DESTINATION = $(if $(DESTINATION_ID),"id=$(DESTINATION_ID)",$(GENERIC_SIMULATOR_DESTINATION))
%-device: DESTINATION = $(if $(DESTINATION_ID),"id=$(DESTINATION_ID)",$(GENERIC_DEVICE_DESTINATION))

XCODE_VERSION_MAJOR := $(shell xcodebuild -version | grep Xcode | sed -e 's/Xcode //' | sed -e 's/\..*//')

ifeq ($(shell test $(XCODE_VERSION_MAJOR) -gt 7; echo $$?),0)
  XCODEBUILD_FLAGS += $(shell echo "$(MAKEFLAGS)" | sed -e 's/\([^ ]*\).*/\1/' | grep -qv 's' || echo -quiet)
endif

# Xcodebuild

DESTINATION_MESSAGE = "Running $(call tolower,$(CONFIGURATION)) $(ACTION) \
  on '$(DESTINATION_NAME)' ($(DESTINATION_ID))$(if $(DESTINATION_OS),$(,) $(DESTINATION_PLATFORM) $(DESTINATION_OS),)"

xcodebuild-%:
		@$(if $(DESTINATION_NAME), echo $(DESTINATION_MESSAGE),)
		xcodebuild $(ACTION) $(XCODEBUILD_FLAGS) -project $(TARGET).xcodeproj -scheme $(TARGET) $(if $(SDK), -sdk $(SDK),) $(if $(CONFIGURATION), -configuration $(CONFIGURATION),) $(if $(DESTINATION), -destination $(DESTINATION) -destination-timeout 1,) $(if $(DESTINATION_ID),, ENABLE_ONLY_ACTIVE_RESOURCES=NO) $(if $(INSTALL_ROOT), DSTROOT=$(INSTALL_ROOT),)

xcodebuild-check-device_%: DESTINATION_ID=$(lastword $(subst _, ,$@))

# Special check target (requires SECONDEXPANSION due to devices)
.SECONDEXPANSION:
check-%: ACTION = test
check-%: $$(foreach device, $$(DEVICES), xcodebuild-check-device_$$(device)) ;
	  @echo $(if $^, Ran $(call tolower,$(CONFIGURATION)) tests on $(words $^) $(SDK) destination\(s\): $(DEVICES), No compatible test devices found for \'$(SDK)\' SDK && false)

# Determined by device
check-%: SDK =

# Default to debug for testing
check-%: CONFIGURATION = Debug

