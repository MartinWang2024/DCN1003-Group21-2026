add_subdirectory(protobuf)
add_subdirectory(sqlite3 EXCLUDE_FROM_ALL)
add_subdirectory(openssl)

if(TARGET SQLite3 AND NOT TARGET sqlite3)
	add_library(sqlite3 ALIAS SQLite3)
endif()