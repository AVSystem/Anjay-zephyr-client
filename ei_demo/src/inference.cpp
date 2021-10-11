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

#include <algorithm>
#include <array>

#include <logging/log.h>

#include "inference.h"
#include <edge-impulse-sdk/classifier/ei_run_classifier.h>

LOG_MODULE_REGISTER(inference);

#define ML_DELTA_TRIGGER 0.2f

const size_t ML_SAMPLE_COUNT = EI_CLASSIFIER_RAW_SAMPLE_COUNT;
const size_t ML_SAMPLES_PER_FRAME = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
const size_t ML_SAMPLING_INTERVAL_MS = EI_CLASSIFIER_INTERVAL_MS;
const size_t ML_LABEL_COUNT = EI_CLASSIFIER_LABEL_COUNT;
const char **ML_LABELS = ei_classifier_inferencing_categories;

/**
 * Returns index of the largest element in \c arr if it's
 * at least \c threshold larger than all other elements.
 *
 * @param arr array to search for the dominant index
 * @param n size of the array
 * @param threshold minimum delta of largest and second largest element
 * @returns index of the element matching the requirements, \c -1 otherwise
 */
template <typename T>
static int dominant_index(const T *arr, size_t n, T threshold) {
    if (n == 0) {
        return -1;
    }
    if (n == 1) {
        return 0;
    }

    size_t largest;
    size_t second_largest;
    if (arr[0] >= arr[1]) {
        largest = 0;
        second_largest = 1;
    } else {
        largest = 1;
        second_largest = 0;
    }

    for (size_t i = 2; i < n; i++) {
        if (arr[i] > arr[largest]) {
            second_largest = largest;
            largest = i;
        } else if (arr[i] > arr[second_largest]) {
            second_largest = i;
        }
    }

    return arr[largest] - arr[second_largest] >= threshold ? largest : -1;
}

int run_ml_inference(float *inference_data) {
    signal_t signal;
    numpy::signal_from_buffer(inference_data,
                              EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);

    ei_impulse_result_t res;
    EI_IMPULSE_ERROR err = run_classifier(&signal, &res);
    if (err < 0) {
        LOG_ERR("run_classifier returned %d", err);
        return -1;
    }

    float class_values[EI_CLASSIFIER_LABEL_COUNT];
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        class_values[i] = res.classification[i].value;
    }

    return dominant_index(class_values, EI_CLASSIFIER_LABEL_COUNT,
                          ML_DELTA_TRIGGER);
}
