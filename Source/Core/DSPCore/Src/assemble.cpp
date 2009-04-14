/*====================================================================

$Id: assemble.cpp,v 1.3 2008-11-11 01:04:26 wntrmute Exp $

project:      GameCube DSP Tool (gcdsp)
mail:		  duddie@walla.com

Copyright (c) 2005 Duddie

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Revision 1.4  2008/10/04 10:30:00  Hermes
added function to export the code to .h file
added support for '/ *' '* /' and '//' for comentaries
added some sintax detection when use registers

$Log: not supported by cvs2svn $
Revision 1.2  2005/09/14 02:19:29  wntrmute
added header guards
use standard main function

Revision 1.1  2005/08/24 22:13:34  wntrmute
Initial import


====================================================================*/

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include <map>

#include "Common.h"
#include "FileUtil.h"
#include "DSPInterpreter.h"
#include "DSPTables.h"
#include "disassemble.h"
#include "assemble.h"

static const char *err_string[] =
{
	"",
	"Unknown Error",
	"Unknown opcode",
	"Not enough parameters",
	"Too many parameters",
	"Wrong parameter",
	"Expected parameter of type 'string'",
	"Expected parameter of type 'value'",
	"Expected parameter of type 'register'",
	"Expected parameter of type 'memory pointer'",
	"Expected parameter of type 'immediate'",
	"Incorrect binary value",
	"Incorrect hexadecimal value",
	"Incorrect decimal value",
	"Label already exists",
	"Label not defined",
	"No matching brackets",
	"This opcode cannot be extended",
	"Given extending params for non extensible opcode",
	"Wrong parameter: must be accumulator register",
	"Wrong parameter: must be mid accumulator register",
	"Invalid register",
	"Number out of range"
};

DSPAssembler::DSPAssembler(const AssemblerSettings &settings) 
:   current_param(0),
	cur_addr(0),
	cur_pass(0),
	settings_(settings) 
{
	gdg_buffer = NULL;

}

DSPAssembler::~DSPAssembler()
{
}

bool DSPAssembler::Assemble(const char *text, std::vector<u16> *code, std::vector<int> *line_numbers)
{
	if (line_numbers)
		line_numbers->clear();
	const char *fname = "tmp.asm";
	if (!File::WriteStringToFile(true, text, fname))
		return false;
	InitPass(1);
	if (!AssembleFile(fname, 1))
		return false;
	InitPass(2);
	if (!AssembleFile(fname, 2))
		return false;

	code->resize(gdg_buffer_size);
	for (int i = 0; i < gdg_buffer_size; i++) {
		(*code)[i] = *(u16 *)(gdg_buffer + i * 2);
	}

	last_error_str = "(no errors)";
	last_error = ERR_OK;
	return true;
}

void DSPAssembler::parse_error(err_t err_code, const char *extra_info)
{
	failed = true;
	char error_buffer[1024];
	char *buf_ptr = error_buffer;
	buf_ptr += sprintf(buf_ptr, "%i : %s\n", code_line, cur_line);
	if (!extra_info)
		extra_info = "-";
	if (fsrc)
		fclose(fsrc);
	else
		buf_ptr += sprintf(buf_ptr, "ERROR: %s : %s\n", err_string[err_code], extra_info);

	if (current_param == 0)
		buf_ptr += sprintf(buf_ptr, "ERROR: %s Line: %d : %s\n", err_string[err_code], code_line, extra_info);
	else 
		buf_ptr += sprintf(buf_ptr, "ERROR: %s Line: %d Param: %d : %s\n",
						   err_string[err_code], code_line, current_param, extra_info);
	last_error_str = error_buffer;
	last_error = err_code;
}

char *skip_spaces(char *ptr)
{
	while (*ptr == ' ')
		ptr++;
	return ptr;
}

const char *skip_spaces(const char *ptr)
{
	while (*ptr == ' ')
		ptr++;
	return ptr;
}

void DSPAssembler::gd_ass_register_label(const char *label, u16 lval)
{
	labels.push_back(label_t(label, lval));
}

void DSPAssembler::gd_ass_clear_labels()
{
	labels.clear();
}

