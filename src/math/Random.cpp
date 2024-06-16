/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "math/Random.h"

#include <ctime>

thread_local Random::Generator * Random::rng = nullptr;

void Random::seed() {
	seed(u64(std::time(nullptr)));
}

void Random::seed(u64 seedVal) {
	if(rng) {
		rng->seed(seedVal);
	} else {
		rng = new Generator(seedVal);
	}
}

void Random::shutdown() {
	delete rng;
	rng = nullptr;
}

float Random::Mt19937() {
	static std::random_device rndDev;
	static std::mt19937 rngMt19937{rndDev()}; 
	static std::uniform_real_distribution<float> urd(0.0, 1.0);
	return urd(rngMt19937);
}
float Random::Mt19937plus(int max) {
	float rnf = 0.f;
	int iTotRnd = Random::get(1, max);
	for(int iR2 = 0; iR2 < iTotRnd; iR2++) rnf += Random::Mt19937(); // trying to increase unpredictability
	rnf = static_cast<float>(std::fmod(rnf, 1.0));
	return rnf;
}
