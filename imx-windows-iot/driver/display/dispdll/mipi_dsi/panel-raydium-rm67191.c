// SPDX-License-Identifier: GPL-2.0
/*
 * Raydium RM67191 MIPI-DSI panel driver
 *
 * Copyright 2019, 2023 NXP
 */

#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>

#include "mipi_dsi/mipi_display.h"
#include <video/videomode.h>

#include <drm/drm_crtc.h>
#include "mipi_dsi/drm_mipi_dsi.h"
#include <drm/drm_panel.h>
#include "mipi_dsi/panel-raydium-rm67191.h"

/* Panel specific color-format bits */
#define COL_FMT_16BPP 0x55
#define COL_FMT_18BPP 0x66
#define COL_FMT_24BPP 0x77

/* Write Manufacture Command Set Control */
#define WRMAUCCTR 0xFE

/* Manufacturer Command Set pages (CMD2) */
struct cmd_set_entry {
	u8 cmd;
	u8 param;
};

/*
 * There is no description in the Reference Manual about these commands.
 * We received them from vendor, so just use them as is.
 */
static const struct cmd_set_entry mcs_rm67191[] = {
	{0xFE, 0x0B}, {0x28, 0x40}, {0x29, 0x4F}, {0xFE, 0x0E},
	{0x4B, 0x00}, {0x4C, 0x0F}, {0x4D, 0x20}, {0x4E, 0x40},
	{0x4F, 0x60}, {0x50, 0xA0}, {0x51, 0xC0}, {0x52, 0xE0},
	{0x53, 0xFF}, {0xFE, 0x0D}, {0x18, 0x08}, {0x42, 0x00},
	{0x08, 0x41}, {0x46, 0x02}, {0x72, 0x09}, {0xFE, 0x0A},
	{0x24, 0x17}, {0x04, 0x07}, {0x1A, 0x0C}, {0x0F, 0x44},
	{0xFE, 0x04}, {0x00, 0x0C}, {0x05, 0x08}, {0x06, 0x08},
	{0x08, 0x08}, {0x09, 0x08}, {0x0A, 0xE6}, {0x0B, 0x8C},
	{0x1A, 0x12}, {0x1E, 0xE0}, {0x29, 0x93}, {0x2A, 0x93},
	{0x2F, 0x02}, {0x31, 0x02}, {0x33, 0x05}, {0x37, 0x2D},
	{0x38, 0x2D}, {0x3A, 0x1E}, {0x3B, 0x1E}, {0x3D, 0x27},
	{0x3F, 0x80}, {0x40, 0x40}, {0x41, 0xE0}, {0x4F, 0x2F},
	{0x50, 0x1E}, {0xFE, 0x06}, {0x00, 0xCC}, {0x05, 0x05},
	{0x07, 0xA2}, {0x08, 0xCC}, {0x0D, 0x03}, {0x0F, 0xA2},
	{0x32, 0xCC}, {0x37, 0x05}, {0x39, 0x83}, {0x3A, 0xCC},
	{0x41, 0x04}, {0x43, 0x83}, {0x44, 0xCC}, {0x49, 0x05},
	{0x4B, 0xA2}, {0x4C, 0xCC}, {0x51, 0x03}, {0x53, 0xA2},
	{0x75, 0xCC}, {0x7A, 0x03}, {0x7C, 0x83}, {0x7D, 0xCC},
	{0x82, 0x02}, {0x84, 0x83}, {0x85, 0xEC}, {0x86, 0x0F},
	{0x87, 0xFF}, {0x88, 0x00}, {0x8A, 0x02}, {0x8C, 0xA2},
	{0x8D, 0xEA}, {0x8E, 0x01}, {0x8F, 0xE8}, {0xFE, 0x06},
	{0x90, 0x0A}, {0x92, 0x06}, {0x93, 0xA0}, {0x94, 0xA8},
	{0x95, 0xEC}, {0x96, 0x0F}, {0x97, 0xFF}, {0x98, 0x00},
	{0x9A, 0x02}, {0x9C, 0xA2}, {0xAC, 0x04}, {0xFE, 0x06},
	{0xB1, 0x12}, {0xB2, 0x17}, {0xB3, 0x17}, {0xB4, 0x17},
	{0xB5, 0x17}, {0xB6, 0x11}, {0xB7, 0x08}, {0xB8, 0x09},
	{0xB9, 0x06}, {0xBA, 0x07}, {0xBB, 0x17}, {0xBC, 0x17},
	{0xBD, 0x17}, {0xBE, 0x17}, {0xBF, 0x17}, {0xC0, 0x17},
	{0xC1, 0x17}, {0xC2, 0x17}, {0xC3, 0x17}, {0xC4, 0x0F},
	{0xC5, 0x0E}, {0xC6, 0x00}, {0xC7, 0x01}, {0xC8, 0x10},
	{0xFE, 0x06}, {0x95, 0xEC}, {0x8D, 0xEE}, {0x44, 0xEC},
	{0x4C, 0xEC}, {0x32, 0xEC}, {0x3A, 0xEC}, {0x7D, 0xEC},
	{0x75, 0xEC}, {0x00, 0xEC}, {0x08, 0xEC}, {0x85, 0xEC},
	{0xA6, 0x21}, {0xA7, 0x05}, {0xA9, 0x06}, {0x82, 0x06},
	{0x41, 0x06}, {0x7A, 0x07}, {0x37, 0x07}, {0x05, 0x06},
	{0x49, 0x06}, {0x0D, 0x04}, {0x51, 0x04},
};

