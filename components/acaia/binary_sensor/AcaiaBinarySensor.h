//
// Created by Magnus Nordlander on 2023-10-31.
//

#ifndef ESPHOME_BIANCA_ACAIABINARYSENSOR_H
#define ESPHOME_BIANCA_ACAIABINARYSENSOR_H

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"
#include "../Acaia.h"
#include "../AcaiaMessage.h"

namespace esphome {
    namespace acaia {
        class AcaiaBinarySensor
                : public esphome::Component, public esphome::Parented<Acaia> {
        public:
            void setup() {
                get_parent()->add_on_message_callback([this](AcaiaMessage msg) { this->handleMessage(msg); });
            }

            void handleMessage(AcaiaMessage message) {
                if (connected_ != nullptr) {
                    connected_->publish_state(message.connected);
                }
            }

            void set_connected(esphome::binary_sensor::BinarySensor *sens) { connected_ = sens; }
        protected:
            esphome::binary_sensor::BinarySensor *connected_{nullptr};
        };
    }
}

#endif //ESPHOME_BIANCA_ACAIABINARYSENSOR_H
