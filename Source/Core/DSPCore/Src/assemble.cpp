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
#include "DSPInterpreter.h"
#include "DSPTables.h"
#include "disassemble.h"
//#include "gdsp_tool.h"

char *include_dir = NULL;

struct label_t
{
	char	*label;
	s32	addr;
};

struct param_t
{
	u32		val;
	partype_t	type;
	char		*str;
};

enum segment_t
{
	SEGMENT_CODE = 0,
	SEGMENT_DATA,
	SEGMENT_OVERLAY,
	SEGMENT_MAX
};

typedef struct fass_t
{
	FILE	*fsrc;
	u32	code_line;
	bool failed;
} fass_t;

label_t labels[10000];
int	labels_count = 0;

char cur_line[4096];

u32 cur_addr;
u8  cur_pass;
fass_t *cur_fa;

typedef std::map<std::string, std::string> AliasMap;
AliasMap aliases;

segment_t cur_segment;
u32 segment_addr[SEGMENT_MAX];

int current_param = 0;

typedef enum err_t
{
	ERR_OK = 0,
	ERR_UNKNOWN,
	ERR_UNKNOWN_OPCODE,
	ERR_NOT_ENOUGH_PARAMETERS,
	ERR_TOO_MANY_PARAMETERS,
	ERR_WRONG_PARAMETER,
	ERR_EXPECTED_PARAM_STR,
	ERR_EXPECTED_PARAM_VAL,
	ERR_EXPECTED_PARAM_REG,
	ERR_EXPECTED_PARAM_MEM,
	ERR_EXPECTED_PARAM_IMM,
	ERR_INCORRECT_BIN,
	ERR_INCORRECT_HEX,
	ERR_INCORRECT_DEC,
	ERR_LABEL_EXISTS,
	ERR_UNKNOWN_LABEL,
	ERR_NO_MATCHING_BRACKETS,
	ERR_EXT_CANT_EXTEND_OPCODE,
	ERR_EXT_PAR_NOT_EXT,
	ERR_WRONG_PARAMETER_ACC,
	ERR_WRONG_PARAMETER_MID_ACC,
	ERR_INVALID_REGISTER,
	ERR_OUT_RANGE_NUMBER
};

char *err_string[] =
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

void parse_error(err_t err_code, fass_t *fa, const char *extra_info = NULL)
{
	fprintf(stderr, "%i : %s\n", fa->code_line, cur_line);
	fa->failed = true;
	if (!extra_info)
		extra_info = "-";
	if (fa->fsrc)
		fclose(fa->fsrc);
	else
	{
		fprintf(stderr, "ERROR: %s : %s\n", err_string[err_code], extra_info);
	}

	// modified by Hermes

	if (current_param == 0)
		fprintf(stderr, "ERROR: %s Line: %d : %s\n", err_string[err_code], fa->code_line, extra_info);
	else 
		fprintf(stderr, "ERROR: %s Line: %d Param: %d : %s\n",
		        err_string[err_code], fa->code_line, current_param, extra_info);
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

void gd_ass_register_label(const char *label, u16 lval)
{
	labels[labels_count].label = (char *)malloc(strlen(label) + 1);
	strcpy(labels[labels_count].label, label);
	labels[labels_count].addr = lval;
	labels_count++;
}

void gd_ass_clear_labels()
{
	for (int i = 0; i < labels_count; i++)
	{
		free(labels[i].label);
	}
	labels_count = 0;
}

// Parse a standalone value - it can be a number in one of several formats or a label.
s32 strtoval(const char *str)
{
	bool negative = false;
	s32 val = 0;
	const char *ptr = str;

	if (ptr[0] == '#')
	{
		ptr++;
		negative = true;  // Wow! Double # (needed one to get in here) ]negates???
	}
	if (ptr[0] == '-')
	{
		ptr++;
		negative = true;  // Wow! # negates???
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
					parse_error(ERR_INCORRECT_DEC, cur_fa, str);
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
						val += (ptr[i]-'0');
					else
						parse_error(ERR_INCORRECT_HEX, cur_fa, str);
				}
				break;
			case '\'': // binary
				for (int i = 2; ptr[i] != 0; i++)
				{
					val *=2;
					if(ptr[i] >= '0' && ptr[i] <= '1')
						val += ptr[i] - '0';
					else
						parse_error(ERR_INCORRECT_BIN, cur_fa, str);
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
					parse_error(ERR_INCORRECT_DEC, cur_fa, str);
			}
		}
		else  // Everything else is a label.
		{
			// Lookup label
			for (int i = 0; i < labels_count; i++)
			{
				if (strcmp(labels[i].label, ptr) == 0)
					return labels[i].addr;
			}
			if (cur_pass == 2)
				parse_error(ERR_UNKNOWN_LABEL, cur_fa, str);
		}
	}
	if (negative)
		return -val;
	return val;
}