static const struct cmd_set_entry mcs_rm67199[] = {
	{0xFE, 0xA0}, {0x2B, 0x18}, {0xFE, 0x70}, {0x7D, 0x05},
	{0x5D, 0x0A}, {0x5A, 0x79}, {0x5C, 0x00}, {0x52, 0x00},
	{0xFE, 0xD0}, {0x40, 0x02}, {0x13, 0x40}, {0xFE, 0x40},
	{0x05, 0x08}, {0x06, 0x08}, {0x08, 0x08}, {0x09, 0x08},
	{0x0A, 0xCA}, {0x0B, 0x88}, {0x20, 0x93}, {0x21, 0x93},
	{0x24, 0x02}, {0x26, 0x02}, {0x28, 0x05}, {0x2A, 0x05},
	{0x74, 0x2F}, {0x75, 0x1E}, {0xAD, 0x00}, {0xFE, 0x60},
	{0x00, 0xCC}, {0x01, 0x00}, {0x02, 0x04}, {0x03, 0x00},
	{0x04, 0x00}, {0x05, 0x07}, {0x06, 0x00}, {0x07, 0x88},
	{0x08, 0x00}, {0x09, 0xCC}, {0x0A, 0x00}, {0x0B, 0x04},
	{0x0C, 0x00}, {0x0D, 0x00}, {0x0E, 0x05}, {0x0F, 0x00},
	{0x10, 0x88}, {0x11, 0x00}, {0x12, 0xCC}, {0x13, 0x0F},
	{0x14, 0xFF}, {0x15, 0x04}, {0x16, 0x00}, {0x17, 0x06},
	{0x18, 0x00}, {0x19, 0x96}, {0x1A, 0x00}, {0x24, 0xCC},
	{0x25, 0x00}, {0x26, 0x02}, {0x27, 0x00}, {0x28, 0x00},
	{0x29, 0x06}, {0x2A, 0x06}, {0x2B, 0x82}, {0x2D, 0x00},
	{0x2F, 0xCC}, {0x30, 0x00}, {0x31, 0x02}, {0x32, 0x00},
	{0x33, 0x00}, {0x34, 0x07}, {0x35, 0x06}, {0x36, 0x82},
	{0x37, 0x00}, {0x38, 0xCC}, {0x39, 0x00}, {0x3A, 0x02},
	{0x3B, 0x00}, {0x3D, 0x00}, {0x3F, 0x07}, {0x40, 0x00},
	{0x41, 0x88}, {0x42, 0x00}, {0x43, 0xCC}, {0x44, 0x00},
	{0x45, 0x02}, {0x46, 0x00}, {0x47, 0x00}, {0x48, 0x06},
	{0x49, 0x02}, {0x4A, 0x8A}, {0x4B, 0x00}, {0x5F, 0xCA},
	{0x60, 0x01}, {0x61, 0xE8}, {0x62, 0x09}, {0x63, 0x00},
	{0x64, 0x07}, {0x65, 0x00}, {0x66, 0x30}, {0x67, 0x80},
	{0x9B, 0x03}, {0xA9, 0x07}, {0xAA, 0x06}, {0xAB, 0x02},
	{0xAC, 0x10}, {0xAD, 0x11}, {0xAE, 0x05}, {0xAF, 0x04},
	{0xB0, 0x10}, {0xB1, 0x10}, {0xB2, 0x10}, {0xB3, 0x10},
	{0xB4, 0x10}, {0xB5, 0x10}, {0xB6, 0x10}, {0xB7, 0x10},
	{0xB8, 0x10}, {0xB9, 0x10}, {0xBA, 0x04}, {0xBB, 0x05},
	{0xBC, 0x00}, {0xBD, 0x01}, {0xBE, 0x0A}, {0xBF, 0x10},
	{0xC0, 0x11}, {0xFE, 0xA0}, {0x22, 0x00},
};

