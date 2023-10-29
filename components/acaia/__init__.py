import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
)

CODEOWNERS = ["@magnusnordlander"]
acaia_ns = cg.esphome_ns.namespace("esphome::acaia")

Acaia = acaia_ns.class_("Acaia", cg.Component, ble_client.BLEClientNode)

CONF_ACAIA_ID = "acaia_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Acaia),
    }
).extend(ble_client.BLE_CLIENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)