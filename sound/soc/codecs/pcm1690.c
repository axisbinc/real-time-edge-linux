// SPDX-License-Identifier: GPL-2.0
// Audio driver for pcm1690
// Copyright (C) 2018 Bootlin
// Mylène Josserand <mylene.josserand@bootlin.com>

#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>

#include "pcm1690.h"

#define pcm1690_MUTE_CONTROL	0x10
#define pcm1690_FMT_CONTROL	0x11
#define pcm1690_SOFT_MUTE	0x14
#define pcm1690_DAC_VOL_LEFT	0x18
#define pcm1690_DAC_VOL_RIGHT	0x19

#define pcm1690_FMT_MASK	0x07
#define pcm1690_MUTE_MASK	0x03
#define pcm1690_MUTE_SRET	0x06

struct pcm1690_private {
	struct regmap *regmap;
	unsigned int format;
	unsigned int rate;
	struct gpio_desc *reset;
	struct work_struct work;
	struct device *dev;
};

static const struct reg_default pcm1690_reg_defaults[] = {
	{ pcm1690_FMT_CONTROL, 0x00 },
	{ pcm1690_SOFT_MUTE, 0x00 },
	{ pcm1690_DAC_VOL_LEFT, 0xff },
	{ pcm1690_DAC_VOL_RIGHT, 0xff },
};

static bool pcm1690_accessible_reg(struct device *dev, unsigned int reg)
{
	return reg >= pcm1690_MUTE_CONTROL && reg <= pcm1690_DAC_VOL_RIGHT;
}

static bool pcm1690_writeable_reg(struct device *dev, unsigned int reg)
{
	return pcm1690_accessible_reg(dev, reg);
}

static int pcm1690_set_dai_fmt(struct snd_soc_dai *codec_dai,
			       unsigned int format)
{
	struct snd_soc_component *component = codec_dai->component;
	struct pcm1690_private *priv = snd_soc_component_get_drvdata(component);

	priv->format = format;

	return 0;
}

static int pcm1690_mute(struct snd_soc_dai *codec_dai, int mute, int direction)
{
	struct snd_soc_component *component = codec_dai->component;
	struct pcm1690_private *priv = snd_soc_component_get_drvdata(component);

	return regmap_update_bits(priv->regmap, pcm1690_SOFT_MUTE,
				  pcm1690_MUTE_MASK,
				  mute ? 0 : pcm1690_MUTE_MASK);
}

