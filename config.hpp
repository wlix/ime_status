#pragma once

#include <stdexcept>
#include "ime_status.hpp"

class config {
public:
	config();
	~config();
	static config& get_instance();
	void load_config();
	void save_config();
};
