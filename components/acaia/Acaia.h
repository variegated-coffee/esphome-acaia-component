//
// Created by Magnus Nordlander on 2023-10-24.
//

#ifndef SMART_LCC_ACAIA_H
#define SMART_LCC_ACAIA_H

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ble_client/ble_client.h"
#include <esp_gattc_api.h>
#include <esp_timer.h>
#include <cstdint>
#include <cmath>
#include "AcaiaMessage.h"

#define SCALE_SERVICE_UUID "00001820-0000-1000-8000-00805f9b34fb"
#define SCALE_CHARACTERISTIC_UUID "00002a80-0000-1000-8000-00805f9b34fb"

#define MAGIC1 0xef
#define MAGIC2 0xdd

#define PACKET_TYPE_MESSAGE 0x0c
#define PACKET_TYPE_SETTINGS 0x08

#define PACKET_BUFFER_SIZE 1024

#define ACAIA_MESSAGE_TYPE_IDENT 11
#define ACAIA_MESSAGE_TYPE_NOTIFICATION_REQUEST 12
#define ACAIA_MESSAGE_TYPE_HEARTBEAT 0
#define ACAIA_MESSAGE_TYPE_TARE 4
#define ACAIA_MESSAGE_TYPE_TIMER_MANIPULATION 13

namespace esphome {
    namespace acaia {
        static const char* TAG = "ACAIA";

