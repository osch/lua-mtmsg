#########################################################################################
#
# Minimal cmake file to support other platforms or build systems.
#
# This cmake file is only used for compiling and linking. It does not find out
# system configuration and it does not install.
#
# For Linux use the Makefile in same directory than this file.
# The Makefile will automatically setup everything for linux, uses cmake
# for compiling/linking and also has an install target.
#
#########################################################################################
#
# Use the following variables to configure compiling & linking:
#

SET ( LUAMTMSG_COMPILE_FLAGS  CACHE STRING "" )
SET ( LUAMTMSG_LINK_FLAGS     CACHE STRING "" )

#########################################################################################

PROJECT ( luamtmsg C )
CMAKE_MINIMUM_REQUIRED ( VERSION 2.8 )

SET ( CMAKE_C_FLAGS  
      "${CMAKE_C_FLAGS} ${LUAMTMSG_COMPILE_FLAGS}" )

SET ( CMAKE_SHARED_LINKER_FLAGS 
      "${CMAKE_SHARED_LINKER_FLAGS} ${LUAMTMSG_LINK_FLAGS}")

ADD_LIBRARY (
	luamtmsg SHARED
	src/main.c
	src/buffer.c
	src/listener.c
	src/util.c
	src/async_util.c
	src/mtmsg_compat.c
)

TARGET_COMPILE_DEFINITIONS (
	luamtmsg PRIVATE
)
