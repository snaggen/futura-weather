#include "pebble.h"

#include "config.h"
#include "preferences.h"
#include "weather.h"
#include "main.h"

static Window *window;

GFont futura_18;
GFont futura_25;
GFont futura_28;
GFont futura_35;
GFont futura_40;
GFont futura_53;

static Layer *statusbar_layer;
static BitmapLayer *statusbar_battery_layer;
static uint32_t statusbar_battery_resource;
static GBitmap *statusbar_battery_bitmap = NULL;
static GRect default_statusbar_frame;

static TextLayer *time_layer;
GRect default_time_frame;

static TextLayer *date_layer;
GRect default_date_frame;

static Layer *weather_layer;
static TextLayer *weather_temperature_layer;
static BitmapLayer *weather_icon_layer;
static GBitmap *weather_icon_bitmap = NULL;
GRect default_weather_frame;



static Preferences *prefs;
static Weather *weather;

uint32_t get_resource_for_weather_conditions(uint32_t conditions) {
	bool is_day = conditions >= 1000;
    switch((conditions - (conditions % 100)) % 1000) {
        case 0:
            APP_LOG(APP_LOG_LEVEL_DEBUG, "Error getting data (conditions returned %d)", (int)conditions);
            return RESOURCE_ID_ICON_CLOUD_ERROR;
        case 200:
            return RESOURCE_ID_WEATHER_THUNDER;
        case 300:
            return RESOURCE_ID_WEATHER_DRIZZLE;
        case 500:
            switch(conditions % 1000) {
                case 511:
                    return RESOURCE_ID_WEATHER_RAIN_SNOW;
            }
            return RESOURCE_ID_WEATHER_RAIN;
        case 600:
            switch(conditions % 1000) {
                case 611:
                    return RESOURCE_ID_WEATHER_SLEET;
                case 612:
                    return RESOURCE_ID_WEATHER_RAIN_SLEET;
                case 615:
                case 616:
                    return RESOURCE_ID_WEATHER_RAIN_SNOW;
            }
            return RESOURCE_ID_WEATHER_SNOW;
        case 700:
            switch(conditions % 1000) {
                case 731:
                case 781:
                    return RESOURCE_ID_WEATHER_WIND;
            }
            return RESOURCE_ID_WEATHER_FOG;
        case 800:
            switch(conditions % 1000) {
                case 800:
					if(is_day)
						return RESOURCE_ID_WEATHER_CLEAR_DAY;
					return RESOURCE_ID_WEATHER_CLEAR_NIGHT;
                case 801:
                case 802:
					if(is_day)
						return RESOURCE_ID_WEATHER_PARTLY_CLOUDY_DAY;
					return RESOURCE_ID_WEATHER_PARTLY_CLOUDY_NIGHT;
                case 803:
                case 804:
                    return RESOURCE_ID_WEATHER_CLOUDY;
            }
        case 900:
            switch(conditions % 1000) {
                case 900:
                case 901:
                case 902:
                    return RESOURCE_ID_WEATHER_WIND;
                case 903:
                    return RESOURCE_ID_WEATHER_HOT;
                case 904:
                    return RESOURCE_ID_WEATHER_COLD;
                case 905:
                    return RESOURCE_ID_WEATHER_WIND;
                case 950:
                case 951:
                case 952:
                case 953:
					if(is_day)
						return RESOURCE_ID_WEATHER_CLEAR_DAY;
					return RESOURCE_ID_WEATHER_CLEAR_NIGHT;
                case 954:
                case 955:
                case 956:
                case 957:
                case 959:
                case 960:
                case 961:
                case 962:
                    return RESOURCE_ID_WEATHER_WIND;
            }
    }
    
    APP_LOG(APP_LOG_LEVEL_WARNING, "Unknown wearther conditions: %d", (int)conditions);
    return RESOURCE_ID_ICON_CLOUD_ERROR;
}