        class Acaia : public esphome::Component, public esphome::ble_client::BLEClientNode {
        public:
            void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
            {
                switch (event) {
                    case ESP_GATTC_CONNECT_EVT:
                        ESP_LOGD(TAG, "Connected");
                    break;
                    case ESP_GATTC_OPEN_EVT: {
                        ESP_LOGD(TAG, "ESP_GATTC_OPEN_EVT");
                        conn_id_ = param->open.conn_id;

                        connected = true;

                        AcaiaMessage message;
                        message.timestamp = esp_timer_get_time();
                        message.connected = connected;
                        sendMessageToHandlers(message);

                        break;
                    }
                    case ESP_GATTC_SEARCH_CMPL_EVT: {
                        ESP_LOGD(TAG, "Registering for notifications");

                        auto *chr = this->parent_->get_characteristic(
                                esphome::esp32_ble::ESPBTUUID::from_raw(SCALE_SERVICE_UUID),
                                esphome::esp32_ble::ESPBTUUID::from_raw(SCALE_CHARACTERISTIC_UUID)
                        );
                        if (chr == nullptr) {
                            ESP_LOGW(TAG, "No control service found at device, not an Anova..?");
                            break;
                        }
                        this->char_handle_ = chr->handle;

                        auto status = esp_ble_gattc_register_for_notify(this->parent_->get_gattc_if(), this->parent_->get_remote_bda(),
                                                                        chr->handle);
                        if (status) {
                            ESP_LOGW(TAG, "esp_ble_gattc_register_for_notify failed, status=%d", status);
                        }
                        break;
                    }
                    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
                        ESP_LOGD(TAG, "Registered for notifications, getting client characteristic");

                        auto *chr = this->parent_->get_config_descriptor(
                                this->char_handle_
                        );
                        if (chr == nullptr) {
                            ESP_LOGW(TAG, "No client config descriptor found");
                            break;
                        }

                        uint16_t notify_en = 1;
                        esp_err_t status =
                                esp_ble_gattc_write_char_descr(this->parent_->get_gattc_if(), this->conn_id_, chr->handle, sizeof(notify_en),
                                                               (uint8_t *) &notify_en, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
                        if (status) {
                            ESP_LOGW(TAG, "esp_ble_gattc_write_char_descr error, status=%d", status);
                        }

//                        this->node_state = espbt::ClientState::ESTABLISHED;
                        ident();
//                        vTaskDelay(pdMS_TO_TICKS(100));
                        notificationRequest();
                        last_heartbeat = esp_timer_get_time();
                        break;
                    }
                    case ESP_GATTC_NOTIFY_EVT: {
                        if (param->notify.handle != this->char_handle_)
                            break;

                        if (packetBufferHead + param->notify.value_len >= PACKET_BUFFER_SIZE) {
                            ESP_LOGE(TAG, "Received packet would overflow the packet buffer. Ignoring.");
                            maybeSendHeartbeat();
                            break;
                        }
                        memcpy(packetBuffer + packetBufferHead, param->notify.value, param->notify.value_len);
                        packetBufferHead += param->notify.value_len;

                        if (packetBuffer[0] != MAGIC1 || packetBuffer[1] != MAGIC2) {
                            ESP_LOGW(TAG, "Packet doesn't start with the magic numbers. Flushing.");
                            packetBufferHead = 0;
                            maybeSendHeartbeat();
                            break;
                        }

                        if (packetBufferHead < 4) {
                            //ESP_LOGD(TAG, "Packet is as of yet incomplete, continuing.");
                            maybeSendHeartbeat();
                            break;
                        }

                        auto packetLen = packetBuffer[3] + 5;

                        if (packetBufferHead < packetLen) {
                           // ESP_LOGD(TAG, "Packet is as of yet incomplete, continuing.");
                            maybeSendHeartbeat();
                            break;
                        }

                        auto packetType = packetBuffer[2];
                       // ESP_LOGD(TAG, "Complete packet received. Type: %x", packetType);

                        if (packetType == PACKET_TYPE_SETTINGS) {
                            // Settings
                            ESP_LOGI(TAG, "Settings packet");
                        } else if (packetType == PACKET_TYPE_MESSAGE) {
                            // Message
                            auto messageType = packetBuffer[4];

                            AcaiaMessage message;

                            message.timestamp = esp_timer_get_time();

                            message.connected = connected;

                            switch (messageType) {
                                case 0x05: {
                                    message.type = AcaiaMessageType::weight;
                                    message.has_weight = true;
                                    message.weight = decodeWeight((packetBuffer[6] << 8) + packetBuffer[5], packetBuffer[9], packetBuffer[10]);

                                    break;
                                }
                                case 0x07: {
                                    message.type = AcaiaMessageType::timer;
                                    message.has_time = true;
                                    message.time = decodeTime(packetBuffer[5], packetBuffer[6], packetBuffer[7]);

                                    break;
                                }
                                case 0x08: {
                                    message.type = AcaiaMessageType::tare_start_stop_reset;

                                    if (packetBuffer[5] == 0x00 && packetBuffer[6] == 0x05) {
                                        message.button = tare;
                                        message.has_weight = true;
                                        message.weight = decodeWeight((packetBuffer[8] << 8) + packetBuffer[7], packetBuffer[11], packetBuffer[12]);
                                    } else if (packetBuffer[5] == 0x08 && packetBuffer[6] == 0x05) {
                                        message.button = start;
                                        message.has_weight = true;
                                        message.weight = decodeWeight((packetBuffer[8] << 8) + packetBuffer[7], packetBuffer[11], packetBuffer[12]);
                                    } else if (packetBuffer[5] == 0x0a && packetBuffer[6] == 0x07) {
                                        message.button = stop;
                                        message.has_weight = true;
                                        message.weight = decodeWeight((packetBuffer[12] << 8) + packetBuffer[11], packetBuffer[15], packetBuffer[16]);
                                        message.has_time = true;
                                        message.time = decodeTime(packetBuffer[7], packetBuffer[8], packetBuffer[9]);
                                    } else if (packetBuffer[5] == 0x09 && packetBuffer[6] == 0x07) {
                                        message.button = reset;
                                        message.has_time = true;
                                        message.time = decodeTime(packetBuffer[7], packetBuffer[8], packetBuffer[9]);
                                        message.has_weight = true;
                                        message.weight = decodeWeight((packetBuffer[12] << 8) + packetBuffer[11], packetBuffer[15], packetBuffer[16]);
                                    }

                                    break;
                                }
                                case 0x11: {
                                    message.type = heartbeat;
                                    if (packetBuffer[7] == 0x05) {
                                        message.has_weight = true;
                                        message.weight = decodeWeight((packetBuffer[9] << 8) + packetBuffer[8], packetBuffer[12], packetBuffer[13]);
                                    } else if (packetBuffer[7] == 0x07) {
                                        message.has_time = true;
                                        message.time = decodeTime(packetBuffer[8], packetBuffer[9], packetBuffer[10]);
                                    }

                                    break;
                                }
                            }

                            sendMessageToHandlers(message);
                        }

                        packetBufferHead = 0;

                        maybeSendHeartbeat();

                        break;
                    }
                    case ESP_GATTC_CLOSE_EVT: {
                        connected = false;

                        AcaiaMessage message;
                        message.timestamp = esp_timer_get_time();
                        message.connected = connected;
                        sendMessageToHandlers(message);

                        break;
                    }
                    default:
                        ESP_LOGD(TAG, "GATTC Event incoming, type %u", event);
                }
            }

            void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
            {
            }

            void loop()
            {

            }

            void add_on_message_callback(std::function<void(AcaiaMessage)> callback) {
                this->on_message_callback_.add(std::move(callback));
            }


            void sendTare() {
                ESP_LOGD(TAG, "Sending Tare");
                uint8_t payload[] = {0x00};

                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_TARE, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
            }

            void resetTimer() {
                ESP_LOGD(TAG, "Resetting timer");
                uint8_t payload[] = {0x00, 0x01};

                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_TIMER_MANIPULATION, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
            }

            void startTimer() {
                ESP_LOGD(TAG, "Starting timer");
                uint8_t payload[] = {0x00, 0x00};

                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_TIMER_MANIPULATION, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
            }

            void stopTimer() {
                ESP_LOGD(TAG, "Stopping timer");
                uint8_t payload[] = {0x00, 0x02};

                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_TIMER_MANIPULATION, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
            }

