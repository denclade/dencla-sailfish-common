# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.19)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
string(RANDOM LENGTH 12 ALPHABET abcdef0123456789 token)
set(work "${root}/build/about-test-${token}")
set(source "${work}/source with spaces")
set(binary "${work}/build with spaces")
set(prefix "${work}/install with spaces")
file(MAKE_DIRECTORY "${source}/legal")
file(WRITE "${source}/LICENSE" "Test application license <literal markup>.\n")
file(WRITE "${source}/legal/datenschutz.html.in" "@LEGAL_PREVIEW_NOTICE@<h2>Privacy</h2>@LEGAL_CONTACT_HTML@")
file(WRITE "${source}/legal/provider.local.json" [=[{
  "name":"Test Publisher", "street":"Test street 1", "postal_code":"12345",
  "city":"Test City", "country":"Test Country", "email":"legal@project.test", "phone":""
}]=])
file(WRITE "${source}/CMakeLists.txt.in" [=[
cmake_minimum_required(VERSION 3.19)
project(about_fixture NONE)
if(DENCLA_BUILD_QML_TEST_RUNNER)
    enable_language(CXX)
    find_package(Qt5 COMPONENTS Core Gui Quick REQUIRED)
    find_library(quicktest NAMES Qt5QuickTest libQt5QuickTest.so.5 REQUIRED)
    add_executable(dencla_about_qml_runner "@root@/tests/about_qml_runner.cpp")
    target_link_libraries(dencla_about_qml_runner Qt5::Core Qt5::Gui Qt5::Quick "${quicktest}")
endif()
include("@root@/cmake/DenclaAbout.cmake")
add_custom_target(first_app)
add_custom_target(second_app)
dencla_install_about(TARGET first_app APP_ID first-app APP_NAME "First App"
    LICENSE_ID GPL-3.0-or-later COPYRIGHT "Copyright © First Publisher"
    COMPONENTS BOUNCE_LIST_VIEW PRIVACY_TEMPLATE legal/datenschutz.html.in)
dencla_install_about(TARGET second_app APP_ID second-app APP_NAME "Second App"
    LICENSE_ID MIT COPYRIGHT "Copyright © Second Publisher"
    PRIVACY_TEMPLATE legal/datenschutz.html.in)
]=])
configure_file("${source}/CMakeLists.txt.in" "${source}/CMakeLists.txt" @ONLY)
function(run expected)
    execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN}
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(expected STREQUAL "pass" AND NOT result EQUAL 0)
        message(FATAL_ERROR "About fixture failed: ${stdout}${stderr}")
    elseif(expected STREQUAL "fail" AND result EQUAL 0)
        message(FATAL_ERROR "About fixture unexpectedly succeeded: ${ARGN}")
    endif()
endfunction()
run(pass -S "${source}" -B "${binary}" -DDENCLA_REQUIRE_LEGAL_DETAILS=ON
    "-DDENCLA_BUILD_QML_TEST_RUNNER=${DENCLA_BUILD_QML_TEST_RUNNER}")
run(pass --build "${binary}")
run(pass --install "${binary}" --prefix "${prefix}")
foreach(app first-app second-app)
    set(package "${prefix}/share/${app}")
    foreach(file licenses/index.json licenses/APP-LICENSE.txt legal/impressum.html legal/datenschutz.html
            qml/pages/LegalDocumentPage.qml qml/pages/ThirdPartyLicensesPage.qml qml/pages/LicenseCatalog.qml
            qml/pages/LegalFiles.js qml/pages/ImprintPage.qml qml/components/AboutLinkItem.qml
            qml/components/AboutLicenseNotice.qml qml/components/AboutLegalSection.qml qml/components/LicenseText.qml)
        if(NOT EXISTS "${package}/${file}")
            message(FATAL_ERROR "About UI is missing ${app}/${file}.")
        endif()
    endforeach()
    if(NOT EXISTS "${package}/qml/components/AboutAcknowledgements.qml")
        message(FATAL_ERROR "About acknowledgements component was not installed.")
    endif()
    file(READ "${package}/licenses/index.json" index)
    string(JSON name GET "${index}" application name)
    if((app STREQUAL "first-app" AND NOT name STREQUAL "First App")
            OR (app STREQUAL "second-app" AND NOT name STREQUAL "Second App"))
        message(FATAL_ERROR "Application metadata leaked between packages.")
    endif()
endforeach()

if(DENCLA_BUILD_QML_TEST_RUNNER)
    set(DENCLA_QML_TEST_RUNNER "${binary}/dencla_about_qml_runner")
endif()
if(DENCLA_QML_TEST_RUNNER)
    set(package "${prefix}/share/first-app")
    file(MAKE_DIRECTORY "${package}/tests")
    configure_file("${root}/tests/qml/tst_about.qml" "${package}/tests/tst_about.qml" COPYONLY)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E env QT_LOGGING_TO_CONSOLE=1 QT_QPA_PLATFORM=minimal QT_QUICK_BACKEND=software
        "${DENCLA_QML_TEST_RUNNER}" -input "${package}/tests"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr TIMEOUT 60)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "About QML tests failed: ${stdout}${stderr}")
    endif()
    message(STATUS "${stdout}")
endif()

# Strict checking still runs during build/install, including profile removal.
file(REMOVE "${source}/legal/provider.local.json")
run(fail --build "${binary}" --target first_app)
run(fail --install "${binary}" --prefix "${prefix}")
run(fail -S "${source}" -B "${work}/missing-profile" -DDENCLA_REQUIRE_LEGAL_DETAILS=ON)
file(REMOVE_RECURSE "${work}")
message(STATUS "Shared About installation, two-app isolation and strict checks passed")
