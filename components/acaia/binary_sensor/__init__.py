import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, UNIT_CELSIUS, UNIT_SECOND, ICON_RADIOACTIVE, STATE_CLASS_MEASUREMENT, STATE_CLASS_NONE

from .. import acaia_ns, Acaia

CONF_ACAIA_ID = "pressensor_id"

CONF_CONNECTED = "connected"

AcaiaBinarySensor = acaia_ns.class_(
    "AcaiaBinarySensor",
    cg.Component,
    cg.Parented.template(Acaia),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(AcaiaBinarySensor),
        cv.GenerateID(CONF_ACAIA_ID): cv.use_id(Acaia),
        cv.Optional(CONF_CONNECTED): binary_sensor.binary_sensor_schema(
            icon=ICON_RADIOACTIVE,
        ),
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ACAIA_ID])

    if connected_config := config.get(CONF_CONNECTED):
        sens = await binary_sensor.new_binary_sensor(connected_config)
        cg.add(var.set_connected(sens))