struct rad_panel {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;

	struct gpio_desc *reset;

	bool prepared;
	bool enabled;

	const struct rad_platform_data *pdata;
};

struct rad_platform_data {
	int (*enable)(struct rad_panel *panel);
};

static inline struct rad_panel *to_rad_panel(struct drm_panel *panel)
{
	return container_of(panel, struct rad_panel, panel);
}

static int rad_panel_push_cmd_list(struct mipi_dsi_device *dsi,
				   struct cmd_set_entry const *cmd_set,
				   size_t count)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < count; i++) {
		const struct cmd_set_entry *entry = cmd_set++;
		u8 buffer[2] = { entry->cmd, entry->param };

		ret = (int)mipi_dsi_generic_write(dsi, &buffer, sizeof(buffer));
		if (ret < 0)
			return ret;
	}

	return ret;
};

static int color_format_from_dsi_format(enum mipi_dsi_pixel_format format)
{
	switch (format) {
	case MIPI_DSI_FMT_RGB565:
		return COL_FMT_16BPP;
	case MIPI_DSI_FMT_RGB666:
	case MIPI_DSI_FMT_RGB666_PACKED:
		return COL_FMT_18BPP;
	case MIPI_DSI_FMT_RGB888:
		return COL_FMT_24BPP;
	default:
		return COL_FMT_24BPP; /* for backward compatibility */
	}
};

int rad_panel_prepare(struct drm_panel* panel)
{
	struct rad_panel* rad = to_rad_panel(panel);

	if (rad->prepared)
		return 0;

	/* At lest 10ms needed between power-on and reset-out as RM specifies */
	usleep_range(10000, 12000);

	if (rad->reset) {
		gpiod_set_value_cansleep(rad->reset, 0);
	}

	/*
	 * 50ms delay after reset-out, as per manufacturer initalization
	 * sequence.
	 */
	msleep(50);

	rad->prepared = true;

	return 0;
}

int rad_panel_unprepare(struct drm_panel* panel)
{
	struct rad_panel* rad = to_rad_panel(panel);

	if (!rad->prepared)
		return 0;

	/*
	 * Right after asserting the reset, we need to release it, so that the
	 * touch driver can have an active connection with the touch controller
	 * even after the display is turned off.
	 */
	if (rad->reset) {
		gpiod_set_value_cansleep(rad->reset, 1);
		usleep_range(15000, 17000);
		gpiod_set_value_cansleep(rad->reset, 0);
	}

	rad->prepared = false;

	return 0;
}

