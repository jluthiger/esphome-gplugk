#include "gplugk.h"

#include "mbedtls/esp_config.h"
#include "mbedtls/gcm.h"

namespace esphome::gplugk
{

  static constexpr const char *TAG = "gplugk";

  void GplugkComponent::dump_config()
  {
    ESP_LOGCONFIG(TAG,
                  "Gplugk (Kamstrup):\n"
                  "  Read Timeout: %u ms",
                  this->read_timeout_);
#define GPLUGK_LOG_SENSOR(s) LOG_SENSOR("  ", #s, this->s##_sensor_);
    GPLUGK_SENSOR_LIST(GPLUGK_LOG_SENSOR, )
#define GPLUGK_LOG_TEXT_SENSOR(s) LOG_TEXT_SENSOR("  ", #s, this->s##_text_sensor_);
    GPLUGK_TEXT_SENSOR_LIST(GPLUGK_LOG_TEXT_SENSOR, )
  }

  void GplugkComponent::loop()
  {
    size_t avail = this->available();
    if (avail > 0)
    {
      size_t remaining = HDLC_MAX_FRAME_SIZE - this->receive_buffer_.size();
      if (remaining == 0)
      {
        ESP_LOGW(TAG, "Receive buffer full, dropping remaining bytes");
      }
      else
      {
        if (avail > remaining)
        {
          avail = remaining;
        }
        uint8_t buf[64];
        while (avail > 0)
        {
          size_t to_read = std::min(avail, sizeof(buf));
          if (!this->read_array(buf, to_read))
          {
            break;
          }
          avail -= to_read;
          this->receive_buffer_.insert(this->receive_buffer_.end(), buf, buf + to_read);
          this->last_read_ = millis();
        }
      }
    }

    if (!this->receive_buffer_.empty() && millis() - this->last_read_ > this->read_timeout_)
    {
      this->dlms_data_.clear();
      if (!this->parse_hdlc_(this->dlms_data_))
        return;

      uint16_t message_length;
      uint8_t systitle_length;
      uint16_t header_offset;
      if (!this->parse_dlms_(this->dlms_data_, message_length, systitle_length, header_offset))
        return;

      if (message_length > MAX_MESSAGE_LENGTH)
      {
        ESP_LOGE(TAG, "DLMS: Message length invalid: %u", message_length);
        this->receive_buffer_.clear();
        return;
      }

      if (!this->decrypt_(this->dlms_data_, message_length, systitle_length, header_offset))
        return;

      // Insert data-notification APDU header before decrypted payload
      auto payload_pos = this->dlms_data_.begin() + header_offset + DLMS_PAYLOAD_OFFSET;
      this->dlms_data_.insert(payload_pos, DATA_NOTIFICATION_HEADER,
                              DATA_NOTIFICATION_HEADER + DATA_NOTIFICATION_HEADER_SIZE);

      this->decode_cosem_(&this->dlms_data_[header_offset + DLMS_PAYLOAD_OFFSET],
                          message_length + DATA_NOTIFICATION_HEADER_SIZE);
    }
  }