        protected:
            void sendMessageToHandlers(AcaiaMessage message) {
                this->on_message_callback_.call(message);
/*
                switch(message.type) {
                    case weight:
                        ESP_LOGI(TAG, "Weight: %.2f", message.weight);
                        break;
                    case timer:
                        ESP_LOGI(TAG, "Time: %.2f", message.time);
                        break;
                    case heartbeat:
                        if (message.has_weight) {
                            ESP_LOGI(TAG, "Heartbeat, weight: %.2f", message.weight);
                        } else {
                            ESP_LOGI(TAG, "Heartbeat, time: %.2f", message.time);
                        }
                        break;
                    case tare_start_stop_reset:
                        if (message.has_weight && message.has_time) {
                            ESP_LOGI(TAG, "Button %x, weight: %.2f, time: %.2f", message.button, message.weight, message.time);
                        } else if (message.has_weight) {
                            ESP_LOGI(TAG, "Button %x, weight: %.2f", message.button, message.weight);
                        } else {
                            ESP_LOGI(TAG, "Button %x, time: %.2f", message.button, message.time);
                        }
                }*/
            }

            float decodeWeight(uint16_t raw, uint8_t unit, uint8_t signifier) {
                float divisor = powf(10.f, (float)unit);
                float multiplier = ((signifier & 0x02) == 0x02) ? -1.f : 1.f;
                return ((float)raw / divisor) * multiplier;
            }

            float decodeTime(uint8_t minutes, uint8_t seconds, uint8_t tens) {
                float value = minutes * 60;
                value = value + seconds;
                return value + ((float)tens / 10.f);
            }

            void maybeSendHeartbeat() {
                if (esp_timer_get_time() - last_heartbeat > 1000*1000) {
                    sendHeartbeat();
                }
            }

            void sendHeartbeat() {
                ESP_LOGD(TAG, "Sending Heartbeat");

                uint8_t payload[2] = {0x02, 0x00};
                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_HEARTBEAT, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
                last_heartbeat = esp_timer_get_time();
            }

            void ident() {
                ESP_LOGD(TAG, "Sending Ident");
                uint8_t payload[15] = {0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d, 0x2d,
                                     0x2d, 0x2d, 0x2d};

                uint8_t encodedPayload[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_IDENT, payload, sizeof(payload), encodedPayload, sizeof(encodedPayload));

                write_to_scale_characteristic(encodedPayload, sizeof(encodedPayload));
            }

            void notificationRequest() {
                ESP_LOGD(TAG, "Requesting notifications");

                uint8_t payload[] = {
                        9, // Length of notification request
                        0, // weight
                        1, // weight argument
                        1, // battery
                        2, // battery argument
                        2, // timer
                        5, // timer argument (number heartbeats between timer messages)
                        3, // key
                        4, // setting
                };

                uint8_t encodedBuf[sizeof(payload) + 5];

                encode(ACAIA_MESSAGE_TYPE_NOTIFICATION_REQUEST, payload, sizeof(payload), encodedBuf, sizeof(encodedBuf));

                write_to_scale_characteristic(encodedBuf, sizeof(encodedBuf));
            }

            size_t encode(uint8_t messageType, uint8_t* inBuf, size_t len, uint8_t* outBuf, size_t outBufLen)
            {
                if (outBufLen < len + 5) {
                    ESP_LOGE(TAG, "Invalid size on output buffer for encoding message of type %u", messageType);
                    return 0;
                }

                outBuf[0] = MAGIC1;
                outBuf[1] = MAGIC2;
                outBuf[2] = messageType;
                uint8_t cksum1 = 0;
                uint8_t cksum2 = 0;
                for (uint16_t i = 0; i < len; i++) {
                    uint8_t val = inBuf[i] & 0xff;
                    outBuf[3 + i] = val;
                    if (i % 2 == 0) {
                        cksum1 += val;
                    } else {
                        cksum2 += val;
                    }
                }
                outBuf[len + 3] = cksum1 & 0xff;
                outBuf[len + 4] = cksum2 & 0xff;

                return len + 5;
            }

            void write_to_scale_characteristic(uint8_t* buf, size_t len)
            {
                auto status =
                    esp_ble_gattc_write_char(this->parent_->get_gattc_if(), this->parent_->get_conn_id(), this->char_handle_,
                                         len, buf, ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
                if (status) {
                    ESP_LOGW(TAG, "[%s] esp_ble_gattc_write_char failed, status=%d", this->parent_->address_str().c_str(),
                             status);
                }
            }

            bool connected = false;

            uint16_t conn_id_;

            uint16_t char_handle_;
            uint64_t last_heartbeat;

            uint8_t packetBuffer[PACKET_BUFFER_SIZE];
            uint16_t packetBufferHead = 0;

            uint8_t unit;
            uint8_t auto_off;
            bool beep_on;

            CallbackManager<void(AcaiaMessage)> on_message_callback_;
        };
    }
}

#endif //SMART_LCC_ACAIA_H
