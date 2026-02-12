#pragma once

#include <cstdint>

namespace esphome::gplugk {

static constexpr uint8_t DLMS_HEADER_LENGTH = 16;
static constexpr uint8_t DLMS_HEADER_EXT_OFFSET = 2;  // Extra offset for extended length header
static constexpr uint8_t DLMS_CIPHER_OFFSET = 0;
static constexpr uint8_t DLMS_SYST_OFFSET = 1;
static constexpr uint8_t DLMS_LENGTH_OFFSET = 10;
static constexpr uint8_t TWO_BYTE_LENGTH = 0x82;
static constexpr uint8_t DLMS_LENGTH_CORRECTION = 5;  // Header bytes included in length field
static constexpr uint8_t DLMS_SECBYTE_OFFSET = 11;
static constexpr uint8_t DLMS_FRAMECOUNTER_OFFSET = 12;
static constexpr uint8_t DLMS_FRAMECOUNTER_LENGTH = 4;
static constexpr uint8_t DLMS_PAYLOAD_OFFSET = 16;
static constexpr uint8_t GLO_CIPHERING = 0xDB;
static constexpr uint16_t MAX_MESSAGE_LENGTH = 512;

// Kamstrup security byte: encryption + authentication (bits 4+5 set)
static constexpr uint8_t KAMSTRUP_SECURITY_BYTE = 0x30;

// Data-notification APDU header prepended to decrypted message
static constexpr uint8_t DATA_NOTIFICATION_HEADER[] = {
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x07, 0xE9, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x80, 0x00, 0x00,
};
static constexpr uint8_t DATA_NOTIFICATION_HEADER_SIZE = sizeof(DATA_NOTIFICATION_HEADER);

}  // namespace esphome::gplugk
