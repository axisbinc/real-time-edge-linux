// SPDX-License-Identifier: GPL-2.0
// Definitions for PCM1690 audio driver
// Copyright (C) 2018 Bootlin
// Mylène Josserand <mylene.josserand@bootlin.com>

#ifndef __PCM1690_H__
#define __PCM1690_H__

#define PCM1690_FORMATS (SNDRV_PCM_FMTBIT_S32_LE | SNDRV_PCM_FMTBIT_S24_LE | \
			 SNDRV_PCM_FMTBIT_S16_LE)

extern const struct regmap_config pcm1690_regmap_config;

int pcm1690_common_init(struct device *dev, struct regmap *regmap);
void pcm1690_common_exit(struct device *dev);

#endif