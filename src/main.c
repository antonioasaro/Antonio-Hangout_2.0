#include "pebble.h"
#include "vars.h"

Window *window;
TextLayer *layer_date_text;
TextLayer *layer_time_text;
TextLayer *layer_word_text;

BitmapLayer *layer_batt_img;
BitmapLayer *layer_conn_img;
GBitmap *img_battery_full;
GBitmap *img_battery_half;
GBitmap *img_battery_low;
GBitmap *img_battery_charge;
GBitmap *img_bt_connect;
GBitmap *img_bt_disconnect;
TextLayer *layer_batt_text;
int charge_percent = 0;
int cur_day = -1;
bool new_word = true;
	
	
#define INT_DIGITS 5		/* enough for 64 bit integer */
char *itoa(int i)
{
  /* Room for INT_DIGITS digits, - and '\0' */
  static char buf[INT_DIGITS + 2];
  char *p = buf + INT_DIGITS + 1;	/* points to terminating '\0' */
  if (i >= 0) {
    do {
      *--p = '0' + (i % 10);
      i /= 10;
    } while (i != 0);
    return p;
  }
  else {			/* i < 0 */
    do {
      *--p = '0' - (i % 10);
      i /= 10;
    } while (i != 0);
    *--p = '-';
  }
  return p;
}
	
void handle_battery(BatteryChargeState charge_state) {
    static char battery_text[] = " 100 ";

    if (charge_state.is_charging) {
        bitmap_layer_set_bitmap(layer_batt_img, img_battery_charge);
        snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
    } else {
        snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
        if (charge_state.charge_percent <= 20) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_low);
        } else if (charge_state.charge_percent <= 50) {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_half);
        } else {
            bitmap_layer_set_bitmap(layer_batt_img, img_battery_full);
        }
    }
    charge_percent = charge_state.charge_percent; 
    text_layer_set_text(layer_batt_text, battery_text);
}

void handle_bluetooth(bool connected) {
    if (connected) {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);
    } else {
        bitmap_layer_set_bitmap(layer_conn_img, img_bt_disconnect);
        vibes_long_pulse();
    }
}

void handle_appfocus(bool in_focus){
    if (in_focus) {
        handle_bluetooth(bluetooth_connection_service_peek());
        handle_battery(battery_state_service_peek());
    }
}

void update_time(struct tm *t) {
	static char dateText[] = "XXX XXX 00"; 
    strftime(dateText, sizeof(dateText), "%a %b %d", t);
	text_layer_set_text(layer_date_text, dateText);

	static char hourText[] = "04:44pm"; 	//this is the longest possible text based on the font used
	if(clock_is_24h_style())
		strftime(hourText, sizeof(hourText), "%H:%M", t);
	else
		strftime(hourText, sizeof(hourText), "%I:%M", t);
	if (hourText[0] == '0') { hourText[0] = ' '; }
	if (t->tm_hour < 12) strcat(hourText, "am"); else strcat(hourText, "pm");
	text_layer_set_text(layer_time_text, hourText);
	
    static int word_idx;
	static int word_len;
	static int lttr_msk;
	static int rdm_lttr;
	static int pick_msk;
	static int cmpl_msk;
	static char word_text[16];
	static char owrd_text[32];
    static char blanks[]    = "                               ";
	if (new_word) {
		lttr_msk = 0; pick_msk = 0;
		word_idx = rand() % WL_LEN;
		strcpy(word_text, wlst[word_idx]);
		word_len = strlen(word_text);
		cmpl_msk = (1 << word_len) - 1;
		strncpy(owrd_text, blanks, 2*word_len-1);
  		owrd_text[2*word_len] = '\0';
		new_word = false;
	} else {
		if (lttr_msk == cmpl_msk) {
			new_word = true;
		} else {
			rdm_lttr = rand() % word_len;
			pick_msk = 1 << rdm_lttr;
			while (lttr_msk & pick_msk) {
				rdm_lttr = rand() % word_len;
				pick_msk = 1 << rdm_lttr;	
			}
			lttr_msk = lttr_msk | pick_msk;
		}
	}
	
	for (int i=0; i<word_len; i++) { 
		if (lttr_msk & (1<<i)) {
			owrd_text[2*i] = word_text[i]; 
		} else {
			owrd_text[2*i] = '_'; 
		}
	}
	
	text_layer_set_text(layer_word_text, owrd_text);
}

