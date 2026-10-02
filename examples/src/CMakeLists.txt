# avdecc examples

# Examples are using curses on Linux and macOS (but not if the generator is Xcode)
set(CURSES_COMPILE_OPTIONS "")
set(CURSES_LINK_OPTIONS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	# Check if the generator is not Xcode
	if(NOT CMAKE_GENERATOR STREQUAL "Xcode")
		set(CURSES_COMPILE_OPTIONS -DUSE_CURSES)
		set(CURSES_LINK_OPTIONS ncurses)
	endif()
endif()

# RawMessageSend
add_executable(RawMessageSend rawMessageSenderCxx.cpp utils.cpp utils.hpp)
set_target_properties(RawMessageSend PROPERTIES FOLDER "Examples")
# Using avdecc library
target_link_libraries(RawMessageSend PRIVATE la_avdecc_cxx)
# Add (optional) curses support
target_compile_options(RawMessageSend PRIVATE ${CURSES_COMPILE_OPTIONS})
target_link_libraries(RawMessageSend PRIVATE ${CURSES_LINK_OPTIONS})
# Setup common options
cu_setup_executable_options(RawMessageSend)
# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(RawMessageSend ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG})

# SimpleController
add_executable(SimpleController simpleController.cpp utils.cpp utils.hpp)
set_target_properties(SimpleController PROPERTIES FOLDER "Examples")
# Using avdecc library
target_link_libraries(SimpleController PRIVATE la_avdecc_cxx)
# Add (optional) curses support
target_compile_options(SimpleController PRIVATE ${CURSES_COMPILE_OPTIONS})
target_link_libraries(SimpleController PRIVATE ${CURSES_LINK_OPTIONS})
# Setup common options
cu_setup_executable_options(SimpleController)
# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(SimpleController ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG})
# Copy data
add_custom_command(
	TARGET SimpleController
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/../data/SimpleControllerModelV2.json ${CMAKE_BINARY_DIR}/examples/src/SimpleControllerModelV2.json
	COMMENT "Copying Examples data to output folder"
	VERBATIM
)

# StreamFormatInfo
add_executable(StreamFormatInfo streamFormatInfo.cpp)
set_target_properties(StreamFormatInfo PROPERTIES FOLDER "Examples")
# Using avdecc library
target_link_libraries(StreamFormatInfo PRIVATE la_avdecc_cxx)
# Setup common options
cu_setup_executable_options(StreamFormatInfo)
# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(StreamFormatInfo ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG})

# C bindings examples
if(BUILD_AVDECC_BINDINGS_C)
	# C bindings Example (using c++ code)
	add_executable(RawMessageSendC rawMessageSenderC.cpp utils.cpp utils.hpp)
	set_target_properties(RawMessageSendC PROPERTIES FOLDER "Examples/Bindings/C")
	# Use C bindings in utils.cpp
	target_compile_options(RawMessageSendC PRIVATE -DUSE_BINDINGS_C)
	# Using avdecc library
	target_link_libraries(RawMessageSendC PRIVATE la_avdecc_c)
	# Add (optional) curses support
	target_compile_options(RawMessageSendC PRIVATE ${CURSES_COMPILE_OPTIONS})
	target_link_libraries(RawMessageSendC PRIVATE ${CURSES_LINK_OPTIONS})
	# Setup NIH
	target_link_libraries(RawMessageSendC PRIVATE la_networkInterfaceHelper_static)
	# Setup common options
	cu_setup_executable_options(RawMessageSendC)
	# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
	cu_setup_deploy_runtime(RawMessageSendC ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG})
endif()

