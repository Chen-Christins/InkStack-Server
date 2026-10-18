# dependencies.cmake - Manage project dependencies

# Auto-detect chen-sdk directory by pattern "chen-sdk-*"
file(GLOB CHEN_SDK_DIRS "${PROJECT_SOURCE_DIR}/chen-sdk-*")
if (CHEN_SDK_DIRS)
    list(GET CHEN_SDK_DIRS 0 CHEN_SDK_DIR)
    message(STATUS "Using CHEN_SDK_DIR=${CHEN_SDK_DIR}")
else()
    message(FATAL_ERROR "chen-sdk-* not found under ${PROJECT_SOURCE_DIR}. Please fetch chen-sdk.")
endif()

include_directories(${CHEN_SDK_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/dbproxy/data)

# 三方依赖（由 deps.py sync / thirdparty 下载到 deps/：头文件 + 静态库）
# 仅用于编译 chen 头文件（protobuf/yaml-cpp/tinyxml2/jsoncpp/hiredis-vip），不静态链接进模块，
# 运行时符号由预编译 libchen.so 提供，避免重复符号。
set(THIRDPARTY_DEPS_DIR ${PROJECT_SOURCE_DIR}/deps)
if (EXISTS ${THIRDPARTY_DEPS_DIR}/include)
    include_directories(${THIRDPARTY_DEPS_DIR}/include)
    list(PREPEND CMAKE_PREFIX_PATH ${THIRDPARTY_DEPS_DIR})
    list(PREPEND CMAKE_LIBRARY_PATH ${THIRDPARTY_DEPS_DIR}/lib)
    list(PREPEND CMAKE_INCLUDE_PATH ${THIRDPARTY_DEPS_DIR}/include)
    set(Protobuf_ROOT ${THIRDPARTY_DEPS_DIR})
    set(Protobuf_USE_STATIC_LIBS ON)
    message(STATUS "Using THIRDPARTY_DEPS_DIR=${THIRDPARTY_DEPS_DIR}")
else()
    message(WARNING "deps/ 未就绪，运行 ./deps.py sync 下载三方依赖（头文件/静态库）")
endif()

add_subdirectory(${CHEN_SDK_DIR})
add_subdirectory(${PROJECT_SOURCE_DIR}/dbproxy/data)

# i18n: GNU gettext catalogs
find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_program(MSGFMT_EXECUTABLE msgfmt)
if(NOT MSGFMT_EXECUTABLE)
    message(WARNING "msgfmt not found; install gettext to compile locale catalogs")
endif()

set(I18N_MSGFMT_ARG "")
if(MSGFMT_EXECUTABLE)
    set(I18N_MSGFMT_ARG --msgfmt "${MSGFMT_EXECUTABLE}")
endif()

set(I18N_SCRIPT ${PROJECT_SOURCE_DIR}/scripts/i18n.py)
set(I18N_PO_DIR ${PROJECT_SOURCE_DIR}/locale/po)
set(I18N_MO_DIR ${PROJECT_SOURCE_DIR}/locale/C/LC_MESSAGES)
set(I18N_PO_FILES ${I18N_PO_DIR}/en.po ${I18N_PO_DIR}/zh_CN.po)
set(I18N_MO_FILES ${I18N_MO_DIR}/techhaven_en.mo ${I18N_MO_DIR}/techhaven_zh_CN.mo)

add_custom_command(
    OUTPUT ${I18N_MO_FILES}
    COMMAND ${Python3_EXECUTABLE} ${I18N_SCRIPT}
            --project-dir ${PROJECT_SOURCE_DIR} ${I18N_MSGFMT_ARG} check
    COMMAND ${Python3_EXECUTABLE} ${I18N_SCRIPT}
            --project-dir ${PROJECT_SOURCE_DIR} ${I18N_MSGFMT_ARG} build
    DEPENDS ${I18N_PO_FILES} ${I18N_SCRIPT}
            ${PROJECT_SOURCE_DIR}/errors.json
            ${PROJECT_SOURCE_DIR}/locale/techhaven.pot
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Checking and building gettext catalogs"
    VERBATIM
)

add_custom_target(i18n DEPENDS ${I18N_MO_FILES})

add_custom_target(i18n-check
    COMMAND ${Python3_EXECUTABLE} ${I18N_SCRIPT}
            --project-dir ${PROJECT_SOURCE_DIR} ${I18N_MSGFMT_ARG} check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Checking gettext catalogs"
    VERBATIM
)

add_custom_target(i18n-sync
    COMMAND ${Python3_EXECUTABLE} ${I18N_SCRIPT}
            --project-dir ${PROJECT_SOURCE_DIR} sync
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Syncing gettext catalogs"
    VERBATIM
)
