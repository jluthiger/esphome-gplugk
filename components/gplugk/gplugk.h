#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esp_log.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include "hdlc.h"
#include "dlms.h"
#include "obis.h"

#include <array>
#include <vector>

namespace esphome::gplugk
{

#ifndef GPLUGK_SENSOR_LIST
#define GPLUGK_SENSOR_LIST(F, SEP)
#endif

#ifndef GPLUGK_TEXT_SENSOR_LIST
#define GPLUGK_TEXT_SENSOR_LIST(F, SEP)
#endif

  struct MeterData
  {
    // Energy totals
    float active_energy_plus = 0.0f;
    float active_energy_minus = 0.0f;
    float reactive_energy_plus = 0.0f;
    float reactive_energy_minus = 0.0f;

    // Meter ID
    float meter_id = 0.0f;

    // Total power
    float active_power_plus = 0.0f;
    float active_power_minus = 0.0f;
    float reactive_power_plus = 0.0f;
    float reactive_power_minus = 0.0f;

    // Voltage
    float voltage_l1 = 0.0f;
    float voltage_l2 = 0.0f;
    float voltage_l3 = 0.0f;

    // Current
    float current_l1 = 0.0f;
    float current_l2 = 0.0f;
    float current_l3 = 0.0f;

    // Active power per phase
    float active_power_l1 = 0.0f;
    float active_power_l2 = 0.0f;
    float active_power_l3 = 0.0f;
    float active_power_minus_l1 = 0.0f;
    float active_power_minus_l2 = 0.0f;
    float active_power_minus_l3 = 0.0f;

    // Power factor
    float power_factor = 0.0f;
    float power_factor_l1 = 0.0f;
    float power_factor_l2 = 0.0f;
    float power_factor_l3 = 0.0f;

    // Active energy per phase
    float active_energy_plus_l1 = 0.0f;
    float active_energy_plus_l2 = 0.0f;
    float active_energy_plus_l3 = 0.0f;
    float active_energy_minus_l1 = 0.0f;
    float active_energy_minus_l2 = 0.0f;
    float active_energy_minus_l3 = 0.0f;

    // Text sensors
    char timestamp[27]{};
    char meter_name[20]{};
  };

  class GplugkComponent : public Component, public uart::UARTDevice
  {
  public:
    GplugkComponent() = default;

    void dump_config() override;
    void loop() override;

    void set_decryption_key(const std::array<uint8_t, 16> &key) { this->decryption_key_ = key; }

    void publish_sensors(MeterData &data)
    {
#define GPLUGK_PUBLISH_SENSOR(s)    \
  if (this->s##_sensor_ != nullptr) \
    s##_sensor_->publish_state(data.s);
      GPLUGK_SENSOR_LIST(GPLUGK_PUBLISH_SENSOR, )

#define GPLUGK_PUBLISH_TEXT_SENSOR(s)    \
  if (this->s##_text_sensor_ != nullptr) \
    s##_text_sensor_->publish_state(data.s);
      GPLUGK_TEXT_SENSOR_LIST(GPLUGK_PUBLISH_TEXT_SENSOR, )
    }

    GPLUGK_SENSOR_LIST(SUB_SENSOR, )
    GPLUGK_TEXT_SENSOR_LIST(SUB_TEXT_SENSOR, )

  protected:
    bool parse_hdlc_(std::vector<uint8_t> &dlms_data);
    bool parse_dlms_(const std::vector<uint8_t> &dlms_data, uint16_t &message_length, uint8_t &systitle_length,
                     uint16_t &header_offset);
    bool decrypt_(std::vector<uint8_t> &dlms_data, uint16_t message_length, uint8_t systitle_length,
                  uint16_t header_offset);
    void decode_cosem_(uint8_t *plaintext, uint16_t message_length);

    std::vector<uint8_t> receive_buffer_;
    std::vector<uint8_t> dlms_data_;
    uint32_t last_read_ = 0;
    uint32_t read_timeout_ = 1000;

    std::array<uint8_t, 16> decryption_key_;
  };

} // namespace esphome::gplugk
