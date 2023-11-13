import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_FLOW, CONF_WEIGHT, UNIT_KILOGRAM, ICON_RADIOACTIVE, STATE_CLASS_MEASUREMENT, STATE_CLASS_NONE

from .. import acaia_ns, Acaia

CONF_ACAIA_ID = "acaia_id"

UNIT_GRAM_PER_SECOND = "g/s"

AcaiaSensor = acaia_ns.class_(
    "AcaiaSensor",
    cg.Component,
    cg.Parented.template(Acaia),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(AcaiaSensor),
        cv.GenerateID(CONF_ACAIA_ID): cv.use_id(Acaia),
        cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOGRAM,
            icon=ICON_RADIOACTIVE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FLOW): sensor.sensor_schema(
            unit_of_measurement=UNIT_GRAM_PER_SECOND,
            icon=ICON_RADIOACTIVE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ACAIA_ID])

    if weight_config := config.get(CONF_WEIGHT):
        sens = await sensor.new_sensor(weight_config)
        cg.add(var.set_weight(sens))

    if flow_config := config.get(CONF_FLOW):
        sens = await sensor.new_sensor(flow_config)
        cg.add(var.set_flow(sens))