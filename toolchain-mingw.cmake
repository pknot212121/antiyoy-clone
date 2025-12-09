set(CMAKE_SYSTEM_NAME Windows)

# Określ kompilatory (dla 64-bitowego Windowsa)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
# Opcjonalnie: Ścieżka do roota środowiska (może być wymagana przy linkowaniu bibliotek)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Ustawienia wyszukiwania bibliotek (szukaj tylko w środowisku docelowym)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)