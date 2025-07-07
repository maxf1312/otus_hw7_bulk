#include <iostream>
#include <boost/program_options.hpp>
#include "bulk.h"

namespace otus_hw7{
    namespace po = boost::program_options;
        
    void show_help(po::options_description const& desc)
    {
        std::cout << desc << std::endl;
    }

    void parse_command_line(int argc, const char* argv[], Options& parsed_options)
    {
        constexpr const char* const OPTION_NAME_HELP = "help"; 
        constexpr const char* const OPTION_NAME_CHUNK_SIZE = "chunk_size"; 
        parsed_options = {false, 0};
        
        po::options_description desc("Аргументы командной строки");
        desc.add_options()
            (OPTION_NAME_HELP, po::bool_switch(&parsed_options.show_help), "Отображение справки")
            (OPTION_NAME_CHUNK_SIZE, po::value<size_t>(&parsed_options.cmd_chunk_sz), "Размер блока команд");

        po::positional_options_description pos_desc;
        pos_desc.add(OPTION_NAME_CHUNK_SIZE, -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
        vm.notify();

        size_t sz = vm.size();
        if( sz < 2 || !vm.count(OPTION_NAME_CHUNK_SIZE) )
            parsed_options.show_help = true;
        
        if( parsed_options.show_help )
            show_help(desc);
    }
};