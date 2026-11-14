#pragma once
#include <stdint.h>
struct Config {
	const char *cursor_theme;
	int cursor_size;
	uint32_t shake_to_locate;
	float shake_to_locate_sensitivity;
	uint32_t shake_to_locate_min_distance;
	float shake_to_locate_intensity;
	uint32_t shake_to_locate_cooldown;
	uint32_t shake_to_locate_max_size;
};
extern struct Config config;
