import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_GPLUGK_ID, GplugkComponent

AUTO_LOAD = ["gplugk"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GPLUGK_ID): cv.use_id(GplugkComponent),
        cv.Optional("timestamp"): text_sensor.text_sensor_schema(),
        cv.Optional("meter_name"): text_sensor.text_sensor_schema(),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GPLUGK_ID])

    text_sensors = []
    for key, conf in config.items():
        if not isinstance(conf, dict):
            continue
        id = conf[CONF_ID]
        if id and id.type == text_sensor.TextSensor:
            sens = await text_sensor.new_text_sensor(conf)
            cg.add(getattr(hub, f"set_{key}_text_sensor")(sens))
            text_sensors.append(f"F({key})")

    if text_sensors:
        cg.add_define(
            "GPLUGK_TEXT_SENSOR_LIST(F, sep)",
            cg.RawExpression(" sep ".join(text_sensors)),
        )