static int rm67191_enable(struct rad_panel *panel)
{
	struct mipi_dsi_device *dsi = panel->dsi;
	struct device *dev = &dsi->dev;
	u8 dsi_mode;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret;

	if (panel->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = rad_panel_push_cmd_list(dsi,
				      &mcs_rm67191[0],
				      ARRAY_SIZE(mcs_rm67191));
	if (ret < 0) {
		dev_err(dev, "Failed to send MCS (%d)\n", ret);
		goto fail;
	}

	/* Select User Command Set table (CMD1) */
	ret = (int)mipi_dsi_generic_write(dsi, (u8[]){ WRMAUCCTR, 0x00 }, 2);
	if (ret < 0)
		goto fail;

	/* Software reset */
	ret = mipi_dsi_dcs_soft_reset(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to do Software Reset (%d)\n", ret);
		goto fail;
	}

	usleep_range(15000, 17000);

	/* Set DSI mode */
	dsi_mode = (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) ? 0x0B : 0x00;
	ret = (int)mipi_dsi_generic_write(dsi, (u8[]){ 0xC2, dsi_mode }, 2);
	if (ret < 0) {
		dev_err(dev, "Failed to set DSI mode (%d)\n", ret);
		goto fail;
	}
	/* Set tear ON */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear ON (%d)\n", ret);
		goto fail;
	}
	/* Set tear scanline */
	ret = mipi_dsi_dcs_set_tear_scanline(dsi, 0x380);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear scanline (%d)\n", ret);
		goto fail;
	}
	/* Set pixel format */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, (u8)color_format);
	dev_dbg(dev, "Interface color format set to 0x%x\n", color_format);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}
	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	usleep_range(5000, 7000);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}

	panel->enabled = true;

	return 0;

fail:
	gpiod_set_value_cansleep(panel->reset, 1);

	return ret;
}

static int rm67199_enable(struct rad_panel *panel)
{
	struct mipi_dsi_device *dsi = panel->dsi;
	struct device *dev = &dsi->dev;
	u8 dsi_mode;
	int color_format = color_format_from_dsi_format(dsi->format);
	int ret;

	if (panel->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = rad_panel_push_cmd_list(dsi,
				      &mcs_rm67199[0],
				      ARRAY_SIZE(mcs_rm67199));
	if (ret < 0) {
		dev_err(dev, "Failed to send MCS (%d)\n", ret);
		goto fail;
	}

	/* Select User Command Set table (CMD1) */
	ret = (int)mipi_dsi_generic_write(dsi, (u8[]){ WRMAUCCTR, 0x00 }, 2);
	if (ret < 0)
		goto fail;

	/* Set DSI mode */
	dsi_mode = (dsi->mode_flags & MIPI_DSI_MODE_VIDEO) ? 0x0B : 0x00;
	ret = (int)mipi_dsi_generic_write(dsi, (u8[]){ 0xC2, dsi_mode }, 2);
	if (ret < 0) {
		dev_err(dev, "Failed to set DSI mode (%d)\n", ret);
		goto fail;
	}
	/* Set tear ON */
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear ON (%d)\n", ret);
		goto fail;
	}
	/* Set tear scanline */
	ret = mipi_dsi_dcs_set_tear_scanline(dsi, 0x00);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear scanline (%d)\n", ret);
		goto fail;
	}
	/* Set pixel format */
	ret = mipi_dsi_dcs_set_pixel_format(dsi, (u8)color_format);
	dev_dbg(dev, "Interface color format set to 0x%x\n",
			     color_format);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format (%d)\n", ret);
		goto fail;
	}
	/* Exit sleep mode */
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode (%d)\n", ret);
		goto fail;
	}

	/*
	 * Although, 120ms seems a lot, this is the amount of delay that the
	 * manufacturer suggests it should be used between the sleep-out and
	 * display-on commands
	 */
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display ON (%d)\n", ret);
		goto fail;
	}

	/*
	 * Also, 100ms delay between display-on and backlight enable as per
	 * manufacturer initialization sequence.
	 */
	msleep(100);

	panel->enabled = true;

	return 0;

fail:
	gpiod_set_value_cansleep(panel->reset, 1);

	return ret;
}

int rad_panel_enable(struct drm_panel* panel)
{
	struct rad_panel* rad = to_rad_panel(panel);

	return rad->pdata->enable(rad);
}

