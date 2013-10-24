# Konfiguriert gcc:  stellt sicher, daß `#include "foo.h"' auch im aktuellen Verzeichnis sucht
set(CMAKE_INCLUDE_CURRENT_DIR ON)


# Aktiviere Unterstützung für Parser-Generator
find_package(BISON)
# Aktiviere Unterstützung für Lexer-Generator
find_package(FLEX)

# Codegenerierung: parser.y -> parser.c
BISON_TARGET(Parser ${CMAKE_CURRENT_SOURCE_DIR}/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)
# Codegenerierung: lexer.l -> lexer.c
FLEX_TARGET(Lexer ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l  ${CMAKE_CURRENT_BINARY_DIR}/lexer.c)
# Codegenerierung: Abhängigkeiten:  Lexer liefert Daten an parser
ADD_FLEX_BISON_DEPENDENCY(Lexer Parser)

# Generierte Header-Dateien für Lexer/Parser werden vom Rest des Programmes gebraucht
include_directories(${CMAKE_CURRENT_BINARY_DIR})




# Unit test suites
# Aktiviere Unterstützung für add_test
enable_testing()
include(CTest)
# Binde Check-Bibliothek zwingend ein:
find_package(Check REQUIRED)
include_directories(${CHECK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${CHECK_LIBRARIES})

add_executable(test-lexer test-lexer.c ast.c ${FLEX_Lexer_OUTPUTS} ${BISON_Parser_OUTPUTS})
target_link_libraries(test-lexer ${LIBS})
add_test(NAME TestLexer  COMMAND $<TARGET_FILE:test-lexer>)
