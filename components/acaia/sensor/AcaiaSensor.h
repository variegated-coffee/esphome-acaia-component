//
// Created by Magnus Nordlander on 2023-10-26.
//

#ifndef SMART_LCC_ACAIASENSOR_H
#define SMART_LCC_ACAIASENSOR_H

#include <cmath>
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "../Acaia.h"
#include "../AcaiaMessage.h"

#define FLOW_MOVING_AVERAGE_SIZE 20

namespace esphome {
    namespace acaia {
        class AcaiaMeasurement {
        public:
            explicit AcaiaMeasurement(int64_t timestamp, float weight) : timestamp(timestamp), weight(weight) {}

            int64_t timestamp;
            float weight;
        };

        class AcaiaSensor
                : public esphome::Component, public esphome::Parented<Acaia> {
        public:
            void setup() {
                flows.reserve(FLOW_MOVING_AVERAGE_SIZE - 1);
                get_parent()->add_on_message_callback([this](AcaiaMessage msg) { this->handleMessage(msg); });
            }

            void handleMessage(AcaiaMessage message) {
                //ESP_LOGD("ACAIA", "Handling message");
                if (message.has_weight) {
                    weightHistory.emplace_back(message.timestamp, message.weight);

                    while (weightHistory.size() > FLOW_MOVING_AVERAGE_SIZE) {
                        weightHistory.pop_front();
                    }

                    // Kg is the SI unit, which is also the unit represented in esphome.const, so
                    // we'll use that.
                    float weightInKg = message.weight / 1000.f;
                    if (should_set(weight_, weightInKg, 0.00001)) {
                        weight_->publish_state(weightInKg);
                    }
                }

                float avgFlow = calculateAvgFlow();

                if (!std::isnan(avgFlow) && should_set(flow_, avgFlow, 0.1)) {
                    flow_->publish_state(avgFlow);
                }
            }

            float calculateAvgFlow()
            {
                //ESP_LOGD(TAG, "Clearing flows");
                flows.clear();
                int64_t now = esp_timer_get_time();

                for (uint16_t i = 3; i < weightHistory.size(); i++) {
                    auto prev = weightHistory.at(i - 3);
                    auto curr = weightHistory.at(i);

                    if ((now - prev.timestamp) > 5000000) {
                        continue;
                    }

                    float timeDiffS = ((float) (curr.timestamp - prev.timestamp)) / 1000000;
                    float weightDiff = curr.weight - prev.weight;

                    if (fabs(weightDiff) < 20 && timeDiffS >= 0.25 && timeDiffS < 0.35) {
                        //ESP_LOGD(TAG, "Pushing flow: %.1f, WD: %.2f, TD: %.3f", weightDiff / timeDiffS, weightDiff, timeDiffS);
                        flows.push_back(weightDiff / timeDiffS);
                    }
                }

                if (flows.empty()) {
                    return NAN;
                }

                float sumFlows = 0;
                for (auto f : flows) {
                    sumFlows += f;
                }

                return sumFlows / (float)flows.size();
            }

            bool should_set(esphome::sensor::Sensor *sensor, float value, float sigma = 0.1)
            {
                return sensor != nullptr && (!sensor->has_state() || fabs(sensor->get_raw_state() - value) >= sigma);
            }

            void set_weight(esphome::sensor::Sensor *sens) { weight_ = sens; }
            void set_flow(esphome::sensor::Sensor *sens) { flow_ = sens; }
        protected:
            esphome::sensor::Sensor *weight_{nullptr};
            esphome::sensor::Sensor *flow_{nullptr};

            std::deque<AcaiaMeasurement> weightHistory;
            std::vector<float> flows;
        };
    }
}

#endif //SMART_LCC_ACAIASENSOR_H
