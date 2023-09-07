/*
 * Copyright 2020-2023 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "water_meter.h"

enum bubblemaker_state {
	BUBBLEMAKER_IDLE,
	BUBBLEMAKER_START_RED_LIGHT,
	BUBBLEMAKER_START_YELLOW_LIGHT,
	BUBBLEMAKER_MEASURE,
#if WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	BUBBLEMAKER_END_P1_WON,
	BUBBLEMAKER_END_P2_WON
#else // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
	BUBBLEMAKER_END
#endif // WATER_METER_0_AVAILABLE && WATER_METER_1_AVAILABLE
};

extern enum bubblemaker_state bm_state;

int bubblemaker_init(void);
