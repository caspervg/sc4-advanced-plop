# EmbedFile.cmake - Embed a file as a C++ string constant

function(embed_file INPUT_FILE OUTPUT_FILE VARIABLE_NAME)
    file(READ "${INPUT_FILE}" FILE_CONTENTS)

    # Escape special characters for C++ string
    string(REPLACE "\\" "\\\\" FILE_CONTENTS "${FILE_CONTENTS}")
    string(REPLACE "\"" "\\\"" FILE_CONTENTS "${FILE_CONTENTS}")
    string(REPLACE "\n" "\\n\"\n\"" FILE_CONTENTS "${FILE_CONTENTS}")

    # Generate header file
    file(WRITE "${OUTPUT_FILE}"
"// Auto-generated file - do not edit
// Generated from: ${INPUT_FILE}

#pragma once

#include <string_view>

namespace embedded {

constexpr std::string_view ${VARIABLE_NAME} =
\"${FILE_CONTENTS}\";

} // namespace embedded
")

    message(STATUS "Embedded ${INPUT_FILE} as ${VARIABLE_NAME} in ${OUTPUT_FILE}")
endfunction()
