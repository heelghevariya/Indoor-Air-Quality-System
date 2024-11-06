# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

# Add the executable target
#add_executable(app src/main.c)

target_sources(app PRIVATE src/main.c
	# src/sps30.c
    # src/sensirion_common.c
    # src/hal.c
	# src/sensirion_i2c.c
	# src/sensirion_i2c_hal.c
	src/sensors/sps30/sps30.c
    src/sensors/sps30/hal.c
    src/sensors/scd41/scd4x_i2c.c
    src/sensors/scd41/sensirion_common.c
    src/sensors/scd41/sensirion_i2c_hal.c
    src/sensors/scd41/sensirion_i2c.c
	src/cJSON.h
    src/cJSON.c )

# Optionally, if you have header files, you can include their directories
# target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)