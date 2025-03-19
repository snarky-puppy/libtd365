/*
 * Copyright (c) 2025, Matt Wlazlo
 *
 * This file is part of the td365 project.
 * Use in compliance with the Prosperity Public License 3.0.0.
 */

#ifndef JSON_H
#define JSON_H

#include "td365.h"
#include <nlohmann/json_fwd.hpp>

void to_json(nlohmann::json &j, const market_group &mg);
void from_json(const nlohmann::json &j, market_group &mg);
void to_json(nlohmann::json &j, const market &m);
void from_json(const nlohmann::json &j, market &m);
void to_json(nlohmann::json &j, const tick &m);
void from_json(const nlohmann::json &j, tick &m);

#endif // JSON_H
