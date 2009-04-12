/*====================================================================

   project:      GameCube DSP Tool (gcdsp)
   created:      2005.03.04
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

   ====================================================================*/

#ifndef _DSP_ASSEMBLE_H
#define _DSP_ASSEMBLE_H

#include <string>
#include <map>

#include "Common.h"
#include "disassemble.h"
#include "DSPTables.h"

enum err_t
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

// Unless you want labels to carry over between files, you probably
// want to create a new DSPAssembler for every file you assemble.
class DSPAssembler
{
public:
	DSPAssembler();
	~DSPAssembler();

	void Assemble(const char *text, std::vector<u16> *code);

	typedef struct fass_t
	{
		FILE	*fsrc;
		u32	code_line;
		bool failed;
	} fass_t;

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
	void gd_ass_init_pass(int pass);
	bool gd_ass_file(gd_globals_t *gdg, const char *fname, int pass);

private:
	void parse_error(err_t err_code, fass_t *fa, const char *extra_info = NULL);
	void gd_ass_register_label(const char *label, u16 lval);
	void gd_ass_clear_labels();
	s32 strtoval(const char *str);
	char *find_brackets(char *src, char *dst);
	u32 parse_exp(const char *ptr);
	u32 parse_exp_f(const char *ptr, fass_t *fa);
	u32 get_params(char *parstr, param_t *par, fass_t *fa);
	const opc_t *find_opcode(const char *opcode, u32 par_count, const opc_t * const opcod, u32 opcod_size, fass_t *fa);
	bool verify_params(const opc_t *opc, param_t *par, u32 count, fass_t *fa);
	void build_code(const opc_t *opc, param_t *par, u32 par_count, u16 *outbuf);

	char *include_dir;

	label_t labels[10000];
	int	labels_count;

	char cur_line[4096];

	u32 cur_addr;
	u8  cur_pass;
	fass_t *cur_fa;

	typedef std::map<std::string, std::string> AliasMap;
	AliasMap aliases;

	segment_t cur_segment;
	u32 segment_addr[SEGMENT_MAX];

	int current_param;
};

#endif  // _DSP_ASSEMBLE_H