# Controller library examples
if(BUILD_AVDECC_CONTROLLER)
	# Using the MacOSNCap ProtocolInterface requires the com.apple.developer.networking.custom-protocol entitlement, which is a restricted entitlement: macOS kills the process at launch unless a provisioning profile allowing it is embedded in the code.
	# A provisioning profile can only be embedded in a bundle, so the discovery examples are built as app bundles when MacOSNCap is enabled.
	# They remain regular console programs, just run <Name>.app/Contents/MacOS/<Name> from a terminal (stdin, stdout and curses work as usual).
	# All discovery examples share the same bundle identifier, so the provisioning profile of the project (CU_PROVISIONING_PROFILE, from the 'provisioning_profile' option of the config file) is used for all of them.
	# When avdecc is not the top level project, CU_PROVISIONING_PROFILE is the profile of the parent project (for another App ID) and is not used.
	set(DISCOVERY_EXAMPLES_BUNDLE_IDENTIFIER "com.l-acoustics.avdecc.discovery")
	set(DISCOVERY_EXAMPLES_USE_NCAP FALSE)
	if(APPLE AND BUILD_AVDECC_INTERFACE_MAC_NCAP)
		if(ENABLE_CODE_SIGNING AND CU_PROVISIONING_PROFILE AND LA_avdecc_IS_TOP_LEVEL)
			set(DISCOVERY_EXAMPLES_USE_NCAP TRUE)
		else()
			message(WARNING "The discovery examples will not be able to use the MacOSNCap ProtocolInterface: it requires code signing and a provisioning profile for the '${DISCOVERY_EXAMPLES_BUNDLE_IDENTIFIER}' App ID ('provisioning_profile' option of the config file, or CU_PROVISIONING_PROFILE).")
		endif()
	endif()

	# Setup a discovery example so it can use the MacOSNCap ProtocolInterface (see above).
	# Sets OUT_DEPLOY_OPTIONS to the additional options to pass to cu_setup_deploy_runtime.
	function(setup_discovery_example_for_ncap TARGET_NAME OUT_DEPLOY_OPTIONS)
		set(_DEPLOY_OPTIONS "")
		if(APPLE AND BUILD_AVDECC_INTERFACE_MAC_NCAP)
			set_target_properties(${TARGET_NAME} PROPERTIES
				MACOSX_BUNDLE TRUE
				MACOSX_BUNDLE_INFO_PLIST "${CU_ROOT_DIR}/examples/data/Discovery-Info.plist.in"
				MACOSX_BUNDLE_GUI_IDENTIFIER "${DISCOVERY_EXAMPLES_BUNDLE_IDENTIFIER}"
			)
			# Only claim the restricted entitlement when the provisioning profile is embedded, otherwise the process would be killed at launch (preventing the use of the other ProtocolInterfaces)
			if(DISCOVERY_EXAMPLES_USE_NCAP)
				list(APPEND _DEPLOY_OPTIONS CODESIGN_ENTITLEMENTS "${CU_ROOT_DIR}/examples/data/Discovery.entitlements" PROVISION_PROFILE "${CU_PROVISIONING_PROFILE}")
			endif()
		endif()
		set(${OUT_DEPLOY_OPTIONS} ${_DEPLOY_OPTIONS} PARENT_SCOPE)
	endfunction()

	# Discovery
	add_executable(Discovery discovery.cpp utils.cpp utils.hpp)
	set_target_properties(Discovery PROPERTIES FOLDER "Examples/Controller")
	# Using controller library
	target_link_libraries(Discovery PRIVATE la_avdecc_controller_cxx)
	# Add (optional) curses support
	target_compile_options(Discovery PRIVATE ${CURSES_COMPILE_OPTIONS})
	target_link_libraries(Discovery PRIVATE ${CURSES_LINK_OPTIONS})
	# Setup common options
	cu_setup_executable_options(Discovery)
	# Setup MacOSNCap support
	setup_discovery_example_for_ncap(Discovery DISCOVERY_DEPLOY_OPTIONS)
	# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
	cu_setup_deploy_runtime(Discovery ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG} ${DISCOVERY_DEPLOY_OPTIONS})

	# SimpleDiscovery - minimal CLI discovery sample (primarily intended for MacOSNCap testing)
	add_executable(SimpleDiscovery simpleDiscovery.cpp)
	set_target_properties(SimpleDiscovery PROPERTIES FOLDER "Examples/Controller")
	# Using controller library
	target_link_libraries(SimpleDiscovery PRIVATE la_avdecc_controller_cxx)
	# Setup common options
	cu_setup_executable_options(SimpleDiscovery)
	# Setup MacOSNCap support
	setup_discovery_example_for_ncap(SimpleDiscovery SIMPLEDISCOVERY_DEPLOY_OPTIONS)
	# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
	cu_setup_deploy_runtime(SimpleDiscovery ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG} ${SIMPLEDISCOVERY_DEPLOY_OPTIONS})

	if(ENABLE_AVDECC_FEATURE_JSON)
		# EntityDumper
		add_executable(EntityDumper entityDumper.cpp utils.cpp utils.hpp)
		set_target_properties(EntityDumper PROPERTIES FOLDER "Examples/Controller")
		# Using controller library
		target_link_libraries(EntityDumper PRIVATE la_avdecc_controller_cxx)
		# Add (optional) curses support
		target_compile_options(EntityDumper PRIVATE ${CURSES_COMPILE_OPTIONS})
		target_link_libraries(EntityDumper PRIVATE ${CURSES_LINK_OPTIONS})
		# Setup common options
		cu_setup_executable_options(EntityDumper)
		# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
		cu_setup_deploy_runtime(EntityDumper ${INSTALL_EXAMPLE_FLAG} ${SIGN_FLAG})
	endif()

	if(BUILD_AVDECC_SWIG AND AVDECC_SWIG_LANGUAGES MATCHES "csharp")
		# Only include if the CMake generator is single-config
		if(CMAKE_CONFIGURATION_TYPES)
			message("CMake generator is multi-config, skipping C# example")
		else()
			# Network interfaces enumerator using C# bindings
			add_subdirectory(discoveryCSharp)
			# StreamFormatInfo using C# bindings
			add_subdirectory(streamFormatInfoCSharp)
		endif()
	endif()
endif()