uint32_t get_resource_for_battery_state(BatteryChargeState battery) {
	if((battery.is_charging || battery.is_plugged) && battery.charge_percent > 99)
		return RESOURCE_ID_CHARGED;
	
	if(battery.charge_percent >= 99)
		return battery.is_charging ? RESOURCE_ID_CHARGING_100 : RESOURCE_ID_BATTERY_100;
	if(battery.charge_percent >= 91)
		return battery.is_charging ? RESOURCE_ID_CHARGING_91 : RESOURCE_ID_BATTERY_91;
	if(battery.charge_percent >= 82)
		return battery.is_charging ? RESOURCE_ID_CHARGING_82 : RESOURCE_ID_BATTERY_82;
	if(battery.charge_percent >= 73)
		return battery.is_charging ? RESOURCE_ID_CHARGING_73 : RESOURCE_ID_BATTERY_73;
	if(battery.charge_percent >= 64)
		return battery.is_charging ? RESOURCE_ID_CHARGING_64 : RESOURCE_ID_BATTERY_64;
	if(battery.charge_percent >= 55)
		return battery.is_charging ? RESOURCE_ID_CHARGING_55 : RESOURCE_ID_BATTERY_55;
	if(battery.charge_percent >= 46)
		return battery.is_charging ? RESOURCE_ID_CHARGING_46 : RESOURCE_ID_BATTERY_46;
	if(battery.charge_percent >= 37)
		return battery.is_charging ? RESOURCE_ID_CHARGING_37 : RESOURCE_ID_BATTERY_37;
	if(battery.charge_percent >= 28)
		return battery.is_charging ? RESOURCE_ID_CHARGING_28 : RESOURCE_ID_BATTERY_28;
	if(battery.charge_percent >= 19)
		return battery.is_charging ? RESOURCE_ID_CHARGING_19 : RESOURCE_ID_BATTERY_19;
	if(battery.charge_percent >= 9)
		return battery.is_charging ? RESOURCE_ID_CHARGING_9 : RESOURCE_ID_BATTERY_9;
	
	return battery.is_charging ? RESOURCE_ID_CHARGING_0 : RESOURCE_ID_BATTERY_0;
}



GRect get_statusbar_frame(Preferences *prefs) {
	GRect frame = default_statusbar_frame;
	
	if(!prefs->statusbar)
		frame.origin.y -= frame.size.h;
	return frame;
}

GRect get_time_frame(Preferences *prefs, bool weather_visible) {
	GRect frame = default_time_frame;
	
	if(weather_visible && prefs->statusbar)
		frame.origin.y += 8;
	else if(!weather_visible)
		frame.origin.y = 30;
	
	return frame;
}

GRect get_date_frame(Preferences *prefs, bool weather_visible) {
	GRect frame = default_date_frame;
	
	if(weather_visible && prefs->statusbar)
		frame.origin.y += 4;
	else if(!weather_visible)
		frame.origin.y = 103;
	
	return frame;
}

GRect get_weather_frame(bool weather_visible) {
	GRect frame = default_weather_frame;
	
	if(!weather_visible)
		frame.origin.y = 168; // Pebble screen height
	return frame;
}



