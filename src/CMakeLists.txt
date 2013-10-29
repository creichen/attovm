# Konfiguriert gcc:  stellt sicher, daß `#include "foo.h"' auch im aktuellen Verzeichnis sucht
set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Aktiviere Unterstützung für Lexer-Generator
find_package(FLEX)

# Codegenerierung: lexer.l -> lexer.c
FLEX_TARGET(Lexer ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l  ${CMAKE_CURRENT_BINARY_DIR}/lexer.c)

# Generierte Header-Dateien für Lexer/Parser werden vom Rest des Programmes gebraucht
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Alle Komponenten des Pare-Teil des Frontends
set(PARSER_FRONTEND ast.c ${FLEX_Lexer_OUTPUTS} ${FLEX_Lexer_LIBRARIES} rd-parser.h rd-parser.c)

# Unit test suites
# Aktiviere Unterstützung für add_test
enable_testing()
include(CTest)
# Binde Check-Bibliothek zwingend ein:
find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CHECK_LIBRARIES})

add_executable(test-lexer test-lexer.c ${PARSER_FRONTEND})
target_link_libraries(test-lexer ${LIBS})
add_test(NAME TestLexer  COMMAND $<TARGET_FILE:test-lexer>)
