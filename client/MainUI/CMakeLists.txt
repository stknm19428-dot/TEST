cmake_minimum_required(VERSION 3.16)

project(MainUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Charts Sql)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(MainUI
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}

        main.cpp

        # Core
        src/core/database_manager.h
        src/core/database_manager.cpp

        # Services
        src/services/auth_service.h
        src/services/auth_service.cpp

        # Views
        src/views/mainwindow.h
        src/views/mainwindow.cpp
        src/views/mainwindow.ui

        src/views/login_widget.h
        src/views/login_widget.cpp
        src/views/login_widget.ui

        src/views/dashboard_widget.h
        src/views/dashboard_widget.cpp
        src/views/dashboard_widget.ui


        src/views/partner_manage_widget.cpp
        src/views/partner_manage_widget.h
        src/views/partner_manage_widget.ui
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET MainUI APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(MainUI SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(MainUI
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(MainUI PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/views
)

target_link_libraries(MainUI PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Charts Qt${QT_VERSION_MAJOR}::Sql)
# target_link_libraries(MainUI PRIVATE Qt${QT_VERSION_MAJOR}::Charts Qt6::Sql)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.MainUI)
endif()
set_target_properties(MainUI PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS MainUI
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(MainUI)
endif()
