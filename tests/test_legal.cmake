# SPDX-FileCopyrightText: 2026 Dennis Burgermeister
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.19)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
string(RANDOM LENGTH 12 ALPHABET abcdef0123456789 token)
set(work "${root}/build/legal-test-${token}")
set(source "${work}/source")
set(output "${work}/output with spaces")
file(MAKE_DIRECTORY "${source}/legal")
set(privacy_text "@LEGAL_PREVIEW_NOTICE@<h2>Privacy for @LEGAL_APPLICATION_NAME@</h2>@LEGAL_CONTACT_HTML@<p>Fixture policy.</p>\n")
file(WRITE "${source}/legal/datenschutz.html.in" "${privacy_text}")

function(generate expected profile strict)
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DDENCLA_LEGAL_DIRECTORY=${source}/legal"
        "-DDENCLA_LEGAL_PRIVACY_TEMPLATE=${source}/legal/datenschutz.html.in"
        "-DDENCLA_LEGAL_APP_NAME=Test & App"
        "-DDENCLA_LEGAL_OUTPUT_DIR=${output}"
        "-DDENCLA_LEGAL_CONFIG=${profile}"
        "-DDENCLA_REQUIRE_LEGAL_DETAILS=${strict}"
        -P "${root}/cmake/GenerateLegal.cmake"
        RESULT_VARIABLE result OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
    if(expected STREQUAL "pass" AND NOT result EQUAL 0)
        message(FATAL_ERROR "Legal generation unexpectedly failed: ${stderr}")
    elseif(expected STREQUAL "fail" AND result EQUAL 0)
        message(FATAL_ERROR "Incomplete/invalid legal configuration was accepted.")
    endif()
    set(last_error "${stdout}${stderr}" PARENT_SCOPE)
endfunction()

# A fresh clone needs no private configuration and produces an explicit preview.
generate(pass "" OFF)
file(READ "${output}/impressum.html" imprint)
if(NOT imprint MATCHES "Entwicklungsfassung" OR imprint MATCHES "@LEGAL_")
    message(FATAL_ERROR "The anonymous fallback was not rendered correctly.")
endif()
generate(fail "" ON)

# A complete publisher profile renders consistently into both documents.
# Markup and quotes in a personal field must remain text, not HTML.
set(profile "${source}/legal/provider.local.json")
file(WRITE "${profile}" [=[{
  "name": "Test <img src=\"https://example.invalid/tracker\"> & Publisher",
  "street": "Test street 1", "postal_code": "12345", "city": "Test city",
  "country": "Test country", "email": "legal@project.test", "phone": "+49 (0)000 123"
}]=])
file(READ "${profile}" complete_profile)
generate(pass "" ON)
foreach(document impressum datenschutz)
    file(READ "${output}/${document}.html" rendered)
    if(rendered MATCHES "Entwicklungsfassung" OR rendered MATCHES "<img"
            OR NOT rendered MATCHES "&lt;img" OR NOT rendered MATCHES "&amp; Publisher"
            OR NOT rendered MATCHES "tel:\\+49000123")
        message(FATAL_ERROR "Publisher text was not safely rendered into ${document}.")
    endif()
endforeach()

# Whitespace-only address values fail release validation without echoing data.
file(READ "${profile}" data)
string(JSON data SET "${data}" street "\"   \"")
file(WRITE "${profile}" "${data}")
generate(fail "" ON)
if(NOT last_error MATCHES "street" OR last_error MATCHES "legal@project.test|Test street|tracker")
    message(FATAL_ERROR "Incomplete-profile errors must list field names only.")
endif()

# Removing a private file must replace stale documents with anonymous output.
file(REMOVE "${profile}")
generate(pass "" OFF)
file(READ "${output}/datenschutz.html" rendered)
if(rendered MATCHES "legal@project.test|Test city|tracker" OR NOT rendered MATCHES "Entwicklungsfassung")
    message(FATAL_ERROR "Previous publisher data survived fallback regeneration.")
endif()

# Explicit but missing configurations must not silently use the example.
generate(fail "${work}/missing.json" OFF)
file(WRITE "${profile}" "{invalid JSON}")
generate(fail "" OFF)
file(WRITE "${profile}" "{\"name\":123}")
generate(fail "" OFF)

# A privacy draft is never silently accepted as a finished policy.
file(WRITE "${profile}" "${complete_profile}")
configure_file("${root}/legal/datenschutz.example.html.in" "${source}/legal/datenschutz.html.in" COPYONLY)
generate(fail "" ON)
if(NOT last_error MATCHES "privacy draft")
    message(FATAL_ERROR "Release did not reject the common privacy draft.")
