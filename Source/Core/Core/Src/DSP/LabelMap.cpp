// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "LabelMap.h"
#include "DSPTables.h"

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
	labels.push_back(label_t(label, lval, type));
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

bool LabelMap::GetLabelValue(const std::string &label, u16 *value, LabelType type) const
{
	for (u32 i = 0; i < labels.size(); i++)
	{
		if (!label.compare(labels[i].name))
		{
			if (type & labels[i].type) {
				*value = labels[i].addr;
				return true;
			} else {
				printf("WARNING: Wrong label type requested. %s\n", label.c_str());
			}
		}
	}
	return false;
}

void LabelMap::Clear()
{
	labels.clear();
}
