//
// Created by Magnus Nordlander on 2023-10-26.
//

#ifndef SMART_LCC_ACAIASENSOR_H
#define SMART_LCC_ACAIASENSOR_H

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/helpers.h"
#include "../Acaia.h"
#include "../AcaiaMessage.h"

namespace esphome {
    namespace acaia {
        class AcaiaSensor
                : public esphome::Component, public esphome::Parented<Acaia> {
        public:
            void setup() {
                get_parent()->add_on_message_callback([this](AcaiaMessage msg) { this->handleMessage(msg); });
            }

            void handleMessage(AcaiaMessage message) {
                //ESP_LOGD("ACAIA", "Handling message");
                if (message.has_weight) {
                    // Kg is the SI unit, which is also the unit represented in esphome.const, so
                    // we'll use that.
                    float weightInKg = message.weight / 1000.f;
                    if (should_set(weight_, weightInKg, 0.0001)) {
                        weight_->publish_state(weightInKg);
                    }
                }
            }

            bool should_set(esphome::sensor::Sensor *sensor, float value, float sigma = 0.1)
            {
                return sensor != nullptr && (!sensor->has_state() || fabs(sensor->get_state() - value) >= sigma);
            }

            void set_weight(esphome::sensor::Sensor *sens) { weight_ = sens; }
        protected:
            esphome::sensor::Sensor *weight_{nullptr};

        };
    }
}

#endif //SMART_LCC_ACAIASENSOR_H
