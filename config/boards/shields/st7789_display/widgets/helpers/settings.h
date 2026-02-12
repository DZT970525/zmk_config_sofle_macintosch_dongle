typedef struct {
    uint8_t current_theme;
    bool mute;
} settings_t;

int snake_settings_save_current_theme(uint8_t current_theme);
uint8_t snake_settings_get_current_theme(void);

int snake_settings_toggle_mute(void);
bool snake_settings_get_mute(void);