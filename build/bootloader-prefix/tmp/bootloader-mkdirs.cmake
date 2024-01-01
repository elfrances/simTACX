# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/mmourier/ESP32/esp-idf-v5.1.1/components/bootloader/subproject"
  "/home/mmourier/simTACX/build/bootloader"
  "/home/mmourier/simTACX/build/bootloader-prefix"
  "/home/mmourier/simTACX/build/bootloader-prefix/tmp"
  "/home/mmourier/simTACX/build/bootloader-prefix/src/bootloader-stamp"
  "/home/mmourier/simTACX/build/bootloader-prefix/src"
  "/home/mmourier/simTACX/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/mmourier/simTACX/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/mmourier/simTACX/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
