#include <tizen.h>
#include "watch.h"
#include <cairo.h>
#include <math.h>

#define WATCH_RADIUS 180

typedef struct appdata {
	Evas_Coord width;
	Evas_Coord height;

	Evas_Object *win;
	Evas_Object *img;

	cairo_t *cairo;
	cairo_surface_t *surface;
	cairo_surface_t *bg_surface;
	cairo_surface_t *amb_bg_surface;

	cairo_pattern_t *hour_needle_color;
	cairo_pattern_t *second_needle_color;
	cairo_pattern_t *bkg1_color;
	cairo_pattern_t *bkg2_color;

	cairo_pattern_t *amb_hour_needle_color;
	cairo_pattern_t *amb_bkg1_color;
	cairo_pattern_t *amb_bkg2_color;
} appdata_s;

#define TEXT_BUF_SIZE 256

static void
draw_hour_needle(cairo_t *cairo, int hours, int minutes) {
	float angle = M_PI * ((hours % 12 + minutes / 60.0) / 6);
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -8, 36, 16, 45 - WATCH_RADIUS);
	cairo_fill(cairo);
	cairo_rotate(cairo, -angle);
}

static void
draw_minute_needle(cairo_t *cairo, int minutes, int seconds) {
	float angle = M_PI * (minutes + seconds / 60.0) / 30;
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -8, 36, 16, -13 - WATCH_RADIUS);
	cairo_fill(cairo);
	cairo_rotate(cairo, -angle);
}

static void
drawCircle(cairo_t *cairo, int x, int y, int radius) {
	cairo_arc(cairo, x, y, radius, 0, M_PI * 2);
	cairo_fill(cairo);
}

static void
draw_second_needle(cairo_t *cairo, int seconds, int milliseconds) {
	float angle = M_PI * (seconds + milliseconds / 1000.0) / 30.0;
	cairo_rotate(cairo, angle);
	cairo_rectangle(cairo, -3, 36, 6, 30 - WATCH_RADIUS);
	cairo_fill(cairo);
	drawCircle(cairo, 0, -110, 15);
	cairo_rotate(cairo, -angle);
}

static void
update_watch(appdata_s *ad, watch_time_h watch_time, int ambient)
{
	int hour24, minute, second, millisecond;
	cairo_pattern_t *hour_needle_color, *bkg1_color, *bkg2_color;

	if (ambient) {
		hour_needle_color = ad->amb_hour_needle_color;
		bkg1_color = ad->amb_bkg1_color;
		bkg2_color = ad->amb_bkg2_color;
	} else {
		hour_needle_color = ad->hour_needle_color;
		bkg1_color = ad->bkg1_color;
		bkg2_color = ad->bkg2_color;
	}

	if (watch_time == NULL)
		return;

	watch_time_get_hour24(watch_time, &hour24);
	watch_time_get_minute(watch_time, &minute);

	if (ambient) {
		cairo_set_source_surface(ad->cairo, ad->amb_bg_surface, -WATCH_RADIUS, -WATCH_RADIUS);
	} else {
		cairo_set_source_surface(ad->cairo, ad->bg_surface, -WATCH_RADIUS, -WATCH_RADIUS);
	}
	cairo_paint(ad->cairo);

	cairo_set_source(ad->cairo, hour_needle_color);
	draw_hour_needle(ad->cairo, hour24, minute);
	if (ambient) {
		draw_minute_needle(ad->cairo, minute, 0);
	} else {
		watch_time_get_second(watch_time, &second);
		watch_time_get_millisecond(watch_time, &millisecond);
		draw_minute_needle(ad->cairo, minute, second);
		cairo_set_source(ad->cairo, ad->second_needle_color);
		draw_second_needle(ad->cairo, second, millisecond);
		drawCircle(ad->cairo, 0, 0, 6);
	}

	cairo_set_source(ad->cairo, bkg1_color);
	drawCircle(ad->cairo, 0, 0, 2);

	cairo_set_source(ad->cairo, bkg2_color);
	drawCircle(ad->cairo, 0, 0, 1);

	cairo_surface_flush(ad->surface);

	/* display cairo drawing on screen */
	unsigned char * imageData = cairo_image_surface_get_data(cairo_get_target(ad->cairo));
	evas_object_image_data_set(ad->img, imageData);
	evas_object_image_data_update_add(ad->img, 0, 0, ad->width, ad->height);
}

