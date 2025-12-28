cmake_minimum_required(VERSION 3.21)

# macOS-specific vanilla-pipe implementation
add_executable(vanilla-pipe
    main.c
    wifi.m
)

# Link Apple frameworks
target_link_libraries(vanilla-pipe PRIVATE
    "-framework CoreWLAN"
    "-framework SystemConfiguration"
    "-framework Foundation"
    "-framework Security"
)

# Include directories
target_include_directories(vanilla-pipe PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link with libvanilla
target_link_libraries(vanilla-pipe PRIVATE
    libvanilla
    pthread
)

# Install
install(TARGETS vanilla-pipe)
