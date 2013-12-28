#ifndef futura_weather_redux_config_h
#define futura_weather_redux_config_h

enum AppMessageKey {
	REQUEST_WEATHER_KEY = 1,
	SET_WEATHER_KEY = 2,
    WEATHER_TEMPERATURE_KEY = 3,
    WEATHER_CONDITIONS_KEY = 4,
	REQUEST_PREFERENCES_KEY = 5,
	SET_PREFERENCES_KEY = 6,
	TEMP_PREFERENCE_KEY = 7,
	WEATHER_UPDATE_PREFERENCE_KEY = 8
};

typedef enum {
	TEMP_FORMAT_CELCIUS = 1,
	TEMP_FORMAT_FAHRENHEIT = 2
} TempFormat;

#endif
