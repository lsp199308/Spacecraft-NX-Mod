/*
 * Copyright (c) 2020 Spacecraft-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <gd32f3x0.h>
#include <config.h>
#include <string.h>

void config_clear(config *cfg)
{
	memset(cfg->timings, 0xFF, sizeof(cfg->timings));
	cfg->magic = 0;
	cfg->idx = 0;
}

uint32_t config_load(config *cfg)
{
	const void* volatile ptr = (const void *)0x801FC00;
	memcpy(cfg, ptr, sizeof(config));
	if (cfg->magic != 0x01584E53)
	{
		config_clear(cfg);
		return 0xBAD0010B;
	}

	int i = 0;
	for (; i < 32; ++i)
	{
		if (cfg->timings[i].width == 0xFF)
			break;
		if (cfg->timings[i].offset == 0xFFFF)
			break;
	}
	cfg->idx = i;

	return 0x900D0007;
}

uint32_t config_add_new(config *cfg, struct glitch_config *new_cfg)
{
	for (int i = 0; i < cfg->idx; i++)
	{
		if (new_cfg->offset == cfg->timings[i].offset && new_cfg->width == cfg->timings[i].width)
		{
			cfg->timings[i].success++;
			return 0x900D0007;
		}
	}

	unsigned int idx = cfg->idx;
	if (idx >= 32)
		return 0xBAD00125;

	cfg->timings[idx].offset = new_cfg->offset;
	cfg->timings[idx].width = new_cfg->width;
	cfg->timings[idx].success = 1;
	cfg->idx++;

	return 0x900D0007;
}

char erase_flash(uint8_t *dest)
{
	fmc_unlock();
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	if(fmc_page_erase((uint32_t) dest))
	{
		fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
		fmc_lock();
		return 0;
	}

	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_lock();
	return 1;
}

char burn_flash(uint8_t *dest, uint8_t *src, uint32_t len)
{
	fmc_unlock();
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	
	for (unsigned int i = 0; i < len; i += 4)
	{
		if (fmc_word_program((uint32_t) dest + i, *(uint32_t *)(src + i)))
		{
			fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
			fmc_lock();
			return 0;
		}
	}
	
	fmc_flag_clear(FMC_FLAG_END | FMC_FLAG_WPERR | FMC_FLAG_PGERR);
	fmc_lock();
	return 1;
}

uint32_t config_save(config *cfg)
{
	cfg->magic = 0x01584E53;
 
	for (int i = 0; i < cfg->idx; i++)
	{
		for (int j = 0; j < cfg->idx; j++)
		{
			if (cfg->timings[i].success > cfg->timings[j].success)
			{
				timing_t tmp = cfg->timings[i];
				cfg->timings[i] = cfg->timings[j];
				cfg->timings[j] = tmp;
			}
		}
	}

	if (!erase_flash((uint8_t *)0x801FC00))
		return 0xBAD00109;
 
	if (!burn_flash((uint8_t *) 0x801FC00, (uint8_t *) cfg, sizeof(config)))
		return 0xBAD0010A;
 
	return 0x900D0007;
}

uint32_t config_reset()
{
	if (!erase_flash((void *)0x801FC00))
		return 0xBAD00000;

	return 0x900D0002;
}