static void
create_base_gui(appdata_s *ad, int width, int height)
{
	int ret;
	watch_time_h watch_time = NULL;

	/* Window */
	ret = watch_app_get_elm_win(&ad->win);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get window. err = %d", ret);
		return;
	}

	ad->width = width;
	ad->height = height;
	evas_object_resize(ad->win, ad->width, ad->height);

	elm_win_autodel_set(ad->win, EINA_TRUE);
	if (elm_win_wm_rotation_supported_get(ad->win))
	{
		int rots[4] = { 0, 90, 180, 270 };
		elm_win_wm_rotation_available_rotations_set(ad->win, (const int *)(&rots), 4);
	}
	evas_object_show(ad->win);

	/* Image */
	ad->img = evas_object_image_filled_add(evas_object_evas_get(ad->win));
	evas_object_image_size_set(ad->img, ad->width, ad->height);
	evas_object_resize(ad->img, ad->width, ad->height);
	evas_object_show(ad->img);

	ad->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, ad->width, ad->height);
	ad->cairo = cairo_create(ad->surface);

	/* draw watch face background */
	ad->bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch/res/images/clear_bg.png");
	ad->amb_bg_surface = cairo_image_surface_create_from_png("/opt/usr/apps/org.example.watch/res/images/dark_bg.png");
	if (ad->bg_surface == NULL || ad->amb_bg_surface == NULL) {
		dlog_print(DLOG_ERROR, LOG_TAG, "Could not create background image from file");
	}

	ad->hour_needle_color = cairo_pattern_create_rgb(0.19, 0.19, 0.19);
	ad->second_needle_color = cairo_pattern_create_rgb(0.745, 0.0, 0.062);
	ad->bkg1_color = cairo_pattern_create_rgb(0.733, 0.733, 0.733);
	ad->bkg2_color = cairo_pattern_create_rgb(0.0, 0.0, 0.0);

	ad->amb_hour_needle_color = cairo_pattern_create_rgb(0.86, 0.86, 0.86);
	ad->amb_bkg1_color = cairo_pattern_create_rgb(0.733, 0.733, 0.733);
	ad->amb_bkg2_color = cairo_pattern_create_rgb(0.0, 0.0, 0.0);

	cairo_translate(ad->cairo, WATCH_RADIUS, WATCH_RADIUS);

	ret = watch_time_get_current_time(&watch_time);
	if (ret != APP_ERROR_NONE)
		dlog_print(DLOG_ERROR, LOG_TAG, "failed to get current time. err = %d", ret);

	update_watch(ad, watch_time, 0);

	watch_time_delete(watch_time);
}

static bool
app_create(int width, int height, void *data)
{
	/* Hook to take necessary actions before main event loop starts
		Initialize UI resources and application's data
		If this function returns true, the main loop of application starts
		If this function returns false, the application is terminated */
	appdata_s *ad = data;

	create_base_gui(ad, width, height);

	return true;
}

static void
app_control(app_control_h app_control, void *data)
{
	/* Handle the launch request. */
}

static void
app_pause(void *data)
{
	/* Take necessary actions when application becomes invisible. */
}

static void
app_resume(void *data)
{
	/* Take necessary actions when application becomes visible. */
}

static void
app_terminate(void *data)
{
	/* Release all resources. */
}

static void
app_time_tick(watch_time_h watch_time, void *data)
{
	/* Called at each second while your app is visible. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 0);
}

static void
app_ambient_tick(watch_time_h watch_time, void *data)
{
	/* Called at each minute while the device is in ambient mode. Update watch UI. */
	appdata_s *ad = data;
	update_watch(ad, watch_time, 1);
}

static void
app_ambient_changed(bool ambient_mode, void *data)
{
	/* Update your watch UI to conform to the ambient mode */
}

static void
watch_app_lang_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_LANGUAGE_CHANGED*/
	char *locale = NULL;
	app_event_get_language(event_info, &locale);
	elm_language_set(locale);
	free(locale);
	return;
}

static void
watch_app_region_changed(app_event_info_h event_info, void *user_data)
{
	/*APP_EVENT_REGION_FORMAT_CHANGED*/
}

int
main(int argc, char *argv[])
{
	appdata_s ad = {0,};
	int ret = 0;

	watch_app_lifecycle_callback_s event_callback = {0,};
	app_event_handler_h handlers[5] = {NULL, };

	event_callback.create = app_create;
	event_callback.terminate = app_terminate;
	event_callback.pause = app_pause;
	event_callback.resume = app_resume;
	event_callback.app_control = app_control;
	event_callback.time_tick = app_time_tick;
	event_callback.ambient_tick = app_ambient_tick;
	event_callback.ambient_changed = app_ambient_changed;

	watch_app_add_event_handler(&handlers[APP_EVENT_LANGUAGE_CHANGED],
		APP_EVENT_LANGUAGE_CHANGED, watch_app_lang_changed, &ad);
	watch_app_add_event_handler(&handlers[APP_EVENT_REGION_FORMAT_CHANGED],
		APP_EVENT_REGION_FORMAT_CHANGED, watch_app_region_changed, &ad);

	ret = watch_app_main(argc, argv, &event_callback, &ad);
	if (ret != APP_ERROR_NONE) {
		dlog_print(DLOG_ERROR, LOG_TAG, "watch_app_main() is failed. err = %d", ret);
	}

	return ret;
}