endif()
generate(pass "" OFF)
file(READ "${output}/datenschutz.html" rendered)
if(NOT rendered MATCHES "Entwicklungsfassung")
    message(FATAL_ERROR "Draft policy was not marked as a development preview.")
endif()
file(WRITE "${source}/legal/datenschutz.html.in" "${privacy_text}")
execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DDENCLA_LEGAL_DIRECTORY=${source}/legal"
    "-DDENCLA_LEGAL_PRIVACY_TEMPLATE=${source}/legal/datenschutz.html.in"
    "-DDENCLA_LEGAL_OUTPUT_DIR=${work}/must-not-be-created"
    -DDENCLA_REQUIRE_LEGAL_DETAILS=ON -DDENCLA_LEGAL_VALIDATE_ONLY=ON
    -P "${root}/cmake/GenerateLegal.cmake"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
if(NOT result EQUAL 0 OR EXISTS "${work}/must-not-be-created")
    message(FATAL_ERROR "Validation-only mode wrote output or rejected a valid profile: ${stderr}")
endif()

# Use the common module from an unrelated app with a different package ID.
file(WRITE "${source}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.19)\nproject(legal_test NONE)\ninclude(\"${root}/cmake/DenclaLegal.cmake\")\nadd_custom_target(harbour-legal-test)\ndencla_install_legal(TARGET harbour-legal-test APP_NAME [=[Other & \"Reader\"]=] PRIVACY_TEMPLATE legal/datenschutz.html.in)\nadd_custom_target(second-target)\ndencla_install_legal(TARGET second-target APP_ID harbour-another-app PRIVACY_TEMPLATE legal/datenschutz.html.in)\n")
file(REMOVE "${profile}")
set(binary "${work}/build with spaces")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${binary}"
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "The legal CMake module failed to configure: ${stderr}")
endif()
file(WRITE "${profile}" "${complete_profile}")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary}" --target harbour-legal-test-legal
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
file(READ "${binary}/CMakeFiles/dencla-legal-harbour-legal-test/impressum.html" rendered)
if(NOT result EQUAL 0 OR NOT rendered MATCHES "legal@project.test")
    message(FATAL_ERROR "Build did not pick up a profile added after configuration: ${stderr}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary}" --target harbour-legal-test-legal-check
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Configured release check rejected the complete profile: ${stderr}")
endif()
file(REMOVE "${profile}")
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${binary}" --target harbour-legal-test-legal-check
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result EQUAL 0)
    message(FATAL_ERROR "Configured release check accepted a removed profile.")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${binary}" --prefix "${work}/stage" --component Legal
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
file(READ "${work}/stage/share/harbour-legal-test/legal/impressum.html" rendered)
if(NOT result EQUAL 0 OR rendered MATCHES "legal@project.test" OR NOT rendered MATCHES "Entwicklungsfassung")
    message(FATAL_ERROR "Install reused stale publisher data instead of regenerating: ${stderr}")
endif()

file(READ "${work}/stage/share/harbour-another-app/legal/impressum.html" second_imprint)
if(NOT second_imprint MATCHES "harbour-another-app")
    message(FATAL_ERROR "Common module did not use the second application's package ID.")
endif()
file(READ "${work}/stage/share/harbour-legal-test/legal/impressum.html" first_imprint)
if(NOT first_imprint MATCHES "Other &amp; &quot;Reader&quot;")
    message(FATAL_ERROR "Application display name was not escaped/preserved at installation.")
endif()
file(READ "${root}/qml/ImprintPage.qml" qml_source)
file(READ "${work}/stage/share/harbour-another-app/qml/pages/ImprintPage.qml" qml_installed)
if(NOT qml_source STREQUAL qml_installed)
    message(FATAL_ERROR "Common QML legal page was not installed with the app.")
endif()

file(WRITE "${profile}" "${complete_profile}")
set(strict_binary "${work}/strict build")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${source}" -B "${strict_binary}" -DDENCLA_REQUIRE_LEGAL_DETAILS=ON
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE stderr)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Strict configuration rejected a complete profile: ${stderr}")
endif()
file(REMOVE "${profile}")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${strict_binary}" --prefix "${work}/strict stage" --component Legal
    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
if(result EQUAL 0 OR EXISTS "${work}/strict stage/share/harbour-legal-test/legal/impressum.html")
    message(FATAL_ERROR "Strict install packaged outdated data after its profile was removed.")
endif()

file(REMOVE_RECURSE "${work}")
message(STATUS "Legal generation: fallback, validation, escaping, invalid profiles, draft rejection, multiple apps, build and install regeneration passed")
