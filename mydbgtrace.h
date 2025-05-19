#pragma once

#ifndef __PRETTY_FUNCTION__
#include "pretty.h"
#endif

#include <iostream>

#ifdef USE_DBG_TRACE
#ifndef USE_PRETTY
#define DBG_TRACE( func, trace_statements )\
		std::cout << func << trace_statements << std::endl;
#else
#define DBG_TRACE( func, trace_statements )\
		std::cout << __PRETTY_FUNCTION__ << trace_statements << std::endl;
#endif
#else
#define DBG_TRACE( func, trace_statements )   
#endif // USE_DBG_TRACE

