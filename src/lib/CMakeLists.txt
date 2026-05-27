set(SCROBBLE_SOURCES
    libscrobble.cpp
    submit.cpp
    parse-log.cpp
    parse-ipod.cpp
    common.cpp
    conf.cpp
    gettrackinfo.cpp
    dbcache.cpp
    # Headers with Q_OBJECT must be listed so AUTOMOC processes them
    parse.h
    libscrobble.h
    submit.h
    parse-log.h
    parse-ipod.h
    conf.h
    gettrackinfo.h
    dbcache.h
)

if(HAVE_LIBMTP)
    list(APPEND SCROBBLE_SOURCES parse-mtp-libmtp.cpp parse-mtp.h)
endif()

add_library(scrobble STATIC ${SCROBBLE_SOURCES})

target_include_directories(scrobble PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(scrobble
    PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::Xml
        Qt6::Sql
)

if(HAVE_LIBMTP)
    target_compile_definitions(scrobble PUBLIC HAVE_LIBMTP)
    target_include_directories(scrobble PRIVATE ${LIBMTP_INCLUDE_DIRS})
    target_link_libraries(scrobble PRIVATE ${LIBMTP_LIBRARIES})
    target_compile_options(scrobble PRIVATE ${LIBMTP_CFLAGS_OTHER})
endif()
