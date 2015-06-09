// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPTables.h"
#include "Core/DSP/LabelMap.h"

LabelMap::LabelMap()
{

}

void LabelMap::RegisterDefaults()
{
	for (int i = 0; i < 0x24; i++)
	{
		if (regnames[i].name)
			RegisterLabel(regnames[i].name, regnames[i].addr);
	}
	for (int i = 0; i < (int)pdlabels_size; i++)
	{
		if (pdlabels[i].name)
			RegisterLabel(pdlabels[i].name, pdlabels[i].addr);
	}
}

void LabelMap::RegisterLabel(const std::string &label, u16 lval, LabelType type)
{
	u16 old_value;
	if (GetLabelValue(label, &old_value) && old_value != lval)
	{
		printf("WARNING: Redefined label %s to %04x - old value %04x\n",
			   label.c_str(), lval, old_value);
		DeleteLabel(label);
	}
	labels.emplace_back(label, lval, type);
}

void LabelMap::DeleteLabel(const std::string &label)
{
	for (std::vector<label_t>::iterator iter = labels.begin();
		iter != labels.end(); ++iter)
	{
		if (!label.compare(iter->name))
		{
			labels.erase(iter);
			return;
		}
	}
}

bool LabelMap::GetLabelValue(const std::string &name, u16 *value, LabelType type) const
{
	for (auto& label : labels)
	{
		if (!name.compare(label.name))
		{
			if (type & label.type)
			{
				*value = label.addr;
				return true;
			}
			else
			{
				printf("WARNING: Wrong label type requested. %s\n", name.c_str());
			}
		}
	}
	return false;
}

void LabelMap::Clear()
{
	labels.clear();
}