int rad_panel_disable(struct drm_panel* panel)
{
	struct rad_panel* rad = to_rad_panel(panel);
	struct mipi_dsi_device* dsi = rad->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	if (!rad->enabled)
		return 0;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	usleep_range(10000, 12000);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display OFF (%d)\n", ret);
		return ret;
	}

	usleep_range(5000, 10000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	rad->enabled = false;

	return 0;
}

static const struct drm_panel_funcs rad_panel_funcs = {
	.prepare = rad_panel_prepare,
	.unprepare = rad_panel_unprepare,
	.enable = rad_panel_enable,
	.disable = rad_panel_disable,
};

static const struct rad_platform_data rad_rm67191 = {
	.enable = &rm67191_enable,
};

static const struct rad_platform_data rad_rm67199 = {
	.enable = &rm67199_enable,
};

static const struct of_device_id rad_of_match[] = {
	{ .compatible = "raydium,rm67191", .data = &rad_rm67191 },
	{ .compatible = "raydium,rm67199", .data = &rad_rm67199 },
	{ /* sentinel */ 0 }
};

int rad_panel_probe(struct platform_device* pdev, u32 video_mode, u8 lanes)
{
	struct mipi_dsi_device *dsi = (struct mipi_dsi_device *)pdev->data;
	struct device *dev = &pdev->dev;
	const struct of_device_id *of_id = of_match_device(rad_of_match, dev);
	struct rad_panel *panel;
	int ret;

	if (!of_id || !of_id->data)
		return -ENODEV;

	panel = devm_kzalloc(&dsi->dev, sizeof(*panel), GFP_KERNEL);
	if (!panel)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, panel);

	panel->dsi = dsi;
	panel->pdata = of_id->data;

	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_EOT_PACKET;
	dsi->mode_flags = MIPI_DSI_MODE_INCREASE_BITCLK;

	switch (video_mode) {
		case DSI_VIDEO_MODE_BURST:
			/* burst mode */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_BURST |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case DSI_VIDEO_MODE_NON_BURST_SYNC_EVENT:
			/* non-burst mode with sync event */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO;
			break;
		case DSI_VIDEO_MODE_NON_BURST_SYNC_PULSE:
			/* non-burst mode with sync pulse */
			dsi->mode_flags |= MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
					   MIPI_DSI_MODE_VIDEO;
			break;
		case DSI_VIDEO_MODE_COMMAND:
			/* command mode */
			dsi->mode_flags |= MIPI_DSI_CLOCK_NON_CONTINUOUS |
					   MIPI_DSI_MODE_VSYNC_FLUSH;
			break;
		default:
			dev_warn(dev, "invalid video mode %d\n", video_mode);
			break;
	}

	dsi->lanes = lanes;

	panel->reset = devm_gpiod_get_optional(dev, "reset", FLAG_ACTIVE_LOW);
	if (IS_ERR(panel->reset)) {
		ret = PTR_ERR(panel->reset);
		dev_err(dev, "Failed to get reset gpio (%d)\n", ret);
		return ret;
	}
	gpiod_set_value_cansleep(panel->reset, 1);


	drm_panel_init(&panel->panel, dev, &rad_panel_funcs);
	dev_set_drvdata(dev, panel);

	drm_panel_add(&panel->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		drm_panel_remove(&panel->panel);
		devm_gpiod_put(dev, panel->reset);
	}

	return ret;
}

int rad_panel_remove(struct platform_device* pdev)
{
	struct mipi_dsi_device *dsi = (struct mipi_dsi_device *)pdev->data;
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret)
		dev_err(dev, "Failed to detach from host (%d)\n", ret);

	drm_panel_remove(&rad->panel);
	devm_gpiod_put(dev, rad->reset);

	return 0;
}

void rad_panel_shutdown(struct platform_device* pdev)
{
	struct mipi_dsi_device *dsi = (struct mipi_dsi_device *)pdev->data;
	struct rad_panel *rad = mipi_dsi_get_drvdata(dsi);

	rad_panel_disable(&rad->panel);
	rad_panel_unprepare(&rad->panel);
}
