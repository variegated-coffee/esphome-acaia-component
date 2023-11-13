//
// Created by Magnus Nordlander on 2023-10-25.
//

#ifndef SMART_LCC_ACAIAMESSAGE_H
#define SMART_LCC_ACAIAMESSAGE_H

#include <cstdint>

namespace esphome {
    namespace acaia {
        enum AcaiaMessageType { none, weight, heartbeat, timer, tare_start_stop_reset };
        enum AcaiaButton { tare, start, stop, reset };

        class AcaiaMessage {
        public:
            AcaiaMessageType type = none;

            bool has_weight = false;
            float weight;

            bool has_time = false;
            float time;

            bool has_button = false;
            AcaiaButton button = tare;

            bool connected = false;

            int64_t timestamp;
        };
    }
}

#endif //SMART_LCC_ACAIAMESSAGE_H
