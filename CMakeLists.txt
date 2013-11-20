# Version von cmake
cmake_minimum_required(VERSION 2.8)
# CMake ist erweiterbar; wir legen Erweiterungen in cmake-modules ab:
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake-modules/")

# Projektname
project(c-moll)

# Aktiviere C99 (Vorsicht: funktioniert nur für gcc)
# Aktiviere POSIX-Unterstützung (Version bis September 2008)
# Aktiviert alle Warnungen des Übersetzers
set(CMAKE_C_FLAGS "-std=c99 -D_POSIX_C_SOURCE=200809 -Wall")

# Unterverzeichnisse, die cmake verwalten soll (Mehrfachnennungen möglich)
add_subdirectory(src)

################################################################################
# Testunterstützung

# Aktiviere Unterstützung für `make test'
enable_testing()

