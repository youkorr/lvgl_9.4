# LVGL 9.4 - API pour cacher/afficher des widgets

## Vue d'ensemble

Dans LVGL 9.4, la propriété `hidden: true` utilise le flag `LV_OBJ_FLAG_HIDDEN` pour cacher complètement un widget (comme s'il n'existait pas du tout).

## API C++ LVGL 9.4

```cpp
// Cacher un widget
lv_obj_add_flag(widget, LV_OBJ_FLAG_HIDDEN);

// Afficher un widget
lv_obj_remove_flag(widget, LV_OBJ_FLAG_HIDDEN);

// Vérifier si un widget est caché
if(lv_obj_has_flag(widget, LV_OBJ_FLAG_HIDDEN)) {
    // Le widget est caché
}
```

## Utilisation dans ESPHome YAML

```yaml
lvgl:
  pages:
    - id: main_page
      widgets:
        # Widget caché lors de la création
        - obj:
            id: hidden_button
            hidden: true
            width: 100
            height: 50

        # Widget visible lors de la création
        - obj:
            id: visible_button
            hidden: false  # ou omis (visible par défaut)
            width: 100
            height: 50
```

## Actions pour cacher/afficher dynamiquement

```yaml
# Cacher un widget
on_...:
  - lvgl.widget.hide:
      id: my_widget

# Afficher un widget
on_...:
  - lvgl.widget.show:
      id: my_widget
```

## Implémentation dans le code Python

Le code Python génère correctement les appels LVGL 9.4 :

### Dans `components/lvgl/widgets/__init__.py` :

```python
def add_flag(self, flag):
    if "|" in flag:
        flag = f"(lv_obj_flag_t)({flag})"
    return lv_obj.add_flag(self.obj, literal(flag))  # → lv_obj_add_flag()

def clear_flag(self, flag):
    if "|" in flag:
        flag = f"(lv_obj_flag_t)({flag})"
    return lv_obj.remove_flag(self.obj, literal(flag))  # → lv_obj_remove_flag()
```

### Traitement de la propriété `hidden`:

Lorsque `hidden: true` est spécifié dans le YAML :
1. La propriété est collectée par `collect_props()`
2. Ajoutée à `flag_set` dans `set_obj_properties()`
3. Convertie en `"LV_OBJ_FLAG_HIDDEN"` par `join_enums()`
4. Appliquée via `w.add_flag("LV_OBJ_FLAG_HIDDEN")`
5. Génère l'appel C++ : `lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);`

## Changements entre LVGL 8.x et 9.x

| LVGL 8.x | LVGL 9.x | Notes |
|----------|----------|-------|
| `lv_obj_add_flag()` | `lv_obj_add_flag()` | ✓ Inchangé |
| `lv_obj_clear_flag()` | `lv_obj_remove_flag()` | ⚠️ Renommé |
| `LV_OBJ_FLAG_HIDDEN` | `LV_OBJ_FLAG_HIDDEN` | ✓ Inchangé |

## Problèmes connus dans LVGL 9

1. **Pas de modification des flags pendant le rendering** : Appeler `lv_obj_remove_flag()` pendant `LV_EVENT_DRAW_MAIN` déclenche une assertion dans LVGL 9.x

2. **Performance** : Appeler répétitivement `lv_obj_remove_flag()` peut causer des problèmes de performance car l'objet est invalidé à chaque appel

## Sources

- [LVGL 9.4 Flags Documentation](https://docs.lvgl.io/9.4/details/common-widget-features/flags.html)
- [LVGL GitHub Issue #5678](https://github.com/lvgl/lvgl/issues/5678) - lv_obj_remove_flag() issue
- [LVGL Forum - Problem with Hidden flag](https://forum.lvgl.io/t/problem-with-hidden-flag/15465)
