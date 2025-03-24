#pragma once

#include "complex_class.hpp"
#include "parameter.hpp"

extern cgx::unique_parameter_list<0, 10> params;

extern cgx::unique_parameter<int, 0>&             integer;
extern cgx::unique_parameter<complex_class, 100>& complex;
