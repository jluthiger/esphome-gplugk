#pragma once

#include <cstdint>

namespace esphome::gplugk {

// Minimal data type enum for Kamstrup COSEM structures
enum DataType : uint8_t {
  STRUCTURE = 0x02,
  DOUBLE_LONG_UNSIGNED = 0x06,
  OCTET_STRING = 0x09,
  VISIBLE_STRING = 0x0A,
  LONG_UNSIGNED = 0x12,
};

// OBIS code byte indices within 6-byte code (A.B.C.D.E.F)
static constexpr uint8_t OBIS_C = 2;
static constexpr uint8_t OBIS_D = 3;

// OBIS CD values (C << 8 | D) for all 32 Kamstrup entries

// Energy totals
static constexpr uint16_t OBIS_ACTIVE_ENERGY_PLUS = 0x0108;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_MINUS = 0x0208;
static constexpr uint16_t OBIS_REACTIVE_ENERGY_PLUS = 0x0308;
static constexpr uint16_t OBIS_REACTIVE_ENERGY_MINUS = 0x0408;

// Meter ID
static constexpr uint16_t OBIS_METER_ID = 0x0000;

// Total power
static constexpr uint16_t OBIS_ACTIVE_POWER_PLUS = 0x0107;
static constexpr uint16_t OBIS_ACTIVE_POWER_MINUS = 0x0207;
static constexpr uint16_t OBIS_REACTIVE_POWER_PLUS = 0x0307;
static constexpr uint16_t OBIS_REACTIVE_POWER_MINUS = 0x0407;

// Timestamp
static constexpr uint16_t OBIS_TIMESTAMP = 0x0100;

// Voltage
static constexpr uint16_t OBIS_VOLTAGE_L1 = 0x2007;
static constexpr uint16_t OBIS_VOLTAGE_L2 = 0x3407;
static constexpr uint16_t OBIS_VOLTAGE_L3 = 0x4807;

// Current
static constexpr uint16_t OBIS_CURRENT_L1 = 0x1F07;
static constexpr uint16_t OBIS_CURRENT_L2 = 0x3307;
static constexpr uint16_t OBIS_CURRENT_L3 = 0x4707;

// Active power per phase
static constexpr uint16_t OBIS_ACTIVE_POWER_L1 = 0x1507;
static constexpr uint16_t OBIS_ACTIVE_POWER_L2 = 0x2907;
static constexpr uint16_t OBIS_ACTIVE_POWER_L3 = 0x3D07;
static constexpr uint16_t OBIS_ACTIVE_POWER_MINUS_L1 = 0x1607;
static constexpr uint16_t OBIS_ACTIVE_POWER_MINUS_L2 = 0x2A07;
static constexpr uint16_t OBIS_ACTIVE_POWER_MINUS_L3 = 0x3E07;

// Power factor
static constexpr uint16_t OBIS_POWER_FACTOR = 0x0D07;
static constexpr uint16_t OBIS_POWER_FACTOR_L1 = 0x2107;
static constexpr uint16_t OBIS_POWER_FACTOR_L2 = 0x3507;
static constexpr uint16_t OBIS_POWER_FACTOR_L3 = 0x4907;

// Active energy per phase
static constexpr uint16_t OBIS_ACTIVE_ENERGY_PLUS_L1 = 0x1508;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_PLUS_L2 = 0x2908;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_PLUS_L3 = 0x3D08;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_MINUS_L1 = 0x1608;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_MINUS_L2 = 0x2A08;
static constexpr uint16_t OBIS_ACTIVE_ENERGY_MINUS_L3 = 0x3E08;

}  // namespace esphome::gplugk
