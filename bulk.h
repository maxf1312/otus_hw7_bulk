#pragma once

#include <memory>

namespace otus_hw7{
    struct Options
    {
        bool   show_help;
        size_t cmd_chunk_sz;
    };
    void parse_command_line(int argc, const char* argv[], Options& parsed_options);

} // otus_hw7

