/*
 * Copyright 2020-2021 AVSystem <avsystem@avsystem.com>
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

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

extern const size_t ML_SAMPLE_COUNT;
extern const size_t ML_SAMPLES_PER_FRAME;
extern const size_t ML_SAMPLING_INTERVAL_MS;
extern const size_t ML_LABEL_COUNT;
extern const char **ML_LABELS;

/**
 * Run inference of \c data.
 * @param data array of data to run inference on, of size \c ML_SAMPLE_COUNT *
 * \c ML_SAMPLES_PER_FRAME
 * @return \c -1 on no dominant label or error, label ID otherwise
 */
int run_ml_inference(float *inference_data);

#ifdef __cplusplus
}
#endif // __cplusplus