void set_style(void) {

	window_set_background_color(window, GColorBlack);
    text_layer_set_text_color(layer_time_text, GColorWhite);
#ifdef PBL_BW
    text_layer_set_text_color(layer_batt_text, GColorWhite);
    text_layer_set_text_color(layer_date_text, GColorWhite);
    text_layer_set_text_color(layer_word_text, GColorWhite);
#else
    text_layer_set_text_color(layer_batt_text, GColorBrightGreen);
    text_layer_set_text_color(layer_date_text, GColorYellow);
    text_layer_set_text_color(layer_word_text, GColorShockingPink);
#endif
}

void force_update(void) {
    handle_battery(battery_state_service_peek());
    handle_bluetooth(bluetooth_connection_service_peek());
    time_t now = time(NULL);
    update_time(localtime(&now));
}

void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
    update_time(tick_time);
}

void handle_deinit(void) {
    tick_timer_service_unsubscribe();
    battery_state_service_unsubscribe();
    bluetooth_connection_service_unsubscribe();
    app_focus_service_unsubscribe();
    accel_tap_service_unsubscribe();
}

void handle_tap(AccelAxisType axis, int32_t direction) {
    persist_write_bool(STYLE_KEY, !persist_read_bool(STYLE_KEY));
    set_style();
    force_update();
    vibes_long_pulse();
    accel_tap_service_unsubscribe();
}

void handle_tap_timeout(void* data) {
    accel_tap_service_unsubscribe();
}

void handle_init(void) {
    window = window_create();
    window_stack_push(window, true /* Animated */);

    // resources
    img_bt_connect     = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CONNECT);
    img_bt_disconnect  = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_DISCONNECT);
    img_battery_full   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_FULL);
    img_battery_half   = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_HALF);
    img_battery_low    = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_LOW);
    img_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_CHARGE);

#ifdef PBL_ROUND
#define XOFF 18
#define YOFF 8
#else
#define XOFF 0
#define YOFF 0
#endif

	// layers
    layer_batt_img  = bitmap_layer_create(GRect(18+XOFF, 10+YOFF, 16, 16));
	layer_batt_text = text_layer_create(GRect(10+XOFF,20+YOFF,30,20));
    layer_conn_img  = bitmap_layer_create(GRect(120, 10+YOFF, 20, 20));
	layer_date_text = text_layer_create(GRect(6+XOFF, 48, 144-8, 30));
    layer_time_text = text_layer_create(GRect(10+XOFF, 74, 144-7, 50));
	layer_word_text = text_layer_create(GRect(4+XOFF, 132-YOFF, 144-7, 30));
	
    text_layer_set_text_color(layer_word_text, GColorWhite);
	text_layer_set_background_color(layer_word_text, GColorClear);
    text_layer_set_font(layer_word_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));
    text_layer_set_text_alignment(layer_word_text, GTextAlignmentCenter);

    text_layer_set_background_color(layer_date_text, GColorClear);
    text_layer_set_font(layer_date_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_22)));
	text_layer_set_text_alignment(layer_date_text, GTextAlignmentCenter);
	
    text_layer_set_background_color(layer_time_text, GColorClear);
    text_layer_set_font(layer_time_text, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_44)));
    text_layer_set_text_alignment(layer_time_text, GTextAlignmentCenter);
	
    text_layer_set_background_color(layer_batt_text, GColorClear);
    text_layer_set_font(layer_batt_text, fonts_get_system_font(FONT_KEY_FONT_FALLBACK));
    text_layer_set_text_alignment(layer_batt_text, GTextAlignmentCenter);

    bitmap_layer_set_bitmap(layer_batt_img, img_battery_full);
    bitmap_layer_set_bitmap(layer_conn_img, img_bt_connect);

    // composing layers
    Layer *window_layer = window_get_root_layer(window);
    layer_add_child(window_layer, bitmap_layer_get_layer(layer_batt_img));
    layer_add_child(window_layer, bitmap_layer_get_layer(layer_conn_img));
    layer_add_child(window_layer, text_layer_get_layer(layer_date_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_time_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_batt_text));
    layer_add_child(window_layer, text_layer_get_layer(layer_word_text));
	srand(time(NULL));

    // style
    set_style();

    // handlers
    battery_state_service_subscribe(&handle_battery);
    bluetooth_connection_service_subscribe(&handle_bluetooth);
    app_focus_service_subscribe(&handle_appfocus);
    tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);
    accel_tap_service_subscribe(handle_tap);
    app_timer_register(2000, handle_tap_timeout, NULL);

    // draw first frame
    force_update();
}


int main(void) {
    handle_init();

    app_event_loop();

    handle_deinit();
}
