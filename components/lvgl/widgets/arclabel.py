"""
LVGL v9.4 Arc Label Widget for ESPHome
Supports:
- Text along an arc
- Rotation
- Direction (clockwise/counter-clockwise)
- Vertical & horizontal align
- Offset
- Recolor
"""

import esphome.config_validation as cv
from esphome.const import CONF_TEXT, CONF_ROTATION

from ..defines import CONF_END_ANGLE, CONF_MAIN, CONF_RADIUS, CONF_START_ANGLE
from ..helpers import lvgl_components_required
from ..lv_validation import lv_angle_degrees, lv_int, lv_text, pixels
from ..lvcode import lv
from ..types import LvType
from . import Widget, WidgetType

CONF_ARCLABEL = "arclabel"
CONF_DIRECTION = "direction"
CONF_TEXT_VERTICAL_ALIGN = "text_vertical_align"
CONF_TEXT_HORIZONTAL_ALIGN = "text_horizontal_align"
CONF_OFFSET = "offset"
CONF_RECOLOR = "recolor"

lv_arclabel_t = LvType("lv_arclabel_t")

SIGNED_ANGLE = cv.int_range(min=-360, max=360)

# Arc label schema
ARCLABEL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEXT): lv_text,
        cv.Optional(CONF_RADIUS, default=100): pixels,
        cv.Optional(CONF_START_ANGLE, default=0): SIGNED_ANGLE,
        cv.Optional(CONF_END_ANGLE, default=360): SIGNED_ANGLE,
        cv.Optional(CONF_ROTATION, default=0): SIGNED_ANGLE,
        cv.Optional(CONF_DIRECTION, default="clockwise"): cv.one_of("clockwise", "counter_clockwise"),
        cv.Optional(CONF_TEXT_VERTICAL_ALIGN, default="center"): cv.one_of("top", "center", "bottom"),
        cv.Optional(CONF_TEXT_HORIZONTAL_ALIGN, default="center"): cv.one_of("leading", "center", "trailing"),
        cv.Optional(CONF_OFFSET, default=0): pixels,
        cv.Optional(CONF_RECOLOR, default=False): cv.boolean,
    }
)

# LVGL enums mapping
DIRECTION_ENUM = {
    "clockwise": lv.LV_ARCLABEL_DIR_CLOCKWISE,
    "counter_clockwise": lv.LV_ARCLABEL_DIR_COUNTER_CLOCKWISE,
}

VERT_ALIGN_ENUM = {
    "top": lv.LV_ARCLABEL_TEXT_ALIGN_LEADING,
    "center": lv.LV_ARCLABEL_TEXT_ALIGN_CENTER,
    "bottom": lv.LV_ARCLABEL_TEXT_ALIGN_TRAILING,
}

HORIZ_ALIGN_ENUM = {
    "leading": lv.LV_ARCLABEL_TEXT_ALIGN_LEADING,
    "center": lv.LV_ARCLABEL_TEXT_ALIGN_CENTER,
    "trailing": lv.LV_ARCLABEL_TEXT_ALIGN_TRAILING,
}


class ArcLabelType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_ARCLABEL,
            lv_arclabel_t,
            (CONF_MAIN,),
            ARCLABEL_SCHEMA,
            modify_schema={cv.Optional(CONF_TEXT): lv_text},
        )

    async def to_code(self, w: Widget, config):
        lvgl_components_required.add(CONF_ARCLABEL)

        # Text
        text = await lv_text.process(config[CONF_TEXT])
        lv.arclabel_set_text(w.obj, text)

        # Radius
        radius = await pixels.process(config.get(CONF_RADIUS, 100))
        lv.arclabel_set_radius(w.obj, radius)

        # Angles
        start_angle = config.get(CONF_START_ANGLE, 0)
        end_angle = config.get(CONF_END_ANGLE, 360)
        rotation = config.get(CONF_ROTATION, 0)
        lv.arclabel_set_angle_size(w.obj, end_angle - start_angle)

        # Widget size
        widget_size = radius * 2 + 50
        lv.obj_set_size(w.obj, widget_size, widget_size)

        # Rotation
        total_rotation = start_angle + rotation
        lv.obj_set_style_transform_rotation(w.obj, total_rotation * 10, 0)

        # Direction (use lv.enum to create proper C++ expression)
        lv.arclabel_set_dir(w.obj, lv.enum(DIRECTION_ENUM[config.get(CONF_DIRECTION, "clockwise")]))

        # Vertical alignment
        lv.arclabel_set_text_vertical_align(
            w.obj, lv.enum(VERT_ALIGN_ENUM[config.get(CONF_TEXT_VERTICAL_ALIGN, "center")])
        )

        # Horizontal alignment
        lv.arclabel_set_text_horizontal_align(
            w.obj, lv.enum(HORIZ_ALIGN_ENUM[config.get(CONF_TEXT_HORIZONTAL_ALIGN, "center")])
        )

        # Offset
        offset = await pixels.process(config.get(CONF_OFFSET, 0))
        lv.arclabel_set_offset(w.obj, offset)

        # Recolor
        lv.arclabel_set_recolor(w.obj, config.get(CONF_RECOLOR, False))

    async def to_code_update(self, w: Widget, config):
        if CONF_TEXT in config:
            text = await lv_text.process(config[CONF_TEXT])
            lv.arclabel_set_text(w.obj, text)

    def get_uses(self):
        return ("label",)


arclabel_spec = ArcLabelType()