  bool GplugkComponent::parse_hdlc_(std::vector<uint8_t> &dlms_data)
  {
    ESP_LOGV(TAG, "Parsing HDLC frame (%d bytes)", this->receive_buffer_.size());

    // Minimum frame: flag(1) + format(2) + dest(1) + src(1) + ctrl(1) + HCS(2) + flag(1) = 9
    if (this->receive_buffer_.size() < 9)
    {
      ESP_LOGE(TAG, "HDLC: Frame too short (%d bytes)", this->receive_buffer_.size());
      this->receive_buffer_.clear();
      return false;
    }

    if (this->receive_buffer_[0] != HDLC_FLAG)
    {
      ESP_LOGE(TAG, "HDLC: Invalid opening flag: 0x%02X", this->receive_buffer_[0]);
      this->receive_buffer_.clear();
      return false;
    }

    // Frame format field (2 bytes, big-endian)
    uint16_t frame_format = (this->receive_buffer_[1] << 8) | this->receive_buffer_[2];
    uint8_t format_type = (frame_format >> 12) & 0x0F;
    uint16_t frame_length = frame_format & 0x07FF;

    if (format_type != HDLC_FORMAT_TYPE)
    {
      ESP_LOGE(TAG, "HDLC: Unsupported format type: 0x%X", format_type);
      this->receive_buffer_.clear();
      return false;
    }

    // Total bytes: opening flag (1) + frame content (frame_length) + closing flag (1)
    uint16_t total_length = 1 + frame_length + 1;
    if (this->receive_buffer_.size() < total_length)
    {
      ESP_LOGE(TAG, "HDLC: Not enough data (need %u, have %u)", total_length,
               (unsigned)this->receive_buffer_.size());
      this->receive_buffer_.clear();
      return false;
    }

    // Verify closing flag at calculated position (do NOT scan for 0x7E)
    if (this->receive_buffer_[total_length - 1] != HDLC_FLAG)
    {
      ESP_LOGE(TAG, "HDLC: Invalid closing flag at position %u: 0x%02X", total_length - 1,
               this->receive_buffer_[total_length - 1]);
      this->receive_buffer_.clear();
      return false;
    }

    // Verify HCS: CRC-16/X.25 over header bytes (frame_format + dest + src + control)
    if (!crc16_x25_check(&this->receive_buffer_[1], HDLC_HEADER_SIZE, &this->receive_buffer_[HDLC_HCS_OFFSET]))
    {
      ESP_LOGE(TAG, "HDLC: HCS verification failed");
      this->receive_buffer_.clear();
      return false;
    }

    // Verify FCS: CRC-16/X.25 over all bytes between flags, excluding FCS itself
    uint16_t fcs_offset = 1 + frame_length - 2;
    if (!crc16_x25_check(&this->receive_buffer_[1], frame_length - 2, &this->receive_buffer_[fcs_offset]))
    {
      ESP_LOGE(TAG, "HDLC: FCS verification failed");
      this->receive_buffer_.clear();
      return false;
    }

    // Information field starts after HCS, first 3 bytes are LLC header
    uint16_t info_start = HDLC_INFO_OFFSET; // = 8
    uint16_t info_end = fcs_offset;

    if (info_end <= info_start + LLC_HEADER_SIZE)
    {
      ESP_LOGE(TAG, "HDLC: No information field after LLC header");
      this->receive_buffer_.clear();
      return false;
    }

    // Verify LLC header (E6 E7 00)
    if (this->receive_buffer_[info_start] != LLC_HEADER[0] ||
        this->receive_buffer_[info_start + 1] != LLC_HEADER[1] ||
        this->receive_buffer_[info_start + 2] != LLC_HEADER[2])
    {
      ESP_LOGE(TAG, "HDLC: Invalid LLC header: %02X %02X %02X", this->receive_buffer_[info_start],
               this->receive_buffer_[info_start + 1], this->receive_buffer_[info_start + 2]);
      this->receive_buffer_.clear();
      return false;
    }

    // Extract DLMS APDU (after LLC header, before FCS)
    uint16_t dlms_start = info_start + LLC_HEADER_SIZE;
    dlms_data.insert(dlms_data.end(), &this->receive_buffer_[dlms_start], &this->receive_buffer_[info_end]);

    ESP_LOGV(TAG, "HDLC: Extracted %d bytes of DLMS data", info_end - dlms_start);
    return true;
  }

  bool GplugkComponent::parse_dlms_(const std::vector<uint8_t> &dlms_data, uint16_t &message_length,
                                    uint8_t &systitle_length, uint16_t &header_offset)
  {
    ESP_LOGV(TAG, "Parsing DLMS header");

    if (dlms_data.size() < DLMS_HEADER_LENGTH + DLMS_HEADER_EXT_OFFSET)
    {
      ESP_LOGE(TAG, "DLMS: Payload too short");
      this->receive_buffer_.clear();
      return false;
    }

    if (dlms_data[DLMS_CIPHER_OFFSET] != GLO_CIPHERING)
    {
      ESP_LOGE(TAG, "DLMS: Unsupported cipher: 0x%02X", dlms_data[DLMS_CIPHER_OFFSET]);
      this->receive_buffer_.clear();
      return false;
    }

    systitle_length = dlms_data[DLMS_SYST_OFFSET];

    if (systitle_length != 0x08)
    {
      ESP_LOGE(TAG, "DLMS: Unsupported system title length: %u", systitle_length);
      this->receive_buffer_.clear();
      return false;
    }

    message_length = dlms_data[DLMS_LENGTH_OFFSET];
    header_offset = 0;

    if (message_length == TWO_BYTE_LENGTH)
    {
      message_length = encode_uint16(dlms_data[DLMS_LENGTH_OFFSET + 1], dlms_data[DLMS_LENGTH_OFFSET + 2]);
      header_offset = DLMS_HEADER_EXT_OFFSET;
    }

    if (message_length < DLMS_LENGTH_CORRECTION)
    {
      ESP_LOGE(TAG, "DLMS: Message length too short: %u", message_length);
      this->receive_buffer_.clear();
      return false;
    }
    message_length -= DLMS_LENGTH_CORRECTION;

    if (dlms_data.size() - DLMS_HEADER_LENGTH - header_offset != message_length)
    {
      ESP_LOGV(TAG, "DLMS: Length mismatch - payload=%d, header=%d, offset=%d, message=%d",
               dlms_data.size(), DLMS_HEADER_LENGTH, header_offset, message_length);
      ESP_LOGE(TAG, "DLMS: Message has invalid length");
      this->receive_buffer_.clear();
      return false;
    }

    uint8_t sec_byte = dlms_data[header_offset + DLMS_SECBYTE_OFFSET];
    if (sec_byte != KAMSTRUP_SECURITY_BYTE && sec_byte != 0x21 && sec_byte != 0x20)
    {
      ESP_LOGE(TAG, "DLMS: Unsupported security control byte: 0x%02X", sec_byte);
      this->receive_buffer_.clear();
      return false;
    }

    return true;
  }

