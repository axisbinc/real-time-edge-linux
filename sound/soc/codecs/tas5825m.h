/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _TAS5825M_H
#define _TAS5825M_H

#include <sound/soc.h>
#include <linux/regmap.h>

// Existing TAS5825M definitions...
#define TAS5825M_REG_POWER_CTRL    0x02
#define TAS5825M_PWR_MASK          0x03
#define TAS5825M_PWR_ACTIVE        0x00
#define TAS5825M_PWR_STANDBY       0x02

// ADD THESE FUNCTION DECLARATIONS:
int tas5825m_trigger_single(struct device *dev, int cmd);
int tas5825m_hw_params_single(struct regmap *regmap, struct snd_pcm_hw_params *params);

#endif /* _TAS5825M_H */
