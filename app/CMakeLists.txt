# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             native-lib

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/cpp/native-lib.cpp
             src/main/cpp/h264_sei.cpp
             src/main/cpp/h264_stream.cpp
             src/main/cpp/mediacodec.cpp
             src/main/cpp/udp_test.cpp
             src/main/cpp/senderThread.cpp
             src/main/cpp/FramePacket.cpp
             src/main/cpp/live555.cpp
             src/main/cpp/Rtsp.cpp
        )
add_library(
            yuv-lib
            STATIC
            IMPORTED
        )
set_target_properties(
        yuv-lib
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/cpp/include/${ANDROID_ABI}/libyuv_static.a
        )

add_library(
        live555-lib
        SHARED
        IMPORTED
)
set_target_properties(
        live555-lib
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/cpp/include/${ANDROID_ABI}/liblive555.so
)

include_directories(
        src/main/cpp/include
        )
include_directories(
        src/main/cpp/include/live/BasicUsageEnvironment/include
)
include_directories(
        src/main/cpp/include/live/groupsock/include
)
include_directories(
        src/main/cpp/include/live/liveMedia/include
)
include_directories(
        src/main/cpp/include/live/UsageEnvironment/include
)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )
find_library( # Sets the name of the path variable.
              media-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              mediandk )
find_library( # Sets the name of the path variable.
              window-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              android )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       native-lib

                       # Links the target library to the log library
                       # included in the NDK.
                       yuv-lib
                       live555-lib
                       ${log-lib}
                       ${media-lib}
                       ${window-lib} )