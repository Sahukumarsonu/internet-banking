cmake_minimum_required(VERSION 3.16)
project(internet_banking_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Crow is header-only; it's expected to be available as a system package
# (installed in the Dockerfile) or vendored under third_party/crow.
find_path(CROW_INCLUDE_DIR crow.h PATHS /usr/local/include /usr/include third_party/crow/include)
if(NOT CROW_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find crow.h. Install Crow (see Dockerfile) or vendor it under backend/third_party/crow/include.")
endif()

find_path(SQLITE3_INCLUDE_DIR sqlite3.h)
find_library(SQLITE3_LIBRARY sqlite3)
if(NOT SQLITE3_INCLUDE_DIR OR NOT SQLITE3_LIBRARY)
    message(FATAL_ERROR "Could not find libsqlite3-dev. Install it (see Dockerfile).")
endif()

# Boost.Asio is required by Crow for networking, even though we don't call it directly.
find_package(Boost REQUIRED COMPONENTS system)

add_executable(bank_server
    main.cpp
    database.cpp
    auth.cpp
    account.cpp
    transaction.cpp
)

target_include_directories(bank_server PRIVATE ${CROW_INCLUDE_DIR} ${SQLITE3_INCLUDE_DIR})
target_link_libraries(bank_server PRIVATE
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${SQLITE3_LIBRARY}
    Boost::system
)

# Copy the SQL schema/seed next to the binary so the runtime path lookups
# in main.cpp ("./database/schema.sql") resolve correctly in Docker.
add_custom_command(TARGET bank_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:bank_server>/database
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/../database/schema.sql
        ${CMAKE_SOURCE_DIR}/../database/seed.sql
        $<TARGET_FILE_DIR:bank_server>/database/
)