  bool GplugkComponent::decrypt_(std::vector<uint8_t> &dlms_data, uint16_t message_length, uint8_t systitle_length,
                                 uint16_t header_offset)
  {
    ESP_LOGV(TAG, "Decrypting payload (%u bytes)", message_length);

    // Build IV: system title (8 bytes) + frame counter (4 bytes)
    uint8_t iv[12];
    memcpy(&iv[0], &dlms_data[DLMS_SYST_OFFSET + 1], systitle_length);
    memcpy(&iv[8], &dlms_data[header_offset + DLMS_FRAMECOUNTER_OFFSET], DLMS_FRAMECOUNTER_LENGTH);

    uint8_t *payload_ptr = &dlms_data[header_offset + DLMS_PAYLOAD_OFFSET];

    size_t outlen = 0;
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    mbedtls_gcm_setkey(&gcm_ctx, MBEDTLS_CIPHER_ID_AES, this->decryption_key_.data(), this->decryption_key_.size() * 8);
    mbedtls_gcm_starts(&gcm_ctx, MBEDTLS_GCM_DECRYPT, iv, sizeof(iv));
    auto ret = mbedtls_gcm_update(&gcm_ctx, payload_ptr, message_length, payload_ptr, message_length, &outlen);
    mbedtls_gcm_free(&gcm_ctx);

    if (ret != 0)
    {
      ESP_LOGE(TAG, "Decryption failed with error: %d", ret);
      this->receive_buffer_.clear();
      return false;
    }

    // Post-decrypt validation: first byte must be STRUCTURE (0x02)
    if (payload_ptr[0] != DataType::STRUCTURE)
    {
      ESP_LOGE(TAG, "COSEM: Decrypted data invalid (expected STRUCTURE 0x02, got 0x%02X)", payload_ptr[0]);
      this->receive_buffer_.clear();
      return false;
    }

    ESP_LOGV(TAG, "Decrypted payload: %u bytes", message_length);
    return true;
  }