// Parse a standalone value - it can be a number in one of several formats or a label.
s32 DSPAssembler::strtoval(const char *str)
{
	bool negative = false;
	s32 val = 0;
	const char *ptr = str;

	if (ptr[0] == '#')
	{
		ptr++;
		negative = true;  // Wow! Double # (needed one to get in here) negates???
	}
	if (ptr[0] == '-')
	{
		ptr++;
		negative = true;
	}
	if (ptr[0] == '0')
	{
		if (ptr[1] >= '0' && ptr[1] <= '9')
		{
			for (int i = 0; ptr[i] != 0; i++)
			{
				val *= 10;
				if (ptr[i] >= '0' && ptr[i] <= '9')
					val += ptr[i] - '0';
				else
					parse_error(ERR_INCORRECT_DEC, str);
			}
		}
		else
		{
			switch (ptr[1])
			{
			case 'X': // hex
				for (int i = 2 ; ptr[i] != 0 ; i++)
				{
					val <<= 4;
					if (ptr[i] >= 'a' && ptr[i] <= 'f')
						val += (ptr[i]-'a'+10);
					else if (ptr[i] >= 'A' && ptr[i] <= 'F')
						val += (ptr[i]-'A'+10);
					else if (ptr[i] >= '0' && ptr[i] <= '9')
						val += (ptr[i] - '0');
					else
						parse_error(ERR_INCORRECT_HEX, str);
				}
				break;
			case '\'': // binary
				for (int i = 2; ptr[i] != 0; i++)
				{
					val *=2;
					if(ptr[i] >= '0' && ptr[i] <= '1')
						val += ptr[i] - '0';
					else
						parse_error(ERR_INCORRECT_BIN, str);
				}
				break;
			default:
				// value is 0 or error
				val = 0;
				break;
			}
		}
	}
	else
	{
		// Symbol starts with a digit - it's a dec number.
		if (ptr[0] >= '0' && ptr[0] <= '9')
		{
			for (int i = 0; ptr[i] != 0; i++)
			{
				val *= 10;
				if (ptr[i] >= '0' && ptr[i] <= '9')
					val += ptr[i] - '0';
				else
					parse_error(ERR_INCORRECT_DEC, str);
			}
		}
		else  // Everything else is a label.
		{
			// Lookup label
			for (int i = 0; i < labels.size(); i++)
			{
				if (strcmp(labels[i].label.c_str(), ptr) == 0)
					return labels[i].addr;
			}
			if (cur_pass == 2)
				parse_error(ERR_UNKNOWN_LABEL, str);
		}
	}
	if (negative)
		return -val;
	return val;
}


// Modifies both src and dst!
// What does it do, really??
char *DSPAssembler::find_brackets(char *src, char *dst)
{
	s32 len = (s32) strlen(src);
	s32 first = -1;
	s32 count = 0;
	s32 i, j;
	j = 0;
	for (i = 0 ; i < len ; i++)
	{
		if (src[i] == '(')
		{
			if (first < 0)
			{
				count = 1;
				src[i] = 0x0;
				first = i;
			}
			else
			{
				count++;
				dst[j++] = src[i];
			}
		}
		else if (src[i] == ')')
		{
			if (--count == 0)
			{
				dst[j] = 0;
				return &src[i+1];
			}
			else
			{
				dst[j++] = src[i];
			}
		}
		else
		{
			if (first >= 0)
				dst[j++] = src[i];
		}
	}
	if (count)
		parse_error(ERR_NO_MATCHING_BRACKETS);
	return NULL;
}

