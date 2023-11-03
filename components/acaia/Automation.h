//
// Created by Magnus Nordlander on 2023-10-31.
//

#ifndef ESPHOME_BIANCA_AUTOMATION_H
#define ESPHOME_BIANCA_AUTOMATION_H

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "Acaia.h"

namespace esphome {
    namespace acaia {
        template<typename... Ts>
        class AcaiaTareAction : public Action<Ts...> {
        public:
            explicit AcaiaTareAction(Acaia *acaia) : acaia_(acaia) {}

            void play(Ts... x) override { acaia_->sendTare(); }

        protected:
            Acaia *acaia_;
        };

        template<typename... Ts>
        class AcaiaResetTimerAction : public Action<Ts...> {
        public:
            explicit AcaiaResetTimerAction(Acaia *acaia) : acaia_(acaia) {}

            void play(Ts... x) override { acaia_->resetTimer(); }

        protected:
            Acaia *acaia_;
        };

        template<typename... Ts>
        class AcaiaStartTimerAction : public Action<Ts...> {
        public:
            explicit AcaiaStartTimerAction(Acaia *acaia) : acaia_(acaia) {}

            void play(Ts... x) override { acaia_->startTimer(); }

        protected:
            Acaia *acaia_;
        };

        template<typename... Ts>
        class AcaiaStopTimerAction : public Action<Ts...> {
        public:
            explicit AcaiaStopTimerAction(Acaia *acaia) : acaia_(acaia) {}

            void play(Ts... x) override { acaia_->stopTimer(); }

        protected:
            Acaia *acaia_;
        };
    }
}

#endif //ESPHOME_BIANCA_AUTOMATION_H
