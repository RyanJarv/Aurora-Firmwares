# Project Name
TARGET = FataMorgana

# Version
VERSION_MAJOR = 1
VERSION_MINOR = 0
VERSION_PATCH = 0
VERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)

# Build Project for Daisy Bootloader
# Retain the upstream QSPI default. The original alpha reported USB host
# trouble in SRAM mode without documenting the setup or symptom.
APP_TYPE = BOOT_QSPI

# Sources
CPP_SOURCES = src/FataMorgana.cpp

# FatFS unicode support (required for long filenames)
C_SOURCES = $(AURORA_SDK_PATH)/libs/libDaisy/Middlewares/Third_Party/FatFs/src/option/ccsbcs.c

# Path to the root of the Aurora-SDK
AURORA_SDK_PATH = ../..

# Location of Hardware Support File within the SDK
C_INCLUDES += -I$(AURORA_SDK_PATH)/include/
C_INCLUDES += -Isrc/

# Library Locations
LIBDAISY_DIR = $(AURORA_SDK_PATH)/libs/libDaisy/
DAISYSP_DIR = $(AURORA_SDK_PATH)/libs/DaisySP/

# Distribution directory
DIST_DIR = dist

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Keep RAM objects and binaries separate from the QSPI build.
.PHONY: ram
ram:
	$(MAKE) APP_TYPE=BOOT_SRAM BUILD_DIR=build-ram all

# Distribution target - creates versioned binary in dist/
.PHONY: dist clean-dist

dist: $(BUILD_DIR)/$(TARGET).bin
	@mkdir -p $(DIST_DIR)
	cp $(BUILD_DIR)/$(TARGET).bin $(DIST_DIR)/$(TARGET)-$(VERSION).bin

clean-dist:
	rm -rf $(DIST_DIR)
