#include <iostream>

#include "vers.h"
#include "bulk.h"

using namespace std::literals::string_literals;


int main(int argc, char const* argv[]) 
{
	using namespace otus_hw7;
	try
	{
		Options options;
		if (!parse_command_line(argc, argv, options))
			return 1;
		
		IProcessorPtr_t processor = create_processor(options);
		processor->process();	
	}	
	catch(const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