void change_preferences(Preferences *old_prefs, Preferences *new_prefs) {
	// old_prefs will be NULL for initialization (app first loaded)
	if(old_prefs == NULL || old_prefs->temp_format != new_prefs->temp_format) {
		if(!weather_needs_update(weather, new_prefs->weather_update_freq))
			update_weather_info(weather);
	}
	if(old_prefs == NULL || old_prefs->weather_provider != new_prefs->weather_provider) {
		weather_request_update();
	}
	// if(old_prefs == NULL || old_prefs->language != new_prefs->language) {
	//	force_tick(ALL_UNITS);
	// }
	if(old_prefs == NULL || old_prefs->statusbar != new_prefs->statusbar) {
		GRect statusbar_frame = get_statusbar_frame(new_prefs);
		GRect time_frame = get_time_frame(new_prefs, true);
		GRect date_frame = get_date_frame(new_prefs, true);
		
		if(old_prefs == NULL) {
			layer_set_frame(statusbar_layer, statusbar_frame);
			layer_set_frame(text_layer_get_layer(time_layer), time_frame);
			layer_set_frame(text_layer_get_layer(date_layer), date_frame);
			
			set_weather_visible(!weather_needs_update(weather, new_prefs->weather_update_freq), false);
		}
		else {
			PropertyAnimation *statusbar_animation = property_animation_create_layer_frame(statusbar_layer, NULL, &statusbar_frame);
			PropertyAnimation *time_animation = property_animation_create_layer_frame(text_layer_get_layer(time_layer), NULL, &time_frame);
			PropertyAnimation *date_animation = property_animation_create_layer_frame(text_layer_get_layer(date_layer), NULL, &date_frame);
			
			animation_set_delay(&statusbar_animation->animation, 0);
			animation_set_delay(&time_animation->animation, 100);
			animation_set_delay(&date_animation->animation, 200);
			
			animation_set_duration(&statusbar_animation->animation, 250);
			animation_set_duration(&time_animation->animation, 250);
			animation_set_duration(&date_animation->animation, 250);
			
			animation_set_curve(&statusbar_animation->animation, new_prefs->statusbar ? AnimationCurveEaseIn : AnimationCurveEaseOut);
			
			AnimationHandlers animation_handlers = { .stopped = animation_stopped_handler };
			animation_set_handlers(&statusbar_animation->animation, animation_handlers, NULL);
			animation_set_handlers(&time_animation->animation, animation_handlers, NULL);
			animation_set_handlers(&date_animation->animation, animation_handlers, NULL);
			
			animation_schedule(&statusbar_animation->animation);
			animation_schedule(&time_animation->animation);
			animation_schedule(&date_animation->animation);
		}
	}
}

void set_weather_visible(bool visible, bool animate) {
	GRect time_frame = get_time_frame(prefs, visible);
	GRect date_frame = get_date_frame(prefs, visible);
	GRect weather_frame = get_weather_frame(visible);
	
	if(animate) {
		if(visible)
			layer_set_hidden(weather_layer, false);
		
		PropertyAnimation *time_animation = property_animation_create_layer_frame(text_layer_get_layer(time_layer), NULL, &time_frame);
		PropertyAnimation *date_animation = property_animation_create_layer_frame(text_layer_get_layer(date_layer), NULL, &date_frame);
		PropertyAnimation *weather_animation = property_animation_create_layer_frame(weather_layer, NULL, &weather_frame);
		
		animation_set_delay(&time_animation->animation, 300);
		animation_set_delay(&date_animation->animation, 150);
		animation_set_delay(&weather_animation->animation, 0);
		
		animation_set_duration(&time_animation->animation, 350);
		animation_set_duration(&date_animation->animation, 350);
		animation_set_duration(&weather_animation->animation, 500);
		
		animation_set_curve(&weather_animation->animation, visible ? AnimationCurveEaseOut : AnimationCurveEaseIn);
		
		AnimationHandlers animation_handlers = { .stopped = animation_stopped_handler };
		animation_set_handlers(&time_animation->animation, animation_handlers, NULL);
		animation_set_handlers(&date_animation->animation, animation_handlers, NULL);
		
		AnimationHandlers weather_animation_handlers = { .stopped = set_weather_visible_animation_stopped_handler };
		animation_set_handlers(&weather_animation->animation, weather_animation_handlers, visible ? (void*)1 : (void*)0);
		
		animation_schedule(&time_animation->animation);
		animation_schedule(&date_animation->animation);
		animation_schedule(&weather_animation->animation);
	}
	else {
		layer_set_frame(text_layer_get_layer(time_layer), time_frame);
		layer_set_frame(text_layer_get_layer(date_layer), date_frame);
		layer_set_frame(weather_layer, weather_frame);
		layer_set_hidden(weather_layer, !visible);
	}
}

void set_weather_visible_animation_stopped_handler(Animation *animation, bool finished, void *context) {
	if(finished)
		layer_set_hidden(weather_layer, context == 0);
	animation_stopped_handler(animation, finished, context);
}