  void GplugkComponent::decode_cosem_(uint8_t *plaintext, uint16_t message_length)
  {
    ESP_LOGV(TAG, "Decoding COSEM data-notification");
    MeterData data{};
    uint16_t pos = 0;

    // Skip data-notification APDU header: tag(1) + invoke-id(4) + date-time(1+12) = 18 bytes
    if (pos + DATA_NOTIFICATION_HEADER_SIZE > message_length)
    {
      ESP_LOGE(TAG, "COSEM: Too short for data-notification header");
      this->receive_buffer_.clear();
      return;
    }

    if (plaintext[pos] != DATA_NOTIFICATION_TAG)
    {
      ESP_LOGE(TAG, "COSEM: Expected data-notification tag 0x%02X, got 0x%02X", DATA_NOTIFICATION_TAG, plaintext[pos]);
      this->receive_buffer_.clear();
      return;
    }
    pos += DATA_NOTIFICATION_HEADER_SIZE;

    // Parse STRUCTURE header
    if (pos + 2 > message_length)
    {
      ESP_LOGE(TAG, "COSEM: Too short for structure header");
      this->receive_buffer_.clear();
      return;
    }

    if (plaintext[pos] != DataType::STRUCTURE)
    {
      ESP_LOGE(TAG, "COSEM: Expected STRUCTURE, got 0x%02X", plaintext[pos]);
      this->receive_buffer_.clear();
      return;
    }
    pos++;

    uint8_t element_count = plaintext[pos];
    pos++;
    ESP_LOGV(TAG, "COSEM: Structure with %u elements", element_count);

    // First element: VISIBLE_STRING -> meter_name
    if (pos + 2 > message_length)
    {
      ESP_LOGE(TAG, "COSEM: Too short for meter name header");
      this->receive_buffer_.clear();
      return;
    }

    if (plaintext[pos] != DataType::VISIBLE_STRING)
    {
      ESP_LOGE(TAG, "COSEM: Expected VISIBLE_STRING for meter name, got 0x%02X", plaintext[pos]);
      this->receive_buffer_.clear();
      return;
    }
    pos++;

    uint8_t name_length = plaintext[pos];
    pos++;

    if (pos + name_length > message_length)
    {
      ESP_LOGE(TAG, "COSEM: Buffer too short for meter name");
      this->receive_buffer_.clear();
      return;
    }

    uint8_t copy_len = name_length < sizeof(data.meter_name) - 1 ? name_length : sizeof(data.meter_name) - 1;
    memcpy(data.meter_name, &plaintext[pos], copy_len);
    data.meter_name[copy_len] = '\0';
    pos += name_length;

    ESP_LOGV(TAG, "COSEM: Meter name: %s", data.meter_name);

    // Each OBIS entry uses 2 structure elements (code + value), meter_name uses 1
    int obis_count = (element_count - 1) / 2;
    ESP_LOGV(TAG, "COSEM: Expecting %d OBIS entries", obis_count);

    for (int entry = 0; entry < obis_count && pos < message_length; entry++)
    {
      // Read OBIS code: OCTET_STRING (0x09) + length (0x06) + 6-byte code
      if (pos + 8 > message_length)
      {
        ESP_LOGV(TAG, "COSEM: End of data at entry %d, pos %u", entry, pos);
        break;
      }

      if (plaintext[pos] != DataType::OCTET_STRING)
      {
        ESP_LOGE(TAG, "COSEM: Expected OCTET_STRING for OBIS code at entry %d, got 0x%02X", entry, plaintext[pos]);
        this->receive_buffer_.clear();
        return;
      }
      pos++;

      uint8_t obis_len = plaintext[pos];
      pos++;

      if (obis_len != 6)
      {
        ESP_LOGE(TAG, "COSEM: Unexpected OBIS code length: %u", obis_len);
        this->receive_buffer_.clear();
        return;
      }

      if (pos + 6 > message_length)
      {
        ESP_LOGE(TAG, "COSEM: Buffer too short for OBIS code");
        this->receive_buffer_.clear();
        return;
      }

      uint8_t *obis_code = &plaintext[pos];
      uint16_t obis_cd = (obis_code[OBIS_C] << 8) | obis_code[OBIS_D];
      pos += 6;

      // Read data type and value
      if (pos >= message_length)
      {
        ESP_LOGE(TAG, "COSEM: Buffer too short for data type");
        this->receive_buffer_.clear();
        return;
      }

      uint8_t data_type = plaintext[pos];
      pos++;

      float value = 0.0f;
      bool is_numeric = false;

      switch (data_type)
      {
      case DataType::DOUBLE_LONG_UNSIGNED:
      {
        if (pos + 4 > message_length)
        {
          ESP_LOGE(TAG, "COSEM: Buffer too short for DOUBLE_LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        value = static_cast<float>(encode_uint32(plaintext[pos], plaintext[pos + 1],
                                                 plaintext[pos + 2], plaintext[pos + 3]));
        pos += 4;
        is_numeric = true;
        break;
      }
      case DataType::LONG_UNSIGNED:
      {
        if (pos + 2 > message_length)
        {
          ESP_LOGE(TAG, "COSEM: Buffer too short for LONG_UNSIGNED");
          this->receive_buffer_.clear();
          return;
        }
        value = static_cast<float>(encode_uint16(plaintext[pos], plaintext[pos + 1]));
        pos += 2;
        is_numeric = true;
        break;
      }
      case DataType::OCTET_STRING:
      {
        if (pos >= message_length)
        {
          ESP_LOGE(TAG, "COSEM: Buffer too short for OCTET_STRING length");
          this->receive_buffer_.clear();
          return;
        }
        uint8_t data_length = plaintext[pos];
        pos++;

        if (pos + data_length > message_length)
        {
          ESP_LOGE(TAG, "COSEM: Buffer too short for OCTET_STRING data");
          this->receive_buffer_.clear();
          return;
        }

        // Timestamp: 12-byte OCTET_STRING with date-time
        if (obis_cd == OBIS_TIMESTAMP && data_length >= 8)
        {
          uint16_t year = encode_uint16(plaintext[pos], plaintext[pos + 1]);
          uint8_t month = plaintext[pos + 2];
          uint8_t day = plaintext[pos + 3];
          // pos+4 is day-of-week, skip
          uint8_t hour = plaintext[pos + 5];
          uint8_t minute = plaintext[pos + 6];
          uint8_t second = plaintext[pos + 7];

          if (year <= 9999 && month <= 12 && day <= 31 && hour <= 23 && minute <= 59 && second <= 59)
          {
            snprintf(data.timestamp, sizeof(data.timestamp), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                     year, month, day, hour, minute, second);
          }
          else
          {
            ESP_LOGW(TAG, "COSEM: Invalid timestamp values");
          }
        }
        pos += data_length;
        break;
      }
      default:
        ESP_LOGW(TAG, "COSEM: Unknown data type 0x%02X at entry %d", data_type, entry);
        this->receive_buffer_.clear();
        return;
      }

      // Map numeric values to MeterData fields
      if (is_numeric)
      {
        switch (obis_cd)
        {
        case OBIS_ACTIVE_ENERGY_PLUS:
          data.active_energy_plus = value;
          break;
        case OBIS_ACTIVE_ENERGY_MINUS:
          data.active_energy_minus = value;
          break;
        case OBIS_REACTIVE_ENERGY_PLUS:
          data.reactive_energy_plus = value;
          break;
        case OBIS_REACTIVE_ENERGY_MINUS:
          data.reactive_energy_minus = value;
          break;
        case OBIS_METER_ID:
          data.meter_id = value;
          break;
        case OBIS_ACTIVE_POWER_PLUS:
          data.active_power_plus = value;
          break;
        case OBIS_ACTIVE_POWER_MINUS:
          data.active_power_minus = value;
          break;
        case OBIS_REACTIVE_POWER_PLUS:
          data.reactive_power_plus = value;
          break;
        case OBIS_REACTIVE_POWER_MINUS:
          data.reactive_power_minus = value;
          break;
        case OBIS_VOLTAGE_L1:
          data.voltage_l1 = value;
          break;
        case OBIS_VOLTAGE_L2:
          data.voltage_l2 = value;
          break;
        case OBIS_VOLTAGE_L3:
          data.voltage_l3 = value;
          break;
        case OBIS_CURRENT_L1:
          data.current_l1 = value;
          break;
        case OBIS_CURRENT_L2:
          data.current_l2 = value;
          break;
        case OBIS_CURRENT_L3:
          data.current_l3 = value;
          break;
        case OBIS_ACTIVE_POWER_L1:
          data.active_power_l1 = value;
          break;
        case OBIS_ACTIVE_POWER_L2:
          data.active_power_l2 = value;
          break;
        case OBIS_ACTIVE_POWER_L3:
          data.active_power_l3 = value;
          break;
        case OBIS_ACTIVE_POWER_MINUS_L1:
          data.active_power_minus_l1 = value;
          break;
        case OBIS_ACTIVE_POWER_MINUS_L2:
          data.active_power_minus_l2 = value;
          break;
        case OBIS_ACTIVE_POWER_MINUS_L3:
          data.active_power_minus_l3 = value;
          break;
        case OBIS_POWER_FACTOR:
          data.power_factor = value;
          break;
        case OBIS_POWER_FACTOR_L1:
          data.power_factor_l1 = value;
          break;
        case OBIS_POWER_FACTOR_L2:
          data.power_factor_l2 = value;
          break;
        case OBIS_POWER_FACTOR_L3:
          data.power_factor_l3 = value;
          break;
        case OBIS_ACTIVE_ENERGY_PLUS_L1:
          data.active_energy_plus_l1 = value;
          break;
        case OBIS_ACTIVE_ENERGY_PLUS_L2:
          data.active_energy_plus_l2 = value;
          break;
        case OBIS_ACTIVE_ENERGY_PLUS_L3:
          data.active_energy_plus_l3 = value;
          break;
        case OBIS_ACTIVE_ENERGY_MINUS_L1:
          data.active_energy_minus_l1 = value;
          break;
        case OBIS_ACTIVE_ENERGY_MINUS_L2:
          data.active_energy_minus_l2 = value;
          break;
        case OBIS_ACTIVE_ENERGY_MINUS_L3:
          data.active_energy_minus_l3 = value;
          break;
        default:
          ESP_LOGW(TAG, "COSEM: Unknown OBIS CD 0x%04X", obis_cd);
        }
      }
    }

    this->receive_buffer_.clear();

    ESP_LOGI(TAG, "Received valid Kamstrup data");
    this->publish_sensors(data);
    this->status_clear_warning();
  }

} // namespace esphome::gplugk
