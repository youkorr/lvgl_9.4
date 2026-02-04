"""
LVGL Arc Label Widget Implementation
Compatible ESPHome + LVGL 9.4
Rotation implemented via object transform (ESPHome-safe)
"""

import esphome.config_validation as cv
from esphome.const import CONF_ROTATION, CONF_TEXT

from ..defines import (
    CONF_END_ANGLE,
    CONF_MAIN,
    CONF_RADIUS,
    CONF_START_ANGLE,
)
from ..helpers import lvgl_components_required
from ..lv_validation import lv_angle_degrees, lv_text, pixels
from ..lvcode import lv
from ..types import LvType
from . import Widget, WidgetType

CONF_ARCLABEL = "arclabel"

lv_arclabel_t = LvType("lv_arclabel_t")

# Arc label schema
ARCLABEL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEXT): lv_text,
        cv.Optional(CONF_RADIUS, default=100): pixels,
        cv.Optional(CONF_START_ANGLE, default=0): lv_angle_degrees,
        cv.Optional(CONF_END_ANGLE, default=360): lv_angle_degrees,
        cv.Optional(CONF_ROTATION, default=0): lv_angle_degrees,
    }
)

class ArcLabelType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ARCLABEL,
            lv_arclabel_t,
            (CONF_MAIN,),
            ARCLABEL_SCHEMA,
            modify_schema={
                cv.Optional(CONF_TEXT): lv_text,
            },
        )

    async def to_code(self, w: Widget, config):
        lvgl_components_required.add(CONF_ARCLABEL)

        # Set text
        text = await lv_text.process(config[CONF_TEXT])
        lv.arclabel_set_text(w.obj, text)

        # Set radius
        radius = await pixels.process(config.get(CONF_RADIUS, 100))
        lv.arclabel_set_radius(w.obj, radius)

        # Arc angle size (start angle not supported by arclabel API)
        start_angle = config.get(CONF_START_ANGLE, 0)
        end_angle = config.get(CONF_END_ANGLE, 360)

        angle_size = end_angle - start_angle
        if angle_size <= 0:
            angle_size += 360

        lv.arclabel_set_angle_size(w.obj, angle_size)

        # Rotation via LVGL object transform (0.1° units)
        rotation = config.get(CONF_ROTATION, 0)
        lv.obj_set_style_transform_angle(
            w.obj,
            int(rotation * 10),
            0,  # LV_PART_MAIN (ESPHome-safe)
        )

        # Rotate around center
        lv.obj_set_style_transform_pivot_x(w.obj, radius, 0)
        lv.obj_set_style_transform_pivot_y(w.obj, radius, 0)

        # Widget size
        size = radius * 2 + 50
        lv.obj_set_size(w.obj, size, size)

    async def to_code_update(self, w: Widget, config):
        if CONF_TEXT in config:
            text = await lv_text.process(config[CONF_TEXT])
            lv.arclabel_set_text(w.obj, text)

    def get_uses(self):
        return ("label",)


# Global instance
arclabel_spec = ArcLabelType()















