void update_weather_info(Weather *weather) {
    if(weather->conditions % 1000) {
        static char temperature_text[8];
		int temperature = weather_convert_temperature(weather->temperature, prefs->temp_format);
		
        snprintf(temperature_text, 8, "%d\u00B0", temperature);
        text_layer_set_text(weather_temperature_layer, temperature_text);
        
		// TODO: Move this block to another method
        if(10 <= temperature && temperature <= 99) {
            layer_set_frame(text_layer_get_layer(weather_temperature_layer), GRect(70, 19+3, 72, 80));
            text_layer_set_font(weather_temperature_layer, futura_35);
        }
        else if((0 <= temperature && temperature <= 9) || (-9 <= temperature && temperature <= -1)) {
            layer_set_frame(text_layer_get_layer(weather_temperature_layer), GRect(70, 19, 72, 80));
            text_layer_set_font(weather_temperature_layer, futura_40);
        }
        else if((100 <= temperature) || (-99 <= temperature && temperature <= -10)) {
            layer_set_frame(text_layer_get_layer(weather_temperature_layer), GRect(70, 19+3, 72, 80));
            text_layer_set_font(weather_temperature_layer, futura_28);
        }
        else {
            layer_set_frame(text_layer_get_layer(weather_temperature_layer), GRect(70, 19+6, 72, 80));
            text_layer_set_font(weather_temperature_layer, futura_25);
        }
        
        if(weather_icon_bitmap)
            gbitmap_destroy(weather_icon_bitmap);
        weather_icon_bitmap = gbitmap_create_with_resource(get_resource_for_weather_conditions(weather->conditions));
        bitmap_layer_set_bitmap(weather_icon_layer, weather_icon_bitmap);
		
		set_weather_visible(true, true);
    }
}



void animation_stopped_handler(Animation *animation, bool finished, void *context) {
	animation_destroy(animation);
}



void out_sent_handler(DictionaryIterator *sent, void *context) {
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Sending message failed (reason: %d)", (int)reason);
}

void in_received_handler(DictionaryIterator *received, void *context) {
	Tuple *set_weather = dict_find(received, SET_WEATHER_MSG_KEY);
	Tuple *set_preferences = dict_find(received, SET_PREFERENCES_MSG_KEY);
	
	if(set_weather) {
		weather_set(weather, received);
		update_weather_info(weather);
	}
	
	if(set_preferences) {
		Preferences old_prefs = *prefs;
		
		preferences_set(prefs, received);
		change_preferences(&old_prefs, prefs);
		
		preferences_save(prefs);
	}
}

void in_dropped_handler(AppMessageResult reason, void *context) {	
    APP_LOG(APP_LOG_LEVEL_ERROR, "Received message dropped (reason: %d)", (int)reason);
}



int main() {
    init();
    app_event_loop();
    deinit();
}

void init() {
    window = window_create();
    window_set_background_color(window, GColorBlack);
    window_set_fullscreen(window, true);
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload
    });
    
    app_message_register_inbox_received(in_received_handler);
    app_message_register_inbox_dropped(in_dropped_handler);
    app_message_register_outbox_sent(out_sent_handler);
    app_message_register_outbox_failed(out_failed_handler);
    
    const uint32_t inbound_size = 256;
    const uint32_t outbound_size = 256;
    app_message_open(inbound_size, outbound_size);
	
	prefs = preferences_load();
	weather = weather_load_cache();
	
	preferences_send(prefs);
    
    window_stack_push(window, true);
}