// Bizarre in-place expression evaluator.
u32 DSPAssembler::parse_exp(const char *ptr)
{
	char *pbuf;
	s32 val = 0;

	char *d_buffer = (char *)malloc(1024);
	char *s_buffer = (char *)malloc(1024);
	strcpy(s_buffer, ptr);

	while ((pbuf = find_brackets(s_buffer, d_buffer)) != NULL)
	{
		val = parse_exp(d_buffer);
		sprintf(d_buffer, "%s%d%s", s_buffer, val, pbuf);
		strcpy(s_buffer, d_buffer);
	}

	int j = 0;
	for (int i = 0; i < ((s32)strlen(s_buffer) + 1) ; i++)
	{
		char c = s_buffer[i];
		if (c != ' ')
			d_buffer[j++] = c;
	}

	for (int i = 0; i < ((s32)strlen(d_buffer) + 1) ; i++)
	{
		char c = d_buffer[i];
		if (c == '-')
		{
			if (i == 0)
				c = '#';
			else
			{
				switch (d_buffer[i - 1])
				{
				case '/':
				case '%':
				case '*':
					c = '#';
				}
			}
		}
		d_buffer[i] = c;
	}
	while ((pbuf = strstr(d_buffer, "+")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) + parse_exp(pbuf+1);
		sprintf(d_buffer, "%d", val);
	}

	while ((pbuf = strstr(d_buffer, "-")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) - parse_exp(pbuf+1);
		if (val < 0)
		{
			val = 0x10000 + (val & 0xffff); // ATTENTION: avoid a terrible bug!!! number cannot write with '-' in sprintf
			fprintf(stderr, "WARNING: Number Underflow at Line: %d \n", code_line);
		}
		sprintf(d_buffer, "%d", val);
	}

	while ((pbuf = strstr(d_buffer, "*")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) * parse_exp(pbuf+1);
		sprintf(d_buffer, "%d", val);
	}

	while ((pbuf = strstr(d_buffer, "/")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) / parse_exp(pbuf+1);
		sprintf(d_buffer, "%d", val);
	}

	while ((pbuf = strstr(d_buffer, "|")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) | parse_exp(pbuf+1);
		sprintf(d_buffer, "%d", val);
	}

	while ((pbuf = strstr(d_buffer, "&")) != NULL)
	{
		*pbuf = 0x0;
		val = parse_exp(d_buffer) & parse_exp(pbuf+1);
		sprintf(d_buffer, "%d", val);
	}

	val = strtoval(d_buffer);
	free(d_buffer);
	free(s_buffer);
	return val;
}

u32 DSPAssembler::parse_exp_f(const char *ptr)
{
	return parse_exp(ptr);
}

// Destroys parstr
u32 DSPAssembler::get_params(char *parstr, param_t *par)
{
	u32 count = 0;
	char *tmpstr = skip_spaces(parstr);
	tmpstr = strtok(tmpstr, ",\x00");
	for (int i = 0; i < 10; i++)
	{
		if (tmpstr == NULL)
			break;
		tmpstr = skip_spaces(tmpstr);
		if (strlen(tmpstr) == 0)
			break;
		if (tmpstr)
			count++;
		else
			break;

		par[i].type = P_NONE;
		switch (tmpstr[0])
		{
		case '"':
			par[i].str = strtok(tmpstr, "\"");
			par[i].type = P_STR;
			break;
		case '#':
			par[i].val = parse_exp_f(tmpstr + 1);
			par[i].type = P_IMM;
			break;
		case '@':
			if (tmpstr[1] == '$')
			{
				par[i].val = parse_exp_f(tmpstr + 2);
				par[i].type = P_PRG;
			}
			else
			{
				par[i].val = parse_exp_f(tmpstr + 1);
				par[i].type = P_MEM;
			}
			break;
		case '$':
			par[i].val = parse_exp_f(tmpstr + 1);
			par[i].type = P_REG;
			break;
		default:
			par[i].val = parse_exp_f(tmpstr);
			par[i].type = P_VAL;
			break;
		}
		tmpstr = strtok(NULL, ",\x00");
	}
	return count;
}

const opc_t *DSPAssembler::find_opcode(const char *opcode, u32 par_count, const opc_t * const opcod, int opcod_size)
{
	if (opcode[0] == 'C' && opcode[1] == 'W')
		return &cw;

	AliasMap::const_iterator alias_iter = aliases.find(opcode);
	if (alias_iter != aliases.end())
		opcode = alias_iter->second.c_str();
	for (int i = 0; i < opcod_size; i++)
	{
		const opc_t *opc = &opcod[i];
		if (strcmp(opc->name, opcode) == 0)
		{
			if (par_count < opc->param_count)
			{
				parse_error(ERR_NOT_ENOUGH_PARAMETERS);
			}
			if (par_count > opc->param_count)
			{
				parse_error(ERR_TOO_MANY_PARAMETERS);
			}
			return opc;
		}
	}
	parse_error(ERR_UNKNOWN_OPCODE);
	return NULL;
}