// Modifies both src and dst!
// What does it do, really??
char *find_brackets(char *src, char *dst)
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
		parse_error(ERR_NO_MATCHING_BRACKETS, cur_fa);
	return NULL;
}

// Bizarre in-place expression evaluator.
u32 parse_exp(const char *ptr)
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
			if(cur_fa)
				fprintf(stderr, "WARNING: Number Underflow at Line: %d \n", cur_fa->code_line);
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

u32 parse_exp_f(const char *ptr, fass_t *fa)
{
	cur_fa = fa;
	return parse_exp(ptr);
}

// Destroys parstr
u32 get_params(char *parstr, param_t *par, fass_t *fa)
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
			par[i].val = parse_exp_f(tmpstr + 1, fa);
			par[i].type = P_IMM;
			break;
		case '@':
			if (tmpstr[1] == '$')
			{
				par[i].val = parse_exp_f(tmpstr + 2, fa);
				par[i].type = P_PRG;
			}
			else
			{
				par[i].val = parse_exp_f(tmpstr + 1, fa);
				par[i].type = P_MEM;
			}
			break;
		case '$':
			par[i].val = parse_exp_f(tmpstr + 1, fa);
			par[i].type = P_REG;
			break;
		default:
			par[i].val = parse_exp_f(tmpstr, fa);
			par[i].type = P_VAL;
			break;
		}
		tmpstr = strtok(NULL, ",\x00");
	}
	return count;
}

const opc_t *find_opcode(const char *opcode, u32 par_count, const opc_t * const opcod, u32 opcod_size, fass_t *fa)
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
				parse_error(ERR_NOT_ENOUGH_PARAMETERS, fa);
			}
			if (par_count > opc->param_count)
			{
				parse_error(ERR_TOO_MANY_PARAMETERS, fa);
			}
			return opc;
		}
	}
	parse_error(ERR_UNKNOWN_OPCODE, fa);
	return NULL;
}

// weird...
u16 get_mask_shifted_down(u16 mask)
{
	while (!(mask & 1))
		mask >>= 1;
	return mask;
}

