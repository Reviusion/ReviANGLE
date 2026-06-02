cmake_minimum_required(VERSION 3.20)

# ReviANGLE Uninstaller GUI
add_executable(ReviANGLE-Uninstall WIN32
    main.cpp
    uninstaller.rc
)

# Link against Windows libraries
target_link_libraries(ReviANGLE-Uninstall
    PRIVATE
    shlwapi.lib
    shell32.lib
    advapi32.lib
    kernel32.lib
    user32.lib
)

# Set output directory
set_target_properties(ReviANGLE-Uninstall PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Set subsystem to Windows (no console)
set_target_properties(ReviANGLE-Uninstall PROPERTIES
    VS_SUBSYSTEM "windows"
)