// weird...
u16 get_mask_shifted_down(u16 mask)
{
	while (!(mask & 1))
		mask >>= 1;
	return mask;
}

bool DSPAssembler::verify_params(const opc_t *opc, param_t *par, int count, bool ext)
{
	for (int i = 0; i < count; i++)
	{
		const int current_param = i + 1;  // just for display.
		if (opc->params[i].type != par[i].type || (par[i].type & P_REG))
		{
			if ((opc->params[i].type & P_REG) && (par[i].type & P_REG))
			{
				// Just a temp. Should be replaced with more purposeful vars.
				int value;

				// modified by Hermes: test the register range
				switch ((unsigned)opc->params[i].type)
				{ 
				case P_REG18:
				case P_REG19:
				case P_REG1A:
					value = (opc->params[i].type >> 8) & 31;
					if ((int)par[i].val < value ||
						(int)par[i].val > value + get_mask_shifted_down(opc->params[i].mask))
					{
						if (ext) fprintf(stderr, "(ext) ");
						fprintf(stderr, "%s   (param %i)", cur_line.c_str(), current_param);
						parse_error(ERR_INVALID_REGISTER);
					}
					break;
				case P_PRG:
					if ((int)par[i].val < 0 || (int)par[i].val > 0x3)
					{
						if (ext) fprintf(stderr, "(ext) ");
						fprintf(stderr, "%s   (param %i)", cur_line.c_str(), current_param);
						parse_error(ERR_INVALID_REGISTER);
					}
					break;
				case P_ACC:
					if ((int)par[i].val < 0x20 || (int)par[i].val > 0x21)
					{
						if (ext) fprintf(stderr, "(ext) ");
						if (par[i].val >= 0x1e && par[i].val <= 0x1f)
							fprintf(stderr, "WARNING: $ACM%d register used instead of $ACC%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param, ext);
						else if (par[i].val >= 0x1c && par[i].val <= 0x1d)
							fprintf(stderr, "WARNING: $ACL%d register used instead of $ACC%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param);
						else
							parse_error(ERR_WRONG_PARAMETER_ACC);
					}
					break;
				case P_ACCM:
					if ((int)par[i].val < 0x1e || (int)par[i].val > 0x1f)
					{
						if (ext) fprintf(stderr, "(ext) ");
						if (par[i].val >= 0x1c && par[i].val <= 0x1d)
							fprintf(stderr, "WARNING: $ACL%d register used instead of $ACM%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param);
						else if (par[i].val >= 0x20 && par[i].val <= 0x21)
							fprintf(stderr, "WARNING: $ACC%d register used instead of $ACM%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param);
						else
							parse_error(ERR_WRONG_PARAMETER_ACC);
					}
					break;

				case P_ACCL:
					if ((int)par[i].val < 0x1c || (int)par[i].val > 0x1d)
					{
						if (ext) fprintf(stderr, "(ext) ");
						if (par[i].val >= 0x1e && par[i].val <= 0x1f)
						{
							fprintf(stderr, "%s", cur_line.c_str());
							fprintf(stderr, "WARNING: $ACM%d register used instead of $ACL%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param);
						}
						else if (par[i].val >= 0x20 && par[i].val <= 0x21) {
							fprintf(stderr, "%s", cur_line.c_str());
							fprintf(stderr, "WARNING: $ACC%d register used instead of $ACL%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), code_line, current_param);
						}
						else
							parse_error(ERR_WRONG_PARAMETER_ACC);
					}
					break;
/*				case P_ACCM_D: //P_ACC_MID:
					if ((int)par[i].val < 0x1e || (int)par[i].val > 0x1f)
					{
						parse_error(ERR_WRONG_PARAMETER_MID_ACC);
					}
					break;*/
				}
				continue;
			}

			switch (par[i].type & (P_REG | P_VAL | P_MEM | P_IMM))
			{
			case P_REG:
				if (ext) fprintf(stderr, "(ext) ");
				parse_error(ERR_EXPECTED_PARAM_REG);
				break;
			case P_MEM:
				if (ext) fprintf(stderr, "(ext) ");
				parse_error(ERR_EXPECTED_PARAM_MEM);
				break;
			case P_VAL:
				if (ext) fprintf(stderr, "(ext) ");
				parse_error(ERR_EXPECTED_PARAM_VAL);
				break;
			case P_IMM:
				if (ext) fprintf(stderr, "(ext) ");
				parse_error(ERR_EXPECTED_PARAM_IMM);
				break;
			}
			parse_error(ERR_WRONG_PARAMETER);
			break;
		}
		else if ((opc->params[i].type & 3) != 0 && (par[i].type & 3) != 0)
		{
			// modified by Hermes: test NUMBER range
			int value = get_mask_shifted_down(opc->params[i].mask);
			unsigned int valueu = 0xffff & ~(value >> 1);
			if ((int)par[i].val < 0)
			{
				if (value == 7) // value 7 por sbclr/sbset
				{
					fprintf(stderr,"Value must be from 0x0 to 0x%x\n", value);
					parse_error(ERR_OUT_RANGE_NUMBER);
				}
				else if (opc->params[i].type == P_MEM)
				{
					if (value < 256)
						fprintf(stderr, "Address value must be from 0x%x to 0x%x\n",valueu, (value>>1));
					else
						fprintf(stderr, "Address value must be from 0x0 to 0x%x\n", value);

					parse_error(ERR_OUT_RANGE_NUMBER);
				}
				else if ((int)par[i].val < -((value >> 1) + 1))
				{
					if (value < 128)
						fprintf(stderr, "Value must be from -0x%x to 0x%x, is %i\n",
						        (value >> 1) + 1, value >> 1, par[i].val);
					else
						fprintf(stderr, "Value must be from -0x%x to 0x%x or 0x0 to 0x%x, is %i\n",
						        (value >> 1) + 1, value >> 1, value, par[i].val);

					parse_error(ERR_OUT_RANGE_NUMBER);
				}
			}
			else
			{
				if (value == 7) // value 7 por sbclr/sbset
				{
					if (par[i].val > (unsigned)value)
					{
						fprintf(stderr,"Value must be from 0x%x to 0x%x, is %i\n",valueu, value, par[i].val);
						parse_error(ERR_OUT_RANGE_NUMBER);
					}
				}
				else if (opc->params[i].type == P_MEM)
				{
					if (value < 256)
						value >>= 1; // addressing 8 bit with sign
					if (par[i].val > (unsigned)value &&
						(par[i].val < valueu || par[i].val > (unsigned)0xffff))
					{
						if (value < 256)
							fprintf(stderr,"Address value must be from 0x%x to 0x%x, is %04x\n", valueu, value, par[i].val);
						else
							fprintf(stderr,"Address value must be minor of 0x%x\n", value+1);
						parse_error(ERR_OUT_RANGE_NUMBER);
					}
				}
				else
				{
					if (value < 128)
						value >>= 1; // special case ASL/ASR/LSL/LSR
					if (par[i].val > (unsigned)value)
					{
						if (value < 64)
							fprintf(stderr,"Value must be from -0x%x to 0x%x, is %i\n", (value + 1), value, par[i].val);
						else
							fprintf(stderr,"Value must be minor of 0x%x, is %i\n", value + 1, par[i].val);
						parse_error(ERR_OUT_RANGE_NUMBER);
					}
				}
			}
			continue;
		}
	}
	current_param = 0;
	return true;
}


// Merge opcode with params.
void DSPAssembler::build_code(const opc_t *opc, param_t *par, u32 par_count, u16 *outbuf)
{
	outbuf[cur_addr] |= opc->opcode;
	for (u32 i = 0; i < par_count; i++)
	{
		// Ignore the "reverse" parameters since they are implicit.
		if (opc->params[i].type != P_ACC_D && opc->params[i].type != P_ACCM_D)
		{
			u16 t16 = outbuf[cur_addr + opc->params[i].loc];
			u16 v16 = par[i].val;
			if (opc->params[i].lshift > 0)
				v16 <<= opc->params[i].lshift;
			else
				v16 >>= -opc->params[i].lshift;
			v16 &= opc->params[i].mask;
			outbuf[cur_addr + opc->params[i].loc] = t16 | v16;
		}
	}
}

void DSPAssembler::InitPass(int pass)
{
	failed = false;
	if (pass == 1)
	{
		// Reset label table. Pre-populate with hw addresses and registers.
		gd_ass_clear_labels();
		for (int i = 0; i < 0x24; i++)
		{
			gd_ass_register_label(regnames[i].name, regnames[i].addr);
		}
		for (int i = 0; i < (int)pdlabels_size; i++)
		{
			gd_ass_register_label(pdlabels[i].name, pdlabels[i].addr);
		}
		aliases.clear();
		aliases["S15"] = "SET15";
		aliases["S16"] = "SET16";
		aliases["S40"] = "SET40";
	}
	cur_addr = 0;
	cur_segment = SEGMENT_CODE;
	segment_addr[SEGMENT_CODE] = 0;
	segment_addr[SEGMENT_DATA] = 0;
	segment_addr[SEGMENT_OVERLAY] = 0;
}

bool DSPAssembler::AssembleFile(const char *fname, int pass)
{
    int disable_text = 0; // modified by Hermes

	fsrc = fopen(fname, "r");
	if (fsrc == NULL)
	{
		fprintf(stderr, "Cannot open %s file\n", fname);
		return false;
	}

	fseek(fsrc, 0, SEEK_SET);

	printf("Pass %d\n", pass);
	code_line = 0;
	cur_pass = pass;

#define LINEBUF_SIZE 1024
	char linebuffer[LINEBUF_SIZE];
	while (!feof(fsrc) && !failed)
	{
		int opcode_size = 0;
		memset(linebuffer, 0, LINEBUF_SIZE);
		if (!fgets(linebuffer, LINEBUF_SIZE, fsrc))
			break;
		cur_line = linebuffer;
		//printf("A: %s", linebuffer);
		code_line++;

		param_t params[10] = {0};
		param_t params_ext[10] = {0};

		for (int i = 0; i < LINEBUF_SIZE; i++)
		{
			char c = linebuffer[i];
			// This stuff handles /**/ and // comments.
			// modified by Hermes : added // and /* */ for long comentaries 
			if (c == '/')
			{
				if (i < 1023)
				{
					if (linebuffer[i+1] == '/')
						c = 0x00;
					else if (linebuffer[i+1] == '*') 
					{
						// toggle comment mode.
						disable_text = !disable_text;
					}
				}
			}
			else if (c == '*')
			{
				if (i < 1023 && linebuffer[i+1] == '/' && disable_text)
				{
					disable_text = 0;
					c = 32;
					linebuffer[i + 1] = 32;
				}
			}

			// turn text into spaces if disable_text is on (in a comment).
			if (disable_text && ((unsigned char)c) > 32) c = 32;

			if (c == 0x0a || c == 0x0d || c == ';')
				c = 0x00;
			if (c == 0x09)				// tabs to spaces
				c = ' ';
			if (c >= 'a' && c <= 'z')	// convert to uppercase
				c = c - 'a' + 'A';
			linebuffer[i] = c;
			if (c == 0)
				break; // modified by Hermes
		}
		char *ptr = linebuffer;

		char *opcode = NULL;
		char *label = NULL;
		char *col_ptr;
		if ((col_ptr = strstr(ptr, ":")) != NULL)
		{
			int		j;
			bool	valid;
			j = 0;
			valid = true;
			while ((ptr+j) < col_ptr)
			{
				if (j == 0)
					if (!((ptr[j] >= 'A' && ptr[j] <= 'Z') || (ptr[j] == '_')))
						valid = false;
				if (!((ptr[j] >= '0' && ptr[j] <= '9') || (ptr[j] >= 'A' && ptr[j] <= 'Z') || (ptr[j] == '_')))
					valid = false;
				j++;
			}
			if (valid)
			{
				label = strtok(ptr, ":\x20");
				ptr	= col_ptr + 1;
			}
		}

		opcode = strtok(ptr, " ");
		char *opcode_ext = NULL;

		u32 params_count = 0;
		u32 params_count_ext = 0;
		if (opcode)
		{
			if ((opcode_ext = strstr(opcode, "'")) != NULL)
			{
				opcode_ext[0] = '\0';
				opcode_ext++;
				if (strlen(opcode_ext) == 0)
					opcode_ext = NULL;
			}
			// now we have opcode and label

			params_count = 0;
			params_count_ext = 0;

			char *paramstr = strtok(NULL, "\0");
			char *paramstr_ext = 0;
			// there is valid opcode so probably we have parameters

			if (paramstr)
			{
				if ((paramstr_ext = strstr(paramstr, ":")) != NULL)
				{
					paramstr_ext[0] = '\0';
					paramstr_ext++;
				}
			}

			if (paramstr)
				params_count = get_params(paramstr, params);
			if (paramstr_ext)
				params_count_ext = get_params(paramstr_ext, params_ext);
		}

		if (label)
		{
			// there is a valid label so lets store it in labels table
			u32 lval = cur_addr;
			if (opcode)
			{
				if (strcmp(opcode, "EQU") == 0)
				{
					lval = params[0].val;
					opcode = NULL;
				}
			}
			if (pass == 1)
				gd_ass_register_label(label, lval);
		}

		if (opcode == NULL)
			continue;

		// check if opcode is reserved compiler word
		if (strcmp("INCLUDE", opcode) == 0)
		{
			if (params[0].type == P_STR)
			{
				char *tmpstr;
				if (include_dir.size())
				{
					tmpstr = (char *)malloc(include_dir.size() + strlen(params[0].str) + 2);
					sprintf(tmpstr, "%s/%s", include_dir.c_str(), params[0].str);
				}
				else
				{
					tmpstr = (char *)malloc(strlen(params[0].str) + 1);
					strcpy(tmpstr, params[0].str);
				}
				AssembleFile(tmpstr, pass);
				free(tmpstr);
			}
			else
				parse_error(ERR_EXPECTED_PARAM_STR);
			continue;
		}

		if (strcmp("INCDIR", opcode) == 0)
		{
			if (params[0].type == P_STR)
				include_dir = params[0].str;
			else
				parse_error(ERR_EXPECTED_PARAM_STR);
			continue;
		}

		if (strcmp("ORG", opcode) == 0)
		{
			if (params[0].type == P_VAL)
				cur_addr = params[0].val;
			else
				parse_error(ERR_EXPECTED_PARAM_VAL);
			continue;
		}

		if (strcmp("SEGMENT", opcode) == 0)
		{
			if (params[0].type == P_STR)
			{
				segment_addr[cur_segment] = cur_addr;
				if (strcmp("DATA", params[0].str) == 0)
					cur_segment = SEGMENT_DATA;
				if (strcmp("CODE", params[0].str) == 0)
					cur_segment = SEGMENT_CODE;
				cur_addr = segment_addr[cur_segment];
			}
			else
				parse_error(ERR_EXPECTED_PARAM_STR);
			continue;
		}

		const opc_t *opc = find_opcode(opcode, params_count, opcodes, opcodes_size);
		if (!opc)
			opc = &cw;

		opcode_size = opc->size & ~P_EXT;

		verify_params(opc, params, params_count);

		const opc_t *opc_ext = NULL;
		// Check for opcode extensions.
		if (opc->size & P_EXT)
		{
			if (opcode_ext)
			{
				opc_ext = find_opcode(opcode_ext, params_count_ext, opcodes_ext, opcodes_ext_size);
				verify_params(opc_ext, params_ext, params_count_ext, true);
			}
			else if (params_count_ext)
				parse_error(ERR_EXT_PAR_NOT_EXT);
		}
		else
		{
			if (opcode_ext)
				parse_error(ERR_EXT_CANT_EXTEND_OPCODE);
			if (params_count_ext)
				parse_error(ERR_EXT_PAR_NOT_EXT);
		}

		if (pass == 2)
		{
			// generate binary
			((u16 *)gdg_buffer)[cur_addr] = 0x0000;
			build_code(opc, params, params_count, (u16 *)gdg_buffer);
			if (opc_ext)
				build_code(opc_ext, params_ext, params_count_ext, (u16 *)gdg_buffer);
		}

		cur_addr += opcode_size;
	};
	if (gdg_buffer == NULL)
	{
		gdg_buffer_size = cur_addr;
		gdg_buffer = (char *)malloc(gdg_buffer_size * sizeof(u16) + 4);
		memset(gdg_buffer, 0, gdg_buffer_size * sizeof(u16));
	}
	fclose(fsrc);
	return !failed;
}
