cmake_minimum_required(VERSION 3.21)

project(BadUSBSolution LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_QT_UI "Build the Qt Widgets user interface" OFF)

add_library(CapstoneCore STATIC
    Capstone/Core/logger.cpp
    Capstone/Core/policy.cpp
    Capstone/Hook/keyboard_hook.cpp
    Capstone/Detect/hid_scan.cpp
    Capstone/Detect/hid_fingerprint.cpp
    Capstone/Algorithm/PreAnalysis/key_decoder.cpp
    Capstone/Algorithm/PreAnalysis/key_normalizer.cpp
    Capstone/Algorithm/PreAnalysis/url_extractor.cpp
    Capstone/Algorithm/PreAnalysis/typing_analyzer.cpp
    Capstone/Algorithm/Analysis/ShellDetector/shell_detector.cpp
    Capstone/Algorithm/Analysis/ThreatMatcher/threat_matcher.cpp
    Capstone/Algorithm/Analysis/UrlClassifier/url_classifier.cpp
    Capstone/Algorithm/Analysis/CompoundScorer/compound_scorer.cpp
)

target_include_directories(CapstoneCore PUBLIC
    Capstone/Core
    Capstone/Hook
    Capstone/Detect
    Capstone/Algorithm/PreAnalysis
    Capstone/Algorithm/Analysis/ShellDetector
    Capstone/Algorithm/Analysis/ThreatMatcher
    Capstone/Algorithm/Analysis/UrlClassifier
    Capstone/Algorithm/Analysis/CompoundScorer
)

target_compile_definitions(CapstoneCore PUBLIC
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(CapstoneCore PRIVATE /utf-8 /W3)
endif()

target_link_libraries(CapstoneCore PUBLIC
    Cfgmgr32
    Hid
    Shell32
    Setupapi
)

enable_testing()

if(BUILD_QT_UI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

    add_executable(CapstoneQtApp WIN32
        Capstone/UIQt/main_qt.cpp
        Capstone/UIQt/MainWindow.h
        Capstone/UIQt/MainWindow.cpp
        Capstone/UIQt/TrayWidget.h
        Capstone/UIQt/TrayWidget.cpp
        Capstone/UIQt/Theme.h
        Capstone/UIQt/Assets/ui.qrc
    )

    target_link_libraries(CapstoneQtApp PRIVATE
        CapstoneCore
        Qt6::Svg
        Qt6::Widgets
    )

    target_include_directories(CapstoneQtApp PRIVATE
        Capstone/UIQt
    )
endif()