void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    
    futura_18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_18));
    futura_25 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_25));
    futura_28 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_28));
    futura_35 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_35));
    futura_40 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_40));
    futura_53 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_FUTURA_CONDENSED_53));
	
	
	
	statusbar_layer = layer_create(default_statusbar_frame = GRect(0, 0, 144, 15));
	
	statusbar_battery_layer = bitmap_layer_create(GRect(125, 3, 16, 10));
	layer_add_child(statusbar_layer, bitmap_layer_get_layer(statusbar_battery_layer));
	
	layer_add_child(window_layer, statusbar_layer);
	
	
    
    time_layer = text_layer_create(default_time_frame = GRect(0, 2, 144, 162));
    text_layer_set_text_alignment(time_layer, GTextAlignmentCenter);
    text_layer_set_background_color(time_layer, GColorClear);
    text_layer_set_text_color(time_layer, GColorWhite);
    text_layer_set_font(time_layer, futura_53);
    layer_add_child(window_layer, text_layer_get_layer(time_layer));
    
    date_layer = text_layer_create(default_date_frame = GRect(1, 74, 144, 106));
    text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
    text_layer_set_background_color(date_layer, GColorClear);
    text_layer_set_text_color(date_layer, GColorWhite);
    text_layer_set_font(date_layer, futura_18);
    layer_add_child(window_layer, text_layer_get_layer(date_layer));
    
    
	
    weather_layer = layer_create(default_weather_frame = GRect(0, 90, 144, 80));
    
    weather_icon_layer = bitmap_layer_create(GRect(9, 13, 60, 60));
    layer_add_child(weather_layer, bitmap_layer_get_layer(weather_icon_layer));
    
    weather_temperature_layer = text_layer_create(GRect(70, 19, 72, 80));
    text_layer_set_text_color(weather_temperature_layer, GColorWhite);
    text_layer_set_background_color(weather_temperature_layer, GColorClear);
    text_layer_set_font(weather_temperature_layer, futura_40);
    text_layer_set_text_alignment(weather_temperature_layer, GTextAlignmentRight);
    layer_add_child(weather_layer, text_layer_get_layer(weather_temperature_layer));
    
    layer_add_child(window_layer, weather_layer);
	
	
	
	change_preferences(NULL, prefs);
	
	
	
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
	battery_state_service_subscribe(handle_battery);
    
	// "Force" a tick with all units (draws everything, since we're only drawing what we need)
    force_tick(ALL_UNITS);
	
	// "Force" a battery update with the current battery status
	handle_battery(battery_state_service_peek());
}

void window_unload(Window *window) {
    text_layer_destroy(time_layer);
    text_layer_destroy(date_layer);
	
    if(weather_icon_bitmap)
        gbitmap_destroy(weather_icon_bitmap);
    text_layer_destroy(weather_temperature_layer);
    bitmap_layer_destroy(weather_icon_layer);
    layer_destroy(weather_layer);
	
	if(statusbar_battery_bitmap)
		gbitmap_destroy(statusbar_battery_bitmap);
	bitmap_layer_destroy(statusbar_battery_layer);
	layer_destroy(statusbar_layer);
    
    fonts_unload_custom_font(futura_18);
    fonts_unload_custom_font(futura_25);
    fonts_unload_custom_font(futura_28);
    fonts_unload_custom_font(futura_35);
    fonts_unload_custom_font(futura_40);
    fonts_unload_custom_font(futura_53);
}

void deinit() {
    window_destroy(window);
}



void handle_tick(struct tm *now, TimeUnits units_changed) {
    if(units_changed & MINUTE_UNIT) {
        static char time_text[6];
		strftime(time_text, 6, clock_is_24h_style() ? "%H:%M" : "%I:%M", now);
		
        text_layer_set_text(time_layer, time_text);
    }
    
    if(units_changed & DAY_UNIT) {
        static char date_text[18]; // 18 characters should be enougth to fit the most common language formats
        strftime(date_text, sizeof(date_text), "%a %b %d",  now);
        text_layer_set_text(date_layer, date_text);
    }
    
	bool outdated = weather_needs_update(weather, prefs->weather_outdated_time);
    if(outdated || weather_needs_update(weather, prefs->weather_update_freq))
        weather_request_update();
	if(outdated)
		set_weather_visible(false, true);
}

void handle_battery(BatteryChargeState battery) {
	uint32_t new_battery_resource = get_resource_for_battery_state(battery);
	if(!statusbar_battery_bitmap || new_battery_resource != statusbar_battery_resource) {
		statusbar_battery_resource = new_battery_resource;
		
		if(statusbar_battery_bitmap)
			gbitmap_destroy(statusbar_battery_bitmap);
		statusbar_battery_bitmap = gbitmap_create_with_resource(statusbar_battery_resource);
		bitmap_layer_set_bitmap(statusbar_battery_layer, statusbar_battery_bitmap);
		
		// TODO: Why is this needed to redraw the bitmap?
		force_tick(ALL_UNITS);
	}
}



void force_tick(TimeUnits units_changed) {
    time_t then = time(NULL);
    struct tm *now = localtime(&then);
	
    handle_tick(now, units_changed);
}
