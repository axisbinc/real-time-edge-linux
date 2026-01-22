// SPDX-License-Identifier: GPL-2.0
/*
 * Dummy Codec Driver with Device Tree Support
 * Based on snd-soc-dummy but with OF matching for device tree instantiation
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <sound/soc.h>

static const struct snd_soc_dapm_widget dummy_codec_dapm_widgets[] = {
    SND_SOC_DAPM_OUTPUT("DUMMY_OUT"),
    SND_SOC_DAPM_INPUT("DUMMY_IN"),
};

static const struct snd_soc_dapm_route dummy_codec_dapm_routes[] = {
    { "DUMMY_OUT", NULL, "Playback" },
    { "Capture", NULL, "DUMMY_IN" },
};

static struct snd_soc_dai_driver dummy_codec_dai = {
    .name = "dummy-codec-hifi",
    .playback = {
        .stream_name = "Playback",
        .channels_min = 1,
        .channels_max = 384,
        .rates = SNDRV_PCM_RATE_CONTINUOUS,
        .formats = SNDRV_PCM_FMTBIT_S16_LE |
                  SNDRV_PCM_FMTBIT_S24_LE |
                  SNDRV_PCM_FMTBIT_S32_LE,
    },
    .capture = {
        .stream_name = "Capture",
        .channels_min = 1,
        .channels_max = 384,
        .rates = SNDRV_PCM_RATE_CONTINUOUS,
        .formats = SNDRV_PCM_FMTBIT_S16_LE |
                  SNDRV_PCM_FMTBIT_S24_LE |
                  SNDRV_PCM_FMTBIT_S32_LE,
    },
};

static const struct snd_soc_component_driver soc_component_dev_dummy_codec = {
    .dapm_widgets = dummy_codec_dapm_widgets,
    .num_dapm_widgets = ARRAY_SIZE(dummy_codec_dapm_widgets),
    .dapm_routes = dummy_codec_dapm_routes,
    .num_dapm_routes = ARRAY_SIZE(dummy_codec_dapm_routes),
    .idle_bias_on = 1,
    .use_pmdown_time = 1,
    .endianness = 1,
};

static int dummy_codec_probe(struct platform_device* pdev)
{
    return devm_snd_soc_register_component(&pdev->dev,
        &soc_component_dev_dummy_codec,
        &dummy_codec_dai, 1);
}

static const struct of_device_id dummy_codec_of_match[] = {
    {.compatible = "linux,dummy-codec", },
    {},
};
MODULE_DEVICE_TABLE(of, dummy_codec_of_match);

static struct platform_driver dummy_codec_driver = {
    .probe = dummy_codec_probe,
    .driver = {
        .name = "dummy-codec",
        .of_match_table = dummy_codec_of_match,
    },
};

module_platform_driver(dummy_codec_driver);

MODULE_AUTHOR("Axis Communications");
MODULE_DESCRIPTION("Dummy Codec Driver with Device Tree Support");
MODULE_LICENSE("GPL v2");