static int pcm1690_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *codec_dai)
{
	struct snd_soc_component *component = codec_dai->component;
	struct pcm1690_private *priv = snd_soc_component_get_drvdata(component);
	int val = 0, ret;

	priv->rate = params_rate(params);

	switch (priv->format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		switch (params_width(params)) {
		case 24:
			val = 2;
			break;
		case 16:
			val = 3;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_I2S:
		switch (params_width(params)) {
		case 16:
		case 24:
		case 32:
			val = 0;
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		switch (params_width(params)) {
		case 16:
		case 24:
		case 32:
			val = 1;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		dev_err(component->dev, "Invalid DAI format\n");
		return -EINVAL;
	}

	ret = regmap_update_bits(priv->regmap, pcm1690_FMT_CONTROL,
				 pcm1690_FMT_MASK, val);
	if (ret < 0)
		return ret;

	return 0;
}

static void pcm1690_work_queue(struct work_struct *work)
{
	struct pcm1690_private *priv = container_of(work,
						    struct pcm1690_private,
						    work);

	/* Perform a software reset to remove codec from desynchronized state */
	if (regmap_update_bits(priv->regmap, pcm1690_MUTE_CONTROL,
			       0x3 << pcm1690_MUTE_SRET, 0) < 0)
		dev_err(priv->dev, "Error while setting SRET");
}

static int pcm1690_trigger(struct snd_pcm_substream *substream, int cmd,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct pcm1690_private *priv = snd_soc_component_get_drvdata(component);
	int ret = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		schedule_work(&priv->work);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static const struct snd_soc_dai_ops pcm1690_dai_ops = {
	.set_fmt	= pcm1690_set_dai_fmt,
	.hw_params	= pcm1690_hw_params,
	.mute_stream	= pcm1690_mute,
	.trigger	= pcm1690_trigger,
	.no_capture_mute = 1,
};

static const DECLARE_TLV_DB_SCALE(pcm1690_dac_tlv, -12000, 50, 1);

static const struct snd_kcontrol_new pcm1690_controls[] = {
	SOC_DOUBLE_R_RANGE_TLV("DAC Playback Volume", pcm1690_DAC_VOL_LEFT,
			       pcm1690_DAC_VOL_RIGHT, 0, 0xf, 0xff, 0,
			       pcm1690_dac_tlv),
};

static const struct snd_soc_dapm_widget pcm1690_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("IOUTL+"),
	SND_SOC_DAPM_OUTPUT("IOUTL-"),
	SND_SOC_DAPM_OUTPUT("IOUTR+"),
	SND_SOC_DAPM_OUTPUT("IOUTR-"),
};

static const struct snd_soc_dapm_route pcm1690_dapm_routes[] = {
	{ "IOUTL+", NULL, "Playback" },
	{ "IOUTL-", NULL, "Playback" },
	{ "IOUTR+", NULL, "Playback" },
	{ "IOUTR-", NULL, "Playback" },
};

static struct snd_soc_dai_driver pcm1690_dai = {
	.name = "pcm1690-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min = 10000,
		.rate_max = 200000,
		.formats = PCM1690_FORMATS,
	},
	.ops = &pcm1690_dai_ops,
};

const struct regmap_config pcm1690_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= pcm1690_DAC_VOL_RIGHT,
	.reg_defaults		= pcm1690_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(pcm1690_reg_defaults),
	.writeable_reg		= pcm1690_writeable_reg,
	.readable_reg		= pcm1690_accessible_reg,
};
EXPORT_SYMBOL_GPL(pcm1690_regmap_config);

static const struct snd_soc_component_driver soc_component_dev_pcm1690 = {
	.controls		= pcm1690_controls,
	.num_controls		= ARRAY_SIZE(pcm1690_controls),
	.dapm_widgets		= pcm1690_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(pcm1690_dapm_widgets),
	.dapm_routes		= pcm1690_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(pcm1690_dapm_routes),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

int pcm1690_common_init(struct device *dev, struct regmap *regmap)
{
	struct pcm1690_private *pcm1690;

	pcm1690 = devm_kzalloc(dev, sizeof(struct pcm1690_private),
			       GFP_KERNEL);
	if (!pcm1690)
		return -ENOMEM;

	pcm1690->regmap = regmap;
	pcm1690->dev = dev;
	dev_set_drvdata(dev, pcm1690);

	pcm1690->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(pcm1690->reset))
		return PTR_ERR(pcm1690->reset);

	gpiod_set_value_cansleep(pcm1690->reset, 0);
	msleep(300);

	INIT_WORK(&pcm1690->work, pcm1690_work_queue);

	return devm_snd_soc_register_component(dev, &soc_component_dev_pcm1690,
					       &pcm1690_dai, 1);
}
EXPORT_SYMBOL_GPL(pcm1690_common_init);

void pcm1690_common_exit(struct device *dev)
{
	struct pcm1690_private *priv = dev_get_drvdata(dev);

	flush_work(&priv->work);
}
EXPORT_SYMBOL_GPL(pcm1690_common_exit);

static int pcm1690_i2c_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct regmap *regmap;
	int ret;

	dev_info(dev, "pcm1690: probing codec\n");
	regmap = devm_regmap_init_i2c(client, &pcm1690_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err(dev, "Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	dev_info(dev, "pcm1690: probe succeeded\n");
	return pcm1690_common_init(dev, regmap);
}

static void pcm1690_i2c_remove(struct i2c_client *client)
{
	pcm1690_common_exit(&client->dev);
}

#ifdef CONFIG_OF
static const struct of_device_id pcm1690_of_match[] = {
	{ .compatible = "ti,pcm1690", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm1690_of_match);
#endif

static const struct i2c_device_id pcm1690_i2c_ids[] = {
	{ "pcm1690", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, pcm1690_i2c_ids);

static struct i2c_driver pcm1690_i2c_driver = {
	.driver = {
		.name	= "pcm1690",
		.of_match_table = of_match_ptr(pcm1690_of_match),
	},
	.id_table	= pcm1690_i2c_ids,
	.probe_new	= pcm1690_i2c_probe,
	.remove	= pcm1690_i2c_remove,
};

module_i2c_driver(pcm1690_i2c_driver);

MODULE_DESCRIPTION("ASoC pcm1690 driver");
MODULE_AUTHOR("Mylène Josserand <mylene.josserand@free-electrons.com>");
MODULE_LICENSE("GPL");