bool verify_params(const opc_t *opc, param_t *par, u32 count, fass_t *fa)
{
	int value;
	unsigned int valueu;
	for (u32 i = 0; i < count; i++)
	{
		current_param = i+1;
		if (opc->params[i].type != par[i].type || (par[i].type & P_REG))
		{
			if ((opc->params[i].type & P_REG) && (par[i].type & P_REG))
			{
				// modified by Hermes: test the register range
				switch ((unsigned)opc->params[i].type)
				{ 
				case P_REG18:
				case P_REG19:
				case P_REG1A:
				//case P_REG1C:
					value = (opc->params[i].type >> 8) & 31;
					if ((int)par[i].val < value ||
						(int)par[i].val > value + get_mask_shifted_down(opc->params[i].mask))
					{
						parse_error(ERR_INVALID_REGISTER, fa);
					}
					break;
				case P_PRG:
					if ((int)par[i].val < 0 || (int)par[i].val > 0x3)
					{
						parse_error(ERR_INVALID_REGISTER, fa);
					}
					break;
				case P_ACC:
					if ((int)par[i].val < 0x20 || (int)par[i].val > 0x21)
					{
						if (par[i].val >= 0x1e && par[i].val <= 0x1f)
							fprintf(stderr, "WARNING: $ACM%d register used instead $ACC%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else if (par[i].val >= 0x1c && par[i].val <= 0x1d)
							fprintf(stderr, "WARNING: $ACL%d register used instead $ACC%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else
							parse_error(ERR_WRONG_PARAMETER_ACC, fa);
					}
					break;
				case P_ACCM:
					if ((int)par[i].val < 0x1e || (int)par[i].val > 0x1f)
					{
						if (par[i].val >= 0x1c && par[i].val <= 0x1d)
							fprintf(stderr, "WARNING: $ACL%d register used instead $ACM%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else if (par[i].val >= 0x20 && par[i].val <= 0x21)
							fprintf(stderr, "WARNING: $ACC%d register used instead $ACM%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else
							parse_error(ERR_WRONG_PARAMETER_ACC, fa);
					}
					break;

				case P_ACCL:
					if ((int)par[i].val < 0x1c || (int)par[i].val > 0x1d)
					{
						if (par[i].val >= 0x1e && par[i].val <= 0x1f)
							fprintf(stderr, "WARNING: $ACM%d register used instead $ACL%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else if (par[i].val >= 0x20 && par[i].val <= 0x21)
							fprintf(stderr, "WARNING: $ACC%d register used instead $ACL%d register Line: %d Param: %d\n",
								(par[i].val & 1), (par[i].val & 1), fa->code_line, current_param);
						else
							parse_error(ERR_WRONG_PARAMETER_ACC, fa);
					}
					break;
/*				case P_ACCM_D: //P_ACC_MID:
					if ((int)par[i].val < 0x1e || (int)par[i].val > 0x1f)
					{
						parse_error(ERR_WRONG_PARAMETER_MID_ACC, fa);
					}
					break;*/
				}
				continue;
			}

			switch (par[i].type & (P_REG | P_VAL | P_MEM | P_IMM))
			{
			case P_REG:
				parse_error(ERR_EXPECTED_PARAM_REG, fa);
				break;
			case P_MEM:
				parse_error(ERR_EXPECTED_PARAM_MEM, fa);
				break;
			case P_VAL:
				parse_error(ERR_EXPECTED_PARAM_VAL, fa);
				break;
			case P_IMM:
				parse_error(ERR_EXPECTED_PARAM_IMM, fa);
				break;
			}
			parse_error(ERR_WRONG_PARAMETER, fa);
			break;
		}
		else if ((opc->params[i].type & 3) != 0 && (par[i].type & 3) != 0)
		{
			// modified by Hermes: test NUMBER range
			value = get_mask_shifted_down(opc->params[i].mask);

			valueu = 0xffff & ~(value >> 1);
			if ((int)par[i].val < 0)
			{
				if (value == 7) // value 7 por sbclr/sbset
				{
					fprintf(stderr,"Value must be from 0x0 to 0x%x\n", value);
					parse_error(ERR_OUT_RANGE_NUMBER, fa);
				}
				else if (opc->params[i].type == P_MEM)
				{
					if (value < 256)
						fprintf(stderr,"Address value must be from 0x%x to 0x%x\n",valueu, (value>>1));
					else
						fprintf(stderr,"Address value must be from 0x0 to 0x%x\n", value);

					parse_error(ERR_OUT_RANGE_NUMBER, fa);
				}
				else if ((int)par[i].val < -((value >> 1) + 1))
				{
					if (value < 128)
						fprintf(stderr, "Value must be from -0x%x to 0x%x, is %i\n",
						        (value >> 1) + 1, value >> 1, par[i].val);
					else
						fprintf(stderr, "Value must be from -0x%x to 0x%x or 0x0 to 0x%x, is %i\n",
						        (value >> 1) + 1, value >> 1, value, par[i].val);

					parse_error(ERR_OUT_RANGE_NUMBER, fa);
				}
			}
			else
			{
				if (value == 7) // value 7 por sbclr/sbset
				{
					if (par[i].val > (unsigned)value)
					{
						fprintf(stderr,"Value must be from 0x%x to 0x%x, is %i\n",valueu, value, par[i].val);
						parse_error(ERR_OUT_RANGE_NUMBER, fa);
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
						parse_error(ERR_OUT_RANGE_NUMBER, fa);
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
						parse_error(ERR_OUT_RANGE_NUMBER, fa);
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
void build_code(const opc_t *opc, param_t *par, u32 par_count, u16 *outbuf)
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

void gd_ass_init_pass(int pass)
{
	if (pass == 1)
	{
		// Reset label table. Pre-populate with hw addresses and registers.
		gd_ass_clear_labels();
		for (int i = 0; i < 0x24; i++)
		{
			gd_ass_register_label(regnames[i].name, regnames[i].addr);
		}
		for (int i = 0; i < pdlabels_size; i++)
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

bool gd_ass_file(gd_globals_t *gdg, const char *fname, int pass)
{
	fass_t fa;
    int disable_text = 0; // modified by Hermes

	param_t params[10] = {0};
	param_t params_ext[10] = {0};
	u32 params_count = 0;
	u32 params_count_ext = 0;

	fa.failed = false;
	fa.fsrc = fopen(fname, "r");
	if (fa.fsrc == NULL)
	{
		fprintf(stderr, "Cannot open %s file\n", fname);
		return false;
	}

	fseek(fa.fsrc, 0, SEEK_SET);
	printf("Pass %d\n", pass);
	fa.code_line = 0;
	cur_pass = pass;

#define LINEBUF_SIZE 4096
	char linebuffer[LINEBUF_SIZE];
	while (!feof(fa.fsrc) && !fa.failed)
	{
		int opcode_size = 0;
		memset(linebuffer, 0, LINEBUF_SIZE);
		if (!fgets(linebuffer, LINEBUF_SIZE, fa.fsrc))
			break;
		strcpy(cur_line, linebuffer);
		//printf("A: %s", linebuffer);
		fa.code_line++;

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
						// wtf is this?
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


			if(disable_text && ((unsigned char )c)>32) c=32;

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

		char *paramstr;
		char *paramstr_ext;

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

			paramstr = paramstr_ext = 0;
			// there is valid opcode so probably we have parameters

			paramstr = strtok(NULL, "\0");

			if (paramstr)
			{
				if ((paramstr_ext = strstr(paramstr, ":")) != NULL)
				{
					paramstr_ext[0] = '\0';
					paramstr_ext++;
				}
			}

			if (paramstr)
				params_count = get_params(paramstr, params, &fa);
			if (paramstr_ext)
				params_count_ext = get_params(paramstr_ext, params_ext, &fa);
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
			{
				gd_ass_register_label(label, lval);
			}
		}

		if (opcode == NULL)
			continue;

		// check if opcode is reserved compiler word
		if (strcmp("INCLUDE", opcode) == 0)
		{
			if (params[0].type == P_STR)
			{
				char *tmpstr;
				if (include_dir)
				{
					tmpstr = (char *)malloc(strlen(include_dir) + strlen(params[0].str) + 2);
					sprintf(tmpstr, "%s/%s", include_dir, params[0].str);
				}
				else
				{
					tmpstr = (char *)malloc(strlen(params[0].str) + 1);
					strcpy(tmpstr, params[0].str);
				}
				gd_ass_file(gdg, tmpstr, pass);
				free(tmpstr);
			}
			else
				parse_error(ERR_EXPECTED_PARAM_STR, &fa);
			continue;
		}

		if (strcmp("INCDIR", opcode) == 0)
		{
			if (params[0].type == P_STR)
			{
				if (include_dir) free(include_dir);
				include_dir = (char *)malloc(strlen(params[0].str) + 1);
				strcpy(include_dir, params[0].str);
			}
			else
				parse_error(ERR_EXPECTED_PARAM_STR, &fa);
			continue;
		}

		if (strcmp("ORG", opcode) == 0)
		{
			if (params[0].type == P_VAL)
				cur_addr = params[0].val;
			else
				parse_error(ERR_EXPECTED_PARAM_VAL, &fa);
			continue;
		}

		if (strcmp("SEGMENT", opcode) == 0)
		{
			if(params[0].type == P_STR)
			{
				segment_addr[cur_segment] = cur_addr;
				if (strcmp("DATA", params[0].str) == 0)
					cur_segment = SEGMENT_DATA;
				if (strcmp("CODE", params[0].str) == 0)
					cur_segment = SEGMENT_CODE;
				cur_addr = segment_addr[cur_segment];
			}
			else
				parse_error(ERR_EXPECTED_PARAM_STR, &fa);
			continue;
		}

		const opc_t *opc = find_opcode(opcode, params_count, opcodes, opcodes_size, &fa);
		if (!opc)
			opc = &cw;

		opcode_size = opc->size & ~P_EXT;

		verify_params(opc, params, params_count, &fa);

		const opc_t *opc_ext = NULL;
		// Check for opcode extensions.
		if (opc->size & P_EXT)
		{
			if (opcode_ext)
			{
				opc_ext = find_opcode(opcode_ext, params_count_ext, opcodes_ext, opcodes_ext_size, &fa);
				verify_params(opc_ext, params_ext, params_count_ext, &fa);
			}
			else if (params_count_ext)
				parse_error(ERR_EXT_PAR_NOT_EXT, &fa);
		}
		else
		{
			if (opcode_ext)
				parse_error(ERR_EXT_CANT_EXTEND_OPCODE, &fa);
			if (params_count_ext)
				parse_error(ERR_EXT_PAR_NOT_EXT, &fa);
		}

		if (pass == 2)
		{
			// generate binary
			((u16 *)gdg->buffer)[cur_addr] = 0x0000;
			build_code(opc, params, params_count, (u16 *)gdg->buffer);
			if (opc_ext)
				build_code(opc_ext, params_ext, params_count_ext, (u16 *)gdg->buffer);
		}

		cur_addr += opcode_size;
	};
	if (gdg->buffer == NULL)
	{
		gdg->buffer_size = cur_addr;
		gdg->buffer = (char *)malloc(gdg->buffer_size * sizeof(u16) + 4);
		memset(gdg->buffer, 0, gdg->buffer_size * sizeof(u16));
	}
	fclose(fa.fsrc);
	return !fa.failed;
}
