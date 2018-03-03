/*
 * Copyright 2015-2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "spirv_cross.hpp"
#include "GLSL.std.450.h"
#include "spirv_cfg.hpp"
#include <algorithm>
#include <cstring>
#include <utility>

using namespace std;
using namespace spv;
using namespace spirv_cross;

#define log(...) fprintf(stderr, __VA_ARGS__)

static string ensure_valid_identifier(const string &name, bool member)
{
	// Functions in glslangValidator are mangled with name(<mangled> stuff.
	// Normally, we would never see '(' in any legal identifiers, so just strip them out.
	auto str = name.substr(0, name.find('('));

	for (uint32_t i = 0; i < str.size(); i++)
	{
		auto &c = str[i];

		if (member)
		{
			// _m<num> variables are reserved by the internal implementation,
			// otherwise, make sure the name is a valid identifier.
			if (i == 0)
				c = isalpha(c) ? c : '_';
			else if (i == 2 && str[0] == '_' && str[1] == 'm')
				c = isalpha(c) ? c : '_';
			else
				c = isalnum(c) ? c : '_';
		}
		else
		{
			// _<num> variables are reserved by the internal implementation,
			// otherwise, make sure the name is a valid identifier.
			if (i == 0 || (str[0] == '_' && i == 1))
				c = isalpha(c) ? c : '_';
			else
				c = isalnum(c) ? c : '_';
		}
	}
	return str;
}

Instruction::Instruction(const vector<uint32_t> &spirv, uint32_t &index)
{
	op = spirv[index] & 0xffff;
	count = (spirv[index] >> 16) & 0xffff;

	if (count == 0)
		SPIRV_CROSS_THROW("SPIR-V instructions cannot consume 0 words. Invalid SPIR-V file.");

	offset = index + 1;
	length = count - 1;

	index += count;

	if (index > spirv.size())
		SPIRV_CROSS_THROW("SPIR-V instruction goes out of bounds.");
}

Compiler::Compiler(vector<uint32_t> ir)
    : spirv(move(ir))
{
	parse();
}

Compiler::Compiler(const uint32_t *ir, size_t word_count)
    : spirv(ir, ir + word_count)
{
	parse();
}

string Compiler::compile()
{
	// Force a classic "C" locale, reverts when function returns
	ClassicLocale classic_locale;
	return "";
}

bool Compiler::variable_storage_is_aliased(const SPIRVariable &v)
{
	auto &type = get<SPIRType>(v.basetype);
	bool ssbo = v.storage == StorageClassStorageBuffer ||
	            ((meta[type.self].decoration.decoration_flags & (1ull << DecorationBufferBlock)) != 0);
	bool image = type.basetype == SPIRType::Image;
	bool counter = type.basetype == SPIRType::AtomicCounter;
	bool is_restrict = (meta[v.self].decoration.decoration_flags & (1ull << DecorationRestrict)) != 0;
	return !is_restrict && (ssbo || image || counter);
}

bool Compiler::block_is_pure(const SPIRBlock &block)
{
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		case OpFunctionCall:
		{
			uint32_t func = ops[2];
			if (!function_is_pure(get<SPIRFunction>(func)))
				return false;
			break;
		}

		case OpCopyMemory:
		case OpStore:
		{
			auto &type = expression_type(ops[0]);
			if (type.storage != StorageClassFunction)
				return false;
			break;
		}

		case OpImageWrite:
			return false;

		// Atomics are impure.
		case OpAtomicLoad:
		case OpAtomicStore:
		case OpAtomicExchange:
		case OpAtomicCompareExchange:
		case OpAtomicCompareExchangeWeak:
		case OpAtomicIIncrement:
		case OpAtomicIDecrement:
		case OpAtomicIAdd:
		case OpAtomicISub:
		case OpAtomicSMin:
		case OpAtomicUMin:
		case OpAtomicSMax:
		case OpAtomicUMax:
		case OpAtomicAnd:
		case OpAtomicOr:
		case OpAtomicXor:
			return false;

		// Geometry shader builtins modify global state.
		case OpEndPrimitive:
		case OpEmitStreamVertex:
		case OpEndStreamPrimitive:
		case OpEmitVertex:
			return false;

		// Barriers disallow any reordering, so we should treat blocks with barrier as writing.
		case OpControlBarrier:
		case OpMemoryBarrier:
			return false;

			// OpExtInst is potentially impure depending on extension, but GLSL builtins are at least pure.

		default:
			break;
		}
	}

	return true;
}

string Compiler::to_name(uint32_t id, bool allow_alias) const
{
	if (allow_alias && ids.at(id).get_type() == TypeType)
	{
		// If this type is a simple alias, emit the
		// name of the original type instead.
		// We don't want to override the meta alias
		// as that can be overridden by the reflection APIs after parse.
		auto &type = get<SPIRType>(id);
		if (type.type_alias)
			return to_name(type.type_alias);
	}

	if (meta[id].decoration.alias.empty())
		return join("_", id);
	else
		return meta.at(id).decoration.alias;
}

bool Compiler::function_is_pure(const SPIRFunction &func)
{
	for (auto block : func.blocks)
	{
		if (!block_is_pure(get<SPIRBlock>(block)))
		{
			//fprintf(stderr, "Function %s is impure!\n", to_name(func.self).c_str());
			return false;
		}
	}

	//fprintf(stderr, "Function %s is pure!\n", to_name(func.self).c_str());
	return true;
}

void Compiler::register_global_read_dependencies(const SPIRBlock &block, uint32_t id)
{
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		case OpFunctionCall:
		{
			uint32_t func = ops[2];
			register_global_read_dependencies(get<SPIRFunction>(func), id);
			break;
		}

		case OpLoad:
		case OpImageRead:
		{
			// If we're in a storage class which does not get invalidated, adding dependencies here is no big deal.
			auto *var = maybe_get_backing_variable(ops[2]);
			if (var && var->storage != StorageClassFunction)
			{
				auto &type = get<SPIRType>(var->basetype);

				// InputTargets are immutable.
				if (type.basetype != SPIRType::Image && type.image.dim != DimSubpassData)
					var->dependees.push_back(id);
			}
			break;
		}

		default:
			break;
		}
	}
}

void Compiler::register_global_read_dependencies(const SPIRFunction &func, uint32_t id)
{
	for (auto block : func.blocks)
		register_global_read_dependencies(get<SPIRBlock>(block), id);
}

SPIRVariable *Compiler::maybe_get_backing_variable(uint32_t chain)
{
	auto *var = maybe_get<SPIRVariable>(chain);
	if (!var)
	{
		auto *cexpr = maybe_get<SPIRExpression>(chain);
		if (cexpr)
			var = maybe_get<SPIRVariable>(cexpr->loaded_from);

		auto *access_chain = maybe_get<SPIRAccessChain>(chain);
		if (access_chain)
			var = maybe_get<SPIRVariable>(access_chain->loaded_from);
	}

	return var;
}

void Compiler::register_read(uint32_t expr, uint32_t chain, bool forwarded)
{
	auto &e = get<SPIRExpression>(expr);
	auto *var = maybe_get_backing_variable(chain);

	if (var)
	{
		e.loaded_from = var->self;

		// If the backing variable is immutable, we do not need to depend on the variable.
		if (forwarded && !is_immutable(var->self))
			var->dependees.push_back(e.self);

		// If we load from a parameter, make sure we create "inout" if we also write to the parameter.
		// The default is "in" however, so we never invalidate our compilation by reading.
		if (var && var->parameter)
			var->parameter->read_count++;
	}
}

void Compiler::register_write(uint32_t chain)
{
	auto *var = maybe_get<SPIRVariable>(chain);
	if (!var)
	{
		// If we're storing through an access chain, invalidate the backing variable instead.
		auto *expr = maybe_get<SPIRExpression>(chain);
		if (expr && expr->loaded_from)
			var = maybe_get<SPIRVariable>(expr->loaded_from);

		auto *access_chain = maybe_get<SPIRAccessChain>(chain);
		if (access_chain && access_chain->loaded_from)
			var = maybe_get<SPIRVariable>(access_chain->loaded_from);
	}

	if (var)
	{
		// If our variable is in a storage class which can alias with other buffers,
		// invalidate all variables which depend on aliased variables.
		if (variable_storage_is_aliased(*var))
			flush_all_aliased_variables();
		else if (var)
			flush_dependees(*var);

		// We tried to write to a parameter which is not marked with out qualifier, force a recompile.
		if (var->parameter && var->parameter->write_count == 0)
		{
			var->parameter->write_count++;
			force_recompile = true;
		}
	}
}

void Compiler::flush_dependees(SPIRVariable &var)
{
	for (auto expr : var.dependees)
		invalid_expressions.insert(expr);
	var.dependees.clear();
}

void Compiler::flush_all_aliased_variables()
{
	for (auto aliased : aliased_variables)
		flush_dependees(get<SPIRVariable>(aliased));
}

void Compiler::flush_all_atomic_capable_variables()
{
	for (auto global : global_variables)
		flush_dependees(get<SPIRVariable>(global));
	flush_all_aliased_variables();
}

void Compiler::flush_all_active_variables()
{
	// Invalidate all temporaries we read from variables in this block since they were forwarded.
	// Invalidate all temporaries we read from globals.
	for (auto &v : current_function->local_variables)
		flush_dependees(get<SPIRVariable>(v));
	for (auto &arg : current_function->arguments)
		flush_dependees(get<SPIRVariable>(arg.id));
	for (auto global : global_variables)
		flush_dependees(get<SPIRVariable>(global));

	flush_all_aliased_variables();
}

uint32_t Compiler::expression_type_id(uint32_t id) const
{
	switch (ids[id].get_type())
	{
	case TypeVariable:
		return get<SPIRVariable>(id).basetype;

	case TypeExpression:
		return get<SPIRExpression>(id).expression_type;

	case TypeConstant:
		return get<SPIRConstant>(id).constant_type;

	case TypeConstantOp:
		return get<SPIRConstantOp>(id).basetype;

	case TypeUndef:
		return get<SPIRUndef>(id).basetype;

	case TypeCombinedImageSampler:
		return get<SPIRCombinedImageSampler>(id).combined_type;

	case TypeAccessChain:
		return get<SPIRAccessChain>(id).basetype;

	default:
		SPIRV_CROSS_THROW("Cannot resolve expression type.");
	}
}

const SPIRType &Compiler::expression_type(uint32_t id) const
{
	return get<SPIRType>(expression_type_id(id));
}

bool Compiler::expression_is_lvalue(uint32_t id) const
{
	auto &type = expression_type(id);
	switch (type.basetype)
	{
	case SPIRType::SampledImage:
	case SPIRType::Image:
	case SPIRType::Sampler:
		return false;

	default:
		return true;
	}
}

bool Compiler::is_immutable(uint32_t id) const
{
	if (ids[id].get_type() == TypeVariable)
	{
		auto &var = get<SPIRVariable>(id);

		// Anything we load from the UniformConstant address space is guaranteed to be immutable.
		bool pointer_to_const = var.storage == StorageClassUniformConstant;
		return pointer_to_const || var.phi_variable || !expression_is_lvalue(id);
	}
	else if (ids[id].get_type() == TypeAccessChain)
		return get<SPIRAccessChain>(id).immutable;
	else if (ids[id].get_type() == TypeExpression)
		return get<SPIRExpression>(id).immutable;
	else if (ids[id].get_type() == TypeConstant || ids[id].get_type() == TypeConstantOp ||
	         ids[id].get_type() == TypeUndef)
		return true;
	else
		return false;
}

static inline bool storage_class_is_interface(spv::StorageClass storage)
{
	switch (storage)
	{
	case StorageClassInput:
	case StorageClassOutput:
	case StorageClassUniform:
	case StorageClassUniformConstant:
	case StorageClassAtomicCounter:
	case StorageClassPushConstant:
	case StorageClassStorageBuffer:
		return true;

	default:
		return false;
	}
}

bool Compiler::is_hidden_variable(const SPIRVariable &var, bool include_builtins) const
{
	if ((is_builtin_variable(var) && !include_builtins) || var.remapped_variable)
		return true;

	// Combined image samplers are always considered active as they are "magic" variables.
	if (find_if(begin(combined_image_samplers), end(combined_image_samplers), [&var](const CombinedImageSampler &samp) {
		    return samp.combined_id == var.self;
	    }) != end(combined_image_samplers))
	{
		return false;
	}

	bool hidden = false;
	if (check_active_interface_variables && storage_class_is_interface(var.storage))
		hidden = active_interface_variables.find(var.self) == end(active_interface_variables);
	return hidden;
}

bool Compiler::is_builtin_variable(const SPIRVariable &var) const
{
	if (var.compat_builtin || meta[var.self].decoration.builtin)
		return true;

	// We can have builtin structs as well. If one member of a struct is builtin, the struct must also be builtin.
	for (auto &m : meta[get<SPIRType>(var.basetype).self].members)
		if (m.builtin)
			return true;

	return false;
}

bool Compiler::is_member_builtin(const SPIRType &type, uint32_t index, BuiltIn *builtin) const
{
	auto &memb = meta[type.self].members;
	if (index < memb.size() && memb[index].builtin)
	{
		if (builtin)
			*builtin = memb[index].builtin_type;
		return true;
	}

	return false;
}

bool Compiler::is_scalar(const SPIRType &type) const
{
	return type.vecsize == 1 && type.columns == 1;
}

bool Compiler::is_vector(const SPIRType &type) const
{
	return type.vecsize > 1 && type.columns == 1;
}

bool Compiler::is_matrix(const SPIRType &type) const
{
	return type.vecsize > 1 && type.columns > 1;
}

bool Compiler::is_array(const SPIRType &type) const
{
	return !type.array.empty();
}

ShaderResources Compiler::get_shader_resources() const
{
	return get_shader_resources(nullptr);
}

ShaderResources Compiler::get_shader_resources(const unordered_set<uint32_t> &active_variables) const
{
	return get_shader_resources(&active_variables);
}

bool Compiler::InterfaceVariableAccessHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	uint32_t variable = 0;
	switch (opcode)
	{
	// Need this first, otherwise, GCC complains about unhandled switch statements.
	default:
		break;

	case OpFunctionCall:
	{
		// Invalid SPIR-V.
		if (length < 3)
			return false;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
		{
			auto *var = compiler.maybe_get<SPIRVariable>(args[i]);
			if (var && storage_class_is_interface(var->storage))
				variables.insert(args[i]);
		}
		break;
	}

	case OpAtomicStore:
	case OpStore:
		// Invalid SPIR-V.
		if (length < 1)
			return false;
		variable = args[0];
		break;

	case OpCopyMemory:
	{
		if (length < 2)
			return false;

		auto *var = compiler.maybe_get<SPIRVariable>(args[0]);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(variable);

		var = compiler.maybe_get<SPIRVariable>(args[1]);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(variable);
		break;
	}

	case OpExtInst:
	{
		if (length < 5)
			return false;
		uint32_t extension_set = args[2];
		if (compiler.get<SPIRExtension>(extension_set).ext == SPIRExtension::SPV_AMD_shader_explicit_vertex_parameter)
		{
			enum AMDShaderExplicitVertexParameter
			{
				InterpolateAtVertexAMD = 1
			};

			auto op = static_cast<AMDShaderExplicitVertexParameter>(args[3]);

			switch (op)
			{
			case InterpolateAtVertexAMD:
			{
				auto *var = compiler.maybe_get<SPIRVariable>(args[4]);
				if (var && storage_class_is_interface(var->storage))
					variables.insert(args[4]);
				break;
			}

			default:
				break;
			}
		}
		break;
	}

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpLoad:
	case OpCopyObject:
	case OpImageTexelPointer:
	case OpAtomicLoad:
	case OpAtomicExchange:
	case OpAtomicCompareExchange:
	case OpAtomicCompareExchangeWeak:
	case OpAtomicIIncrement:
	case OpAtomicIDecrement:
	case OpAtomicIAdd:
	case OpAtomicISub:
	case OpAtomicSMin:
	case OpAtomicUMin:
	case OpAtomicSMax:
	case OpAtomicUMax:
	case OpAtomicAnd:
	case OpAtomicOr:
	case OpAtomicXor:
		// Invalid SPIR-V.
		if (length < 3)
			return false;
		variable = args[2];
		break;
	}

	if (variable)
	{
		auto *var = compiler.maybe_get<SPIRVariable>(variable);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(variable);
	}
	return true;
}

unordered_set<uint32_t> Compiler::get_active_interface_variables() const
{
	// Traverse the call graph and find all interface variables which are in use.
	unordered_set<uint32_t> variables;
	InterfaceVariableAccessHandler handler(*this, variables);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);

	// If we needed to create one, we'll need it.
	if (dummy_sampler_id)
		variables.insert(dummy_sampler_id);

	return variables;
}

void Compiler::set_enabled_interface_variables(std::unordered_set<uint32_t> active_variables)
{
	active_interface_variables = move(active_variables);
	check_active_interface_variables = true;
}

ShaderResources Compiler::get_shader_resources(const unordered_set<uint32_t> *active_variables) const
{
	ShaderResources res;

	for (auto &id : ids)
	{
		if (id.get_type() != TypeVariable)
			continue;

		auto &var = id.get<SPIRVariable>();
		auto &type = get<SPIRType>(var.basetype);

		// It is possible for uniform storage classes to be passed as function parameters, so detect
		// that. To detect function parameters, check of StorageClass of variable is function scope.
		if (var.storage == StorageClassFunction || !type.pointer || is_builtin_variable(var))
			continue;

		if (active_variables && active_variables->find(var.self) == end(*active_variables))
			continue;

		// Input
		if (var.storage == StorageClassInput && interface_variable_exists_in_entry_point(var.self))
		{
			if (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock))
				res.stage_inputs.push_back(
				    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self) });
			else
				res.stage_inputs.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Subpass inputs
		else if (var.storage == StorageClassUniformConstant && type.image.dim == DimSubpassData)
		{
			res.subpass_inputs.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Outputs
		else if (var.storage == StorageClassOutput && interface_variable_exists_in_entry_point(var.self))
		{
			if (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock))
				res.stage_outputs.push_back(
				    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self) });
			else
				res.stage_outputs.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// UBOs
		else if (type.storage == StorageClassUniform &&
		         (meta[type.self].decoration.decoration_flags & (1ull << DecorationBlock)))
		{
			res.uniform_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self) });
		}
		// Old way to declare SSBOs.
		else if (type.storage == StorageClassUniform &&
		         (meta[type.self].decoration.decoration_flags & (1ull << DecorationBufferBlock)))
		{
			res.storage_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self) });
		}
		// Modern way to declare SSBOs.
		else if (type.storage == StorageClassStorageBuffer)
		{
			res.storage_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self) });
		}
		// Push constant blocks
		else if (type.storage == StorageClassPushConstant)
		{
			// There can only be one push constant block, but keep the vector in case this restriction is lifted
			// in the future.
			res.push_constant_buffers.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Images
		else if (type.storage == StorageClassUniformConstant && type.basetype == SPIRType::Image &&
		         type.image.sampled == 2)
		{
			res.storage_images.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Separate images
		else if (type.storage == StorageClassUniformConstant && type.basetype == SPIRType::Image &&
		         type.image.sampled == 1)
		{
			res.separate_images.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Separate samplers
		else if (type.storage == StorageClassUniformConstant && type.basetype == SPIRType::Sampler)
		{
			res.separate_samplers.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Textures
		else if (type.storage == StorageClassUniformConstant && type.basetype == SPIRType::SampledImage)
		{
			res.sampled_images.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
		// Atomic counters
		else if (type.storage == StorageClassAtomicCounter)
		{
			res.atomic_counters.push_back({ var.self, var.basetype, type.self, meta[var.self].decoration.alias });
		}
	}

	return res;
}

static inline uint32_t swap_endian(uint32_t v)
{
	return ((v >> 24) & 0x000000ffu) | ((v >> 8) & 0x0000ff00u) | ((v << 8) & 0x00ff0000u) | ((v << 24) & 0xff000000u);
}

static string extract_string(const vector<uint32_t> &spirv, uint32_t offset)
{
	string ret;
	for (uint32_t i = offset; i < spirv.size(); i++)
	{
		uint32_t w = spirv[i];

		for (uint32_t j = 0; j < 4; j++, w >>= 8)
		{
			char c = w & 0xff;
			if (c == '\0')
				return ret;
			ret += c;
		}
	}

	SPIRV_CROSS_THROW("String was not terminated before EOF");
}

static bool is_valid_spirv_version(uint32_t version)
{
	switch (version)
	{
	// Allow v99 since it tends to just work.
	case 99:
	case 0x10000: // SPIR-V 1.0
	case 0x10100: // SPIR-V 1.1
	case 0x10200: // SPIR-V 1.2
		return true;

	default:
		return false;
	}
}

void Compiler::parse()
{
	auto len = spirv.size();
	if (len < 5)
		SPIRV_CROSS_THROW("SPIRV file too small.");

	auto s = spirv.data();

	// Endian-swap if we need to.
	if (s[0] == swap_endian(MagicNumber))
		transform(begin(spirv), end(spirv), begin(spirv), [](uint32_t c) { return swap_endian(c); });

	if (s[0] != MagicNumber || !is_valid_spirv_version(s[1]))
		SPIRV_CROSS_THROW("Invalid SPIRV format.");

	uint32_t bound = s[3];
	ids.resize(bound);
	meta.resize(bound);

	uint32_t offset = 5;
	while (offset < len)
		inst.emplace_back(spirv, offset);

	for (auto &i : inst)
		parse(i);

	if (current_function)
		SPIRV_CROSS_THROW("Function was not terminated.");
	if (current_block)
		SPIRV_CROSS_THROW("Block was not terminated.");

	// Figure out specialization constants for work group sizes.
	for (auto &id : ids)
	{
		if (id.get_type() == TypeConstant)
		{
			auto &c = id.get<SPIRConstant>();
			if (meta[c.self].decoration.builtin && meta[c.self].decoration.builtin_type == BuiltInWorkgroupSize)
			{
				// In current SPIR-V, there can be just one constant like this.
				// All entry points will receive the constant value.
				for (auto &entry : entry_points)
				{
					entry.second.workgroup_size.constant = c.self;
					entry.second.workgroup_size.x = c.scalar(0, 0);
					entry.second.workgroup_size.y = c.scalar(0, 1);
					entry.second.workgroup_size.z = c.scalar(0, 2);
				}
			}
		}
	}
}

void Compiler::flatten_interface_block(uint32_t id)
{
	auto &var = get<SPIRVariable>(id);
	auto &type = get<SPIRType>(var.basetype);
	auto flags = meta.at(type.self).decoration.decoration_flags;

	if (!type.array.empty())
		SPIRV_CROSS_THROW("Type is array of UBOs.");
	if (type.basetype != SPIRType::Struct)
		SPIRV_CROSS_THROW("Type is not a struct.");
	if ((flags & (1ull << DecorationBlock)) == 0)
		SPIRV_CROSS_THROW("Type is not a block.");
	if (type.member_types.empty())
		SPIRV_CROSS_THROW("Member list of struct is empty.");

	uint32_t t = type.member_types[0];
	for (auto &m : type.member_types)
		if (t != m)
			SPIRV_CROSS_THROW("Types in block differ.");

	auto &mtype = get<SPIRType>(t);
	if (!mtype.array.empty())
		SPIRV_CROSS_THROW("Member type cannot be arrays.");
	if (mtype.basetype == SPIRType::Struct)
		SPIRV_CROSS_THROW("Member type cannot be struct.");

	// Inherit variable name from interface block name.
	meta.at(var.self).decoration.alias = meta.at(type.self).decoration.alias;

	auto storage = var.storage;
	if (storage == StorageClassUniform)
		storage = StorageClassUniformConstant;

	// Change type definition in-place into an array instead.
	// Access chains will still work as-is.
	uint32_t array_size = uint32_t(type.member_types.size());
	type = mtype;
	type.array.push_back(array_size);
	type.pointer = true;
	type.storage = storage;
	var.storage = storage;
}

void Compiler::update_name_cache(unordered_set<string> &cache, string &name)
{
	if (name.empty())
		return;

	if (cache.find(name) == end(cache))
	{
		cache.insert(name);
		return;
	}

	uint32_t counter = 0;
	auto tmpname = name;

	// If there is a collision (very rare),
	// keep tacking on extra identifier until it's unique.
	do
	{
		counter++;
		name = tmpname + "_" + convert_to_string(counter);
	} while (cache.find(name) != end(cache));
	cache.insert(name);
}

void Compiler::set_name(uint32_t id, const std::string &name)
{
	auto &str = meta.at(id).decoration.alias;
	str.clear();

	if (name.empty())
		return;

	// glslang uses identifiers to pass along meaningful information
	// about HLSL reflection.
	auto &m = meta.at(id);
	if (source.hlsl && name.size() >= 6 && name.find("@count") == name.size() - 6)
	{
		m.hlsl_magic_counter_buffer_candidate = true;
		m.hlsl_magic_counter_buffer_name = name.substr(0, name.find("@count"));
	}
	else
	{
		m.hlsl_magic_counter_buffer_candidate = false;
		m.hlsl_magic_counter_buffer_name.clear();
	}

	// Reserved for temporaries.
	if (name[0] == '_' && name.size() >= 2 && isdigit(name[1]))
		return;

	str = ensure_valid_identifier(name, false);
}

const SPIRType &Compiler::get_type(uint32_t id) const
{
	return get<SPIRType>(id);
}

const SPIRType &Compiler::get_type_from_variable(uint32_t id) const
{
	return get<SPIRType>(get<SPIRVariable>(id).basetype);
}

void Compiler::set_member_decoration(uint32_t id, uint32_t index, Decoration decoration, uint32_t argument)
{
	meta.at(id).members.resize(max(meta[id].members.size(), size_t(index) + 1));
	auto &dec = meta.at(id).members[index];
	dec.decoration_flags |= 1ull << decoration;

	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = true;
		dec.builtin_type = static_cast<BuiltIn>(argument);
		break;

	case DecorationLocation:
		dec.location = argument;
		break;

	case DecorationBinding:
		dec.binding = argument;
		break;

	case DecorationOffset:
		dec.offset = argument;
		break;

	case DecorationSpecId:
		dec.spec_id = argument;
		break;

	case DecorationMatrixStride:
		dec.matrix_stride = argument;
		break;

	default:
		break;
	}
}

void Compiler::set_member_name(uint32_t id, uint32_t index, const std::string &name)
{
	meta.at(id).members.resize(max(meta[id].members.size(), size_t(index) + 1));

	auto &str = meta.at(id).members[index].alias;
	str.clear();
	if (name.empty())
		return;

	// Reserved for unnamed members.
	if (name[0] == '_' && name.size() >= 3 && name[1] == 'm' && isdigit(name[2]))
		return;

	str = ensure_valid_identifier(name, true);
}

const std::string &Compiler::get_member_name(uint32_t id, uint32_t index) const
{
	auto &m = meta.at(id);
	if (index >= m.members.size())
	{
		static string empty;
		return empty;
	}

	return m.members[index].alias;
}

void Compiler::set_member_qualified_name(uint32_t type_id, uint32_t index, const std::string &name)
{
	meta.at(type_id).members.resize(max(meta[type_id].members.size(), size_t(index) + 1));
	meta.at(type_id).members[index].qualified_alias = name;
}

const std::string &Compiler::get_member_qualified_name(uint32_t type_id, uint32_t index) const
{
	const static string empty;

	auto &m = meta.at(type_id);
	if (index < m.members.size())
		return m.members[index].qualified_alias;
	else
		return empty;
}

uint32_t Compiler::get_member_decoration(uint32_t id, uint32_t index, Decoration decoration) const
{
	auto &m = meta.at(id);
	if (index >= m.members.size())
		return 0;

	auto &dec = m.members[index];
	if (!(dec.decoration_flags & (1ull << decoration)))
		return 0;

	switch (decoration)
	{
	case DecorationBuiltIn:
		return dec.builtin_type;
	case DecorationLocation:
		return dec.location;
	case DecorationBinding:
		return dec.binding;
	case DecorationOffset:
		return dec.offset;
	case DecorationSpecId:
		return dec.spec_id;
	default:
		return 1;
	}
}

uint64_t Compiler::get_member_decoration_mask(uint32_t id, uint32_t index) const
{
	auto &m = meta.at(id);
	if (index >= m.members.size())
		return 0;

	return m.members[index].decoration_flags;
}

bool Compiler::has_member_decoration(uint32_t id, uint32_t index, Decoration decoration) const
{
	return get_member_decoration_mask(id, index) & (1ull << decoration);
}

void Compiler::unset_member_decoration(uint32_t id, uint32_t index, Decoration decoration)
{
	auto &m = meta.at(id);
	if (index >= m.members.size())
		return;

	auto &dec = m.members[index];

	dec.decoration_flags &= ~(1ull << decoration);
	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = false;
		break;

	case DecorationLocation:
		dec.location = 0;
		break;

	case DecorationOffset:
		dec.offset = 0;
		break;

	case DecorationSpecId:
		dec.spec_id = 0;
		break;

	default:
		break;
	}
}

void Compiler::set_decoration(uint32_t id, Decoration decoration, uint32_t argument)
{
	auto &dec = meta.at(id).decoration;
	dec.decoration_flags |= 1ull << decoration;

	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = true;
		dec.builtin_type = static_cast<BuiltIn>(argument);
		break;

	case DecorationLocation:
		dec.location = argument;
		break;

	case DecorationOffset:
		dec.offset = argument;
		break;

	case DecorationArrayStride:
		dec.array_stride = argument;
		break;

	case DecorationMatrixStride:
		dec.matrix_stride = argument;
		break;

	case DecorationBinding:
		dec.binding = argument;
		break;

	case DecorationDescriptorSet:
		dec.set = argument;
		break;

	case DecorationInputAttachmentIndex:
		dec.input_attachment = argument;
		break;

	case DecorationSpecId:
		dec.spec_id = argument;
		break;

	default:
		break;
	}
}

StorageClass Compiler::get_storage_class(uint32_t id) const
{
	return get<SPIRVariable>(id).storage;
}

const std::string &Compiler::get_name(uint32_t id) const
{
	return meta.at(id).decoration.alias;
}

const std::string Compiler::get_fallback_name(uint32_t id) const
{
	return join("_", id);
}

const std::string Compiler::get_block_fallback_name(uint32_t id) const
{
	auto &var = get<SPIRVariable>(id);
	if (get_name(id).empty())
		return join("_", get<SPIRType>(var.basetype).self, "_", id);
	else
		return get_name(id);
}

uint64_t Compiler::get_decoration_mask(uint32_t id) const
{
	auto &dec = meta.at(id).decoration;
	return dec.decoration_flags;
}

bool Compiler::has_decoration(uint32_t id, Decoration decoration) const
{
	return get_decoration_mask(id) & (1ull << decoration);
}

uint32_t Compiler::get_decoration(uint32_t id, Decoration decoration) const
{
	auto &dec = meta.at(id).decoration;
	if (!(dec.decoration_flags & (1ull << decoration)))
		return 0;

	switch (decoration)
	{
	case DecorationBuiltIn:
		return dec.builtin_type;
	case DecorationLocation:
		return dec.location;
	case DecorationOffset:
		return dec.offset;
	case DecorationBinding:
		return dec.binding;
	case DecorationDescriptorSet:
		return dec.set;
	case DecorationInputAttachmentIndex:
		return dec.input_attachment;
	case DecorationSpecId:
		return dec.spec_id;
	case DecorationArrayStride:
		return dec.array_stride;
	case DecorationMatrixStride:
		return dec.matrix_stride;
	default:
		return 1;
	}
}

void Compiler::unset_decoration(uint32_t id, Decoration decoration)
{
	auto &dec = meta.at(id).decoration;
	dec.decoration_flags &= ~(1ull << decoration);
	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = false;
		break;

	case DecorationLocation:
		dec.location = 0;
		break;

	case DecorationOffset:
		dec.offset = 0;
		break;

	case DecorationBinding:
		dec.binding = 0;
		break;

	case DecorationDescriptorSet:
		dec.set = 0;
		break;

	case DecorationInputAttachmentIndex:
		dec.input_attachment = 0;
		break;

	case DecorationSpecId:
		dec.spec_id = 0;
		break;

	default:
		break;
	}
}

bool Compiler::get_binary_offset_for_decoration(uint32_t id, spv::Decoration decoration, uint32_t &word_offset) const
{
	auto &word_offsets = meta.at(id).decoration_word_offset;
	auto itr = word_offsets.find(decoration);
	if (itr == end(word_offsets))
		return false;

	word_offset = itr->second;
	return true;
}

void Compiler::parse(const Instruction &instruction)
{
	auto ops = stream(instruction);
	auto op = static_cast<Op>(instruction.op);
	uint32_t length = instruction.length;

	switch (op)
	{
	case OpMemoryModel:
	case OpSourceExtension:
	case OpNop:
	case OpLine:
	case OpNoLine:
	case OpString:
		break;

	case OpSource:
	{
		auto lang = static_cast<SourceLanguage>(ops[0]);
		switch (lang)
		{
		case SourceLanguageESSL:
			source.es = true;
			source.version = ops[1];
			source.known = true;
			source.hlsl = false;
			break;

		case SourceLanguageGLSL:
			source.es = false;
			source.version = ops[1];
			source.known = true;
			source.hlsl = false;
			break;

		case SourceLanguageHLSL:
			// For purposes of cross-compiling, this is GLSL 450.
			source.es = false;
			source.version = 450;
			source.known = true;
			source.hlsl = true;
			break;

		default:
			source.known = false;
			break;
		}
		break;
	}

	case OpUndef:
	{
		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		set<SPIRUndef>(id, result_type);
		break;
	}

	case OpCapability:
	{
		uint32_t cap = ops[0];
		if (cap == CapabilityKernel)
			SPIRV_CROSS_THROW("Kernel capability not supported.");

		declared_capabilities.push_back(static_cast<Capability>(ops[0]));
		break;
	}

	case OpExtension:
	{
		auto ext = extract_string(spirv, instruction.offset);
		declared_extensions.push_back(move(ext));
		break;
	}

	case OpExtInstImport:
	{
		uint32_t id = ops[0];
		auto ext = extract_string(spirv, instruction.offset + 1);
		if (ext == "GLSL.std.450")
			set<SPIRExtension>(id, SPIRExtension::GLSL);
		else if (ext == "SPV_AMD_shader_ballot")
			set<SPIRExtension>(id, SPIRExtension::SPV_AMD_shader_ballot);
		else if (ext == "SPV_AMD_shader_explicit_vertex_parameter")
			set<SPIRExtension>(id, SPIRExtension::SPV_AMD_shader_explicit_vertex_parameter);
		else if (ext == "SPV_AMD_shader_trinary_minmax")
			set<SPIRExtension>(id, SPIRExtension::SPV_AMD_shader_trinary_minmax);
		else if (ext == "SPV_AMD_gcn_shader")
			set<SPIRExtension>(id, SPIRExtension::SPV_AMD_gcn_shader);
		else
			set<SPIRExtension>(id, SPIRExtension::Unsupported);

		// Other SPIR-V extensions currently not supported.

		break;
	}

	case OpEntryPoint:
	{
		auto itr =
		    entry_points.insert(make_pair(ops[1], SPIREntryPoint(ops[1], static_cast<ExecutionModel>(ops[0]),
		                                                         extract_string(spirv, instruction.offset + 2))));
		auto &e = itr.first->second;

		// Strings need nul-terminator and consume the whole word.
		uint32_t strlen_words = uint32_t((e.name.size() + 1 + 3) >> 2);
		e.interface_variables.insert(end(e.interface_variables), ops + strlen_words + 2, ops + instruction.length);

		// Set the name of the entry point in case OpName is not provided later
		set_name(ops[1], e.name);

		// If we don't have an entry, make the first one our "default".
		if (!entry_point)
			entry_point = ops[1];
		break;
	}

	case OpExecutionMode:
	{
		auto &execution = entry_points[ops[0]];
		auto mode = static_cast<ExecutionMode>(ops[1]);
		execution.flags |= 1ull << mode;

		switch (mode)
		{
		case ExecutionModeInvocations:
			execution.invocations = ops[2];
			break;

		case ExecutionModeLocalSize:
			execution.workgroup_size.x = ops[2];
			execution.workgroup_size.y = ops[3];
			execution.workgroup_size.z = ops[4];
			break;

		case ExecutionModeOutputVertices:
			execution.output_vertices = ops[2];
			break;

		default:
			break;
		}
		break;
	}

	case OpName:
	{
		uint32_t id = ops[0];
		set_name(id, extract_string(spirv, instruction.offset + 1));
		break;
	}

	case OpMemberName:
	{
		uint32_t id = ops[0];
		uint32_t member = ops[1];
		set_member_name(id, member, extract_string(spirv, instruction.offset + 2));
		break;
	}

	case OpDecorate:
	{
		uint32_t id = ops[0];

		auto decoration = static_cast<Decoration>(ops[1]);
		if (length >= 3)
		{
			meta[id].decoration_word_offset[decoration] = uint32_t(&ops[2] - spirv.data());
			set_decoration(id, decoration, ops[2]);
		}
		else
			set_decoration(id, decoration);

		break;
	}

	case OpMemberDecorate:
	{
		uint32_t id = ops[0];
		uint32_t member = ops[1];
		auto decoration = static_cast<Decoration>(ops[2]);
		if (length >= 4)
			set_member_decoration(id, member, decoration, ops[3]);
		else
			set_member_decoration(id, member, decoration);
		break;
	}

	// Build up basic types.
	case OpTypeVoid:
	{
		uint32_t id = ops[0];
		auto &type = set<SPIRType>(id);
		type.basetype = SPIRType::Void;
		break;
	}

	case OpTypeBool:
	{
		uint32_t id = ops[0];
		auto &type = set<SPIRType>(id);
		type.basetype = SPIRType::Boolean;
		type.width = 1;
		break;
	}

	case OpTypeFloat:
	{
		uint32_t id = ops[0];
		uint32_t width = ops[1];
		auto &type = set<SPIRType>(id);
		type.basetype = width > 32 ? SPIRType::Double : SPIRType::Float;
		type.width = width;
		break;
	}

	case OpTypeInt:
	{
		uint32_t id = ops[0];
		uint32_t width = ops[1];
		auto &type = set<SPIRType>(id);
		type.basetype =
		    ops[2] ? (width > 32 ? SPIRType::Int64 : SPIRType::Int) : (width > 32 ? SPIRType::UInt64 : SPIRType::UInt);
		type.width = width;
		break;
	}

	// Build composite types by "inheriting".
	// NOTE: The self member is also copied! For pointers and array modifiers this is a good thing
	// since we can refer to decorations on pointee classes which is needed for UBO/SSBO, I/O blocks in geometry/tess etc.
	case OpTypeVector:
	{
		uint32_t id = ops[0];
		uint32_t vecsize = ops[2];

		auto &base = get<SPIRType>(ops[1]);
		auto &vecbase = set<SPIRType>(id);

		vecbase = base;
		vecbase.vecsize = vecsize;
		vecbase.self = id;
		vecbase.parent_type = ops[1];
		break;
	}

	case OpTypeMatrix:
	{
		uint32_t id = ops[0];
		uint32_t colcount = ops[2];

		auto &base = get<SPIRType>(ops[1]);
		auto &matrixbase = set<SPIRType>(id);

		matrixbase = base;
		matrixbase.columns = colcount;
		matrixbase.self = id;
		matrixbase.parent_type = ops[1];
		break;
	}

	case OpTypeArray:
	{
		uint32_t id = ops[0];
		auto &arraybase = set<SPIRType>(id);

		uint32_t tid = ops[1];
		auto &base = get<SPIRType>(tid);

		arraybase = base;
		arraybase.parent_type = tid;

		uint32_t cid = ops[2];
		mark_used_as_array_length(cid);
		auto *c = maybe_get<SPIRConstant>(cid);
		bool literal = c && !c->specialization;

		arraybase.array_size_literal.push_back(literal);
		arraybase.array.push_back(literal ? c->scalar() : cid);
		// Do NOT set arraybase.self!
		break;
	}

	case OpTypeRuntimeArray:
	{
		uint32_t id = ops[0];

		auto &base = get<SPIRType>(ops[1]);
		auto &arraybase = set<SPIRType>(id);

		arraybase = base;
		arraybase.array.push_back(0);
		arraybase.array_size_literal.push_back(true);
		arraybase.parent_type = ops[1];
		// Do NOT set arraybase.self!
		break;
	}

	case OpTypeImage:
	{
		uint32_t id = ops[0];
		auto &type = set<SPIRType>(id);
		type.basetype = SPIRType::Image;
		type.image.type = ops[1];
		type.image.dim = static_cast<Dim>(ops[2]);
		type.image.depth = ops[3] != 0;
		type.image.arrayed = ops[4] != 0;
		type.image.ms = ops[5] != 0;
		type.image.sampled = ops[6];
		type.image.format = static_cast<ImageFormat>(ops[7]);
		type.image.access = (length >= 9) ? static_cast<AccessQualifier>(ops[8]) : AccessQualifierMax;

		if (type.image.sampled == 0)
			SPIRV_CROSS_THROW("OpTypeImage Sampled parameter must not be zero.");

		break;
	}

	case OpTypeSampledImage:
	{
		uint32_t id = ops[0];
		uint32_t imagetype = ops[1];
		auto &type = set<SPIRType>(id);
		type = get<SPIRType>(imagetype);
		type.basetype = SPIRType::SampledImage;
		type.self = id;
		break;
	}

	case OpTypeSampler:
	{
		uint32_t id = ops[0];
		auto &type = set<SPIRType>(id);
		type.basetype = SPIRType::Sampler;
		break;
	}

	case OpTypePointer:
	{
		uint32_t id = ops[0];

		auto &base = get<SPIRType>(ops[2]);
		auto &ptrbase = set<SPIRType>(id);

		ptrbase = base;
		if (ptrbase.pointer)
			SPIRV_CROSS_THROW("Cannot make pointer-to-pointer type.");
		ptrbase.pointer = true;
		ptrbase.storage = static_cast<StorageClass>(ops[1]);

		if (ptrbase.storage == StorageClassAtomicCounter)
			ptrbase.basetype = SPIRType::AtomicCounter;

		ptrbase.parent_type = ops[2];

		// Do NOT set ptrbase.self!
		break;
	}

	case OpTypeStruct:
	{
		uint32_t id = ops[0];
		auto &type = set<SPIRType>(id);
		type.basetype = SPIRType::Struct;
		for (uint32_t i = 1; i < length; i++)
			type.member_types.push_back(ops[i]);

		// Check if we have seen this struct type before, with just different
		// decorations.
		//
		// Add workaround for issue #17 as well by looking at OpName for the struct
		// types, which we shouldn't normally do.
		// We should not normally have to consider type aliases like this to begin with
		// however ... glslang issues #304, #307 cover this.

		// For stripped names, never consider struct type aliasing.
		// We risk declaring the same struct multiple times, but type-punning is not allowed
		// so this is safe.
		bool consider_aliasing = !get_name(type.self).empty();
		if (consider_aliasing)
		{
			for (auto &other : global_struct_cache)
			{
				if (get_name(type.self) == get_name(other) &&
				    types_are_logically_equivalent(type, get<SPIRType>(other)))
				{
					type.type_alias = other;
					break;
				}
			}

			if (type.type_alias == 0)
				global_struct_cache.push_back(id);
		}
		break;
	}

	case OpTypeFunction:
	{
		uint32_t id = ops[0];
		uint32_t ret = ops[1];

		auto &func = set<SPIRFunctionPrototype>(id, ret);
		for (uint32_t i = 2; i < length; i++)
			func.parameter_types.push_back(ops[i]);
		break;
	}

	// Variable declaration
	// All variables are essentially pointers with a storage qualifier.
	case OpVariable:
	{
		uint32_t type = ops[0];
		uint32_t id = ops[1];
		auto storage = static_cast<StorageClass>(ops[2]);
		uint32_t initializer = length == 4 ? ops[3] : 0;

		if (storage == StorageClassFunction)
		{
			if (!current_function)
				SPIRV_CROSS_THROW("No function currently in scope");
			current_function->add_local_variable(id);
		}
		else if (storage == StorageClassPrivate || storage == StorageClassWorkgroup || storage == StorageClassOutput)
		{
			global_variables.push_back(id);
		}

		auto &var = set<SPIRVariable>(id, type, storage, initializer);

		// hlsl based shaders don't have those decorations. force them and then reset when reading/writing images
		auto &ttype = get<SPIRType>(type);
		if (ttype.basetype == SPIRType::BaseType::Image)
		{
			set_decoration(id, DecorationNonWritable);
			set_decoration(id, DecorationNonReadable);
		}

		if (variable_storage_is_aliased(var))
			aliased_variables.push_back(var.self);

		break;
	}

	// OpPhi
	// OpPhi is a fairly magical opcode.
	// It selects temporary variables based on which parent block we *came from*.
	// In high-level languages we can "de-SSA" by creating a function local, and flush out temporaries to this function-local
	// variable to emulate SSA Phi.
	case OpPhi:
	{
		if (!current_function)
			SPIRV_CROSS_THROW("No function currently in scope");
		if (!current_block)
			SPIRV_CROSS_THROW("No block currently in scope");

		uint32_t result_type = ops[0];
		uint32_t id = ops[1];

		// Instead of a temporary, create a new function-wide temporary with this ID instead.
		auto &var = set<SPIRVariable>(id, result_type, spv::StorageClassFunction);
		var.phi_variable = true;

		current_function->add_local_variable(id);

		for (uint32_t i = 2; i + 2 <= length; i += 2)
			current_block->phi_variables.push_back({ ops[i], ops[i + 1], id });
		break;
	}

	// Constants
	case OpSpecConstant:
	case OpConstant:
	{
		uint32_t id = ops[1];
		auto &type = get<SPIRType>(ops[0]);

		if (type.width > 32)
			set<SPIRConstant>(id, ops[0], ops[2] | (uint64_t(ops[3]) << 32), op == OpSpecConstant);
		else
			set<SPIRConstant>(id, ops[0], ops[2], op == OpSpecConstant);
		break;
	}

	case OpSpecConstantFalse:
	case OpConstantFalse:
	{
		uint32_t id = ops[1];
		set<SPIRConstant>(id, ops[0], uint32_t(0), op == OpSpecConstantFalse);
		break;
	}

	case OpSpecConstantTrue:
	case OpConstantTrue:
	{
		uint32_t id = ops[1];
		set<SPIRConstant>(id, ops[0], uint32_t(1), op == OpSpecConstantTrue);
		break;
	}

	case OpConstantNull:
	{
		uint32_t id = ops[1];
		uint32_t type = ops[0];
		make_constant_null(id, type);
		break;
	}

	case OpSpecConstantComposite:
	case OpConstantComposite:
	{
		uint32_t id = ops[1];
		uint32_t type = ops[0];

		auto &ctype = get<SPIRType>(type);

		// We can have constants which are structs and arrays.
		// In this case, our SPIRConstant will be a list of other SPIRConstant ids which we
		// can refer to.
		if (ctype.basetype == SPIRType::Struct || !ctype.array.empty())
		{
			set<SPIRConstant>(id, type, ops + 2, length - 2, op == OpSpecConstantComposite);
		}
		else
		{
			uint32_t elements = length - 2;
			if (elements > 4)
				SPIRV_CROSS_THROW("OpConstantComposite only supports 1, 2, 3 and 4 elements.");

			const SPIRConstant *c[4];
			for (uint32_t i = 0; i < elements; i++)
				c[i] = &get<SPIRConstant>(ops[2 + i]);
			set<SPIRConstant>(id, type, c, elements, op == OpSpecConstantComposite);
		}
		break;
	}

	// Functions
	case OpFunction:
	{
		uint32_t res = ops[0];
		uint32_t id = ops[1];
		// Control
		uint32_t type = ops[3];

		if (current_function)
			SPIRV_CROSS_THROW("Must end a function before starting a new one!");

		current_function = &set<SPIRFunction>(id, res, type);
		break;
	}

	case OpFunctionParameter:
	{
		uint32_t type = ops[0];
		uint32_t id = ops[1];

		if (!current_function)
			SPIRV_CROSS_THROW("Must be in a function!");

		current_function->add_parameter(type, id);
		set<SPIRVariable>(id, type, StorageClassFunction);
		break;
	}

	case OpFunctionEnd:
	{
		if (current_block)
		{
			// Very specific error message, but seems to come up quite often.
			SPIRV_CROSS_THROW(
			    "Cannot end a function before ending the current block.\n"
			    "Likely cause: If this SPIR-V was created from glslang HLSL, make sure the entry point is valid.");
		}
		current_function = nullptr;
		break;
	}

	// Blocks
	case OpLabel:
	{
		// OpLabel always starts a block.
		if (!current_function)
			SPIRV_CROSS_THROW("Blocks cannot exist outside functions!");

		uint32_t id = ops[0];

		current_function->blocks.push_back(id);
		if (!current_function->entry_block)
			current_function->entry_block = id;

		if (current_block)
			SPIRV_CROSS_THROW("Cannot start a block before ending the current block.");

		current_block = &set<SPIRBlock>(id);
		break;
	}

	// Branch instructions end blocks.
	case OpBranch:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");

		uint32_t target = ops[0];
		current_block->terminator = SPIRBlock::Direct;
		current_block->next_block = target;
		current_block = nullptr;
		break;
	}

	case OpBranchConditional:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");

		current_block->condition = ops[0];
		current_block->true_block = ops[1];
		current_block->false_block = ops[2];

		current_block->terminator = SPIRBlock::Select;
		current_block = nullptr;
		break;
	}

	case OpSwitch:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");

		if (current_block->merge == SPIRBlock::MergeNone)
			SPIRV_CROSS_THROW("Switch statement is not structured");

		current_block->terminator = SPIRBlock::MultiSelect;

		current_block->condition = ops[0];
		current_block->default_block = ops[1];

		for (uint32_t i = 2; i + 2 <= length; i += 2)
			current_block->cases.push_back({ ops[i], ops[i + 1] });

		// If we jump to next block, make it break instead since we're inside a switch case block at that point.
		multiselect_merge_targets.insert(current_block->next_block);

		current_block = nullptr;
		break;
	}

	case OpKill:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");
		current_block->terminator = SPIRBlock::Kill;
		current_block = nullptr;
		break;
	}

	case OpReturn:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");
		current_block->terminator = SPIRBlock::Return;
		current_block = nullptr;
		break;
	}

	case OpReturnValue:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");
		current_block->terminator = SPIRBlock::Return;
		current_block->return_value = ops[0];
		current_block = nullptr;
		break;
	}

	case OpUnreachable:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to end a non-existing block.");
		current_block->terminator = SPIRBlock::Unreachable;
		current_block = nullptr;
		break;
	}

	case OpSelectionMerge:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to modify a non-existing block.");

		current_block->next_block = ops[0];
		current_block->merge = SPIRBlock::MergeSelection;
		selection_merge_targets.insert(current_block->next_block);
		break;
	}

	case OpLoopMerge:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Trying to modify a non-existing block.");

		current_block->merge_block = ops[0];
		current_block->continue_block = ops[1];
		current_block->merge = SPIRBlock::MergeLoop;

		loop_blocks.insert(current_block->self);
		loop_merge_targets.insert(current_block->merge_block);

		continue_block_to_loop_header[current_block->continue_block] = current_block->self;

		// Don't add loop headers to continue blocks,
		// which would make it impossible branch into the loop header since
		// they are treated as continues.
		if (current_block->continue_block != current_block->self)
			continue_blocks.insert(current_block->continue_block);
		break;
	}

	case OpSpecConstantOp:
	{
		if (length < 3)
			SPIRV_CROSS_THROW("OpSpecConstantOp not enough arguments.");

		uint32_t result_type = ops[0];
		uint32_t id = ops[1];
		auto spec_op = static_cast<Op>(ops[2]);

		set<SPIRConstantOp>(id, result_type, spec_op, ops + 3, length - 3);
		break;
	}

	// Actual opcodes.
	default:
	{
		if (!current_block)
			SPIRV_CROSS_THROW("Currently no block to insert opcode.");

		current_block->ops.push_back(instruction);
		break;
	}
	}
}

bool Compiler::block_is_loop_candidate(const SPIRBlock &block, SPIRBlock::Method method) const
{
	// Tried and failed.
	if (block.disable_block_optimization || block.complex_continue)
		return false;

	if (method == SPIRBlock::MergeToSelectForLoop)
	{
		// Try to detect common for loop pattern
		// which the code backend can use to create cleaner code.
		// for(;;) { if (cond) { some_body; } else { break; } }
		// is the pattern we're looking for.
		bool ret = block.terminator == SPIRBlock::Select && block.merge == SPIRBlock::MergeLoop &&
		           block.true_block != block.merge_block && block.true_block != block.self &&
		           block.false_block == block.merge_block;

		// If we have OpPhi which depends on branches which came from our own block,
		// we need to flush phi variables in else block instead of a trivial break,
		// so we cannot assume this is a for loop candidate.
		if (ret)
		{
			for (auto &phi : block.phi_variables)
				if (phi.parent == block.self)
					return false;

			auto *merge = maybe_get<SPIRBlock>(block.merge_block);
			if (merge)
				for (auto &phi : merge->phi_variables)
					if (phi.parent == block.self)
						return false;
		}
		return ret;
	}
	else if (method == SPIRBlock::MergeToDirectForLoop)
	{
		// Empty loop header that just sets up merge target
		// and branches to loop body.
		bool ret = block.terminator == SPIRBlock::Direct && block.merge == SPIRBlock::MergeLoop && block.ops.empty();

		if (!ret)
			return false;

		auto &child = get<SPIRBlock>(block.next_block);
		ret = child.terminator == SPIRBlock::Select && child.merge == SPIRBlock::MergeNone &&
		      child.false_block == block.merge_block && child.true_block != block.merge_block &&
		      child.true_block != block.self;

		// If we have OpPhi which depends on branches which came from our own block,
		// we need to flush phi variables in else block instead of a trivial break,
		// so we cannot assume this is a for loop candidate.
		if (ret)
		{
			for (auto &phi : block.phi_variables)
				if (phi.parent == block.self || phi.parent == child.self)
					return false;

			for (auto &phi : child.phi_variables)
				if (phi.parent == block.self)
					return false;

			auto *merge = maybe_get<SPIRBlock>(block.merge_block);
			if (merge)
				for (auto &phi : merge->phi_variables)
					if (phi.parent == block.self || phi.parent == child.false_block)
						return false;
		}

		return ret;
	}
	else
		return false;
}

bool Compiler::block_is_outside_flow_control_from_block(const SPIRBlock &from, const SPIRBlock &to)
{
	auto *start = &from;

	if (start->self == to.self)
		return true;

	// Break cycles.
	if (is_continue(start->self))
		return false;

	// If our select block doesn't merge, we must break or continue in these blocks,
	// so if continues occur branchless within these blocks, consider them branchless as well.
	// This is typically used for loop control.
	if (start->terminator == SPIRBlock::Select && start->merge == SPIRBlock::MergeNone &&
	    (block_is_outside_flow_control_from_block(get<SPIRBlock>(start->true_block), to) ||
	     block_is_outside_flow_control_from_block(get<SPIRBlock>(start->false_block), to)))
	{
		return true;
	}
	else if (start->merge_block && block_is_outside_flow_control_from_block(get<SPIRBlock>(start->merge_block), to))
	{
		return true;
	}
	else if (start->next_block && block_is_outside_flow_control_from_block(get<SPIRBlock>(start->next_block), to))
	{
		return true;
	}
	else
		return false;
}

bool Compiler::execution_is_noop(const SPIRBlock &from, const SPIRBlock &to) const
{
	if (!execution_is_branchless(from, to))
		return false;

	auto *start = &from;
	for (;;)
	{
		if (start->self == to.self)
			return true;

		if (!start->ops.empty())
			return false;

		start = &get<SPIRBlock>(start->next_block);
	}
}

bool Compiler::execution_is_branchless(const SPIRBlock &from, const SPIRBlock &to) const
{
	auto *start = &from;
	for (;;)
	{
		if (start->self == to.self)
			return true;

		if (start->terminator == SPIRBlock::Direct && start->merge == SPIRBlock::MergeNone)
			start = &get<SPIRBlock>(start->next_block);
		else
			return false;
	}
}

SPIRBlock::ContinueBlockType Compiler::continue_block_type(const SPIRBlock &block) const
{
	// The block was deemed too complex during code emit, pick conservative fallback paths.
	if (block.complex_continue)
		return SPIRBlock::ComplexLoop;

	// In older glslang output continue block can be equal to the loop header.
	// In this case, execution is clearly branchless, so just assume a while loop header here.
	if (block.merge == SPIRBlock::MergeLoop)
		return SPIRBlock::WhileLoop;

	auto &dominator = get<SPIRBlock>(block.loop_dominator);

	if (execution_is_noop(block, dominator))
		return SPIRBlock::WhileLoop;
	else if (execution_is_branchless(block, dominator))
		return SPIRBlock::ForLoop;
	else
	{
		if (block.merge == SPIRBlock::MergeNone && block.terminator == SPIRBlock::Select &&
		    block.true_block == dominator.self && block.false_block == dominator.merge_block)
		{
			return SPIRBlock::DoWhileLoop;
		}
		else
			return SPIRBlock::ComplexLoop;
	}
}

bool Compiler::traverse_all_reachable_opcodes(const SPIRBlock &block, OpcodeHandler &handler) const
{
	handler.set_current_block(block);

	// Ideally, perhaps traverse the CFG instead of all blocks in order to eliminate dead blocks,
	// but this shouldn't be a problem in practice unless the SPIR-V is doing insane things like recursing
	// inside dead blocks ...
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		if (!handler.handle(op, ops, i.length))
			return false;

		if (op == OpFunctionCall)
		{
			auto &func = get<SPIRFunction>(ops[2]);
			if (handler.follow_function_call(func))
			{
				if (!handler.begin_function_scope(ops, i.length))
					return false;
				if (!traverse_all_reachable_opcodes(get<SPIRFunction>(ops[2]), handler))
					return false;
				if (!handler.end_function_scope(ops, i.length))
					return false;
			}
		}
	}

	return true;
}

bool Compiler::traverse_all_reachable_opcodes(const SPIRFunction &func, OpcodeHandler &handler) const
{
	for (auto block : func.blocks)
		if (!traverse_all_reachable_opcodes(get<SPIRBlock>(block), handler))
			return false;

	return true;
}

uint32_t Compiler::type_struct_member_offset(const SPIRType &type, uint32_t index) const
{
	// Decoration must be set in valid SPIR-V, otherwise throw.
	auto &dec = meta[type.self].members.at(index);
	if (dec.decoration_flags & (1ull << DecorationOffset))
		return dec.offset;
	else
		SPIRV_CROSS_THROW("Struct member does not have Offset set.");
}

uint32_t Compiler::type_struct_member_array_stride(const SPIRType &type, uint32_t index) const
{
	// Decoration must be set in valid SPIR-V, otherwise throw.
	// ArrayStride is part of the array type not OpMemberDecorate.
	auto &dec = meta[type.member_types[index]].decoration;
	if (dec.decoration_flags & (1ull << DecorationArrayStride))
		return dec.array_stride;
	else
		SPIRV_CROSS_THROW("Struct member does not have ArrayStride set.");
}

uint32_t Compiler::type_struct_member_matrix_stride(const SPIRType &type, uint32_t index) const
{
	// Decoration must be set in valid SPIR-V, otherwise throw.
	// MatrixStride is part of OpMemberDecorate.
	auto &dec = meta[type.self].members[index];
	if (dec.decoration_flags & (1ull << DecorationMatrixStride))
		return dec.matrix_stride;
	else
		SPIRV_CROSS_THROW("Struct member does not have MatrixStride set.");
}

size_t Compiler::get_declared_struct_size(const SPIRType &type) const
{
	uint32_t last = uint32_t(type.member_types.size() - 1);
	size_t offset = type_struct_member_offset(type, last);
	size_t size = get_declared_struct_member_size(type, last);
	return offset + size;
}

size_t Compiler::get_declared_struct_member_size(const SPIRType &struct_type, uint32_t index) const
{
	auto flags = get_member_decoration_mask(struct_type.self, index);
	auto &type = get<SPIRType>(struct_type.member_types[index]);

	switch (type.basetype)
	{
	case SPIRType::Unknown:
	case SPIRType::Void:
	case SPIRType::Boolean: // Bools are purely logical, and cannot be used for externally visible types.
	case SPIRType::AtomicCounter:
	case SPIRType::Image:
	case SPIRType::SampledImage:
	case SPIRType::Sampler:
		SPIRV_CROSS_THROW("Querying size for object with opaque size.");

	default:
		break;
	}

	if (!type.array.empty())
	{
		// For arrays, we can use ArrayStride to get an easy check.
		bool array_size_literal = type.array_size_literal.back();
		uint32_t array_size = array_size_literal ? type.array.back() : get<SPIRConstant>(type.array.back()).scalar();
		return type_struct_member_array_stride(struct_type, index) * array_size;
	}
	else if (type.basetype == SPIRType::Struct)
	{
		return get_declared_struct_size(type);
	}
	else
	{
		unsigned vecsize = type.vecsize;
		unsigned columns = type.columns;

		// Vectors.
		if (columns == 1)
		{
			size_t component_size = type.width / 8;
			return vecsize * component_size;
		}
		else
		{
			uint32_t matrix_stride = type_struct_member_matrix_stride(struct_type, index);

			// Per SPIR-V spec, matrices must be tightly packed and aligned up for vec3 accesses.
			if (flags & (1ull << DecorationRowMajor))
				return matrix_stride * vecsize;
			else if (flags & (1ull << DecorationColMajor))
				return matrix_stride * columns;
			else
				SPIRV_CROSS_THROW("Either row-major or column-major must be declared for matrices.");
		}
	}
}

bool Compiler::BufferAccessHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	if (opcode != OpAccessChain && opcode != OpInBoundsAccessChain)
		return true;

	// Invalid SPIR-V.
	if (length < 4)
		return false;

	if (args[2] != id)
		return true;

	// Don't bother traversing the entire access chain tree yet.
	// If we access a struct member, assume we access the entire member.
	uint32_t index = compiler.get<SPIRConstant>(args[3]).scalar();

	// Seen this index already.
	if (seen.find(index) != end(seen))
		return true;
	seen.insert(index);

	auto &type = compiler.expression_type(id);
	uint32_t offset = compiler.type_struct_member_offset(type, index);

	size_t range;
	// If we have another member in the struct, deduce the range by looking at the next member.
	// This is okay since structs in SPIR-V can have padding, but Offset decoration must be
	// monotonically increasing.
	// Of course, this doesn't take into account if the SPIR-V for some reason decided to add
	// very large amounts of padding, but that's not really a big deal.
	if (index + 1 < type.member_types.size())
	{
		range = compiler.type_struct_member_offset(type, index + 1) - offset;
	}
	else
	{
		// No padding, so just deduce it from the size of the member directly.
		range = compiler.get_declared_struct_member_size(type, index);
	}

	ranges.push_back({ index, offset, range });
	return true;
}

std::vector<BufferRange> Compiler::get_active_buffer_ranges(uint32_t id) const
{
	std::vector<BufferRange> ranges;
	BufferAccessHandler handler(*this, ranges, id);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);
	return ranges;
}

// Increase the number of IDs by the specified incremental amount.
// Returns the value of the first ID available for use in the expanded bound.
uint32_t Compiler::increase_bound_by(uint32_t incr_amount)
{
	auto curr_bound = ids.size();
	auto new_bound = curr_bound + incr_amount;
	ids.resize(new_bound);
	meta.resize(new_bound);
	return uint32_t(curr_bound);
}

bool Compiler::types_are_logically_equivalent(const SPIRType &a, const SPIRType &b) const
{
	if (a.basetype != b.basetype)
		return false;
	if (a.width != b.width)
		return false;
	if (a.vecsize != b.vecsize)
		return false;
	if (a.columns != b.columns)
		return false;
	if (a.array.size() != b.array.size())
		return false;

	size_t array_count = a.array.size();
	if (array_count && memcmp(a.array.data(), b.array.data(), array_count * sizeof(uint32_t)) != 0)
		return false;

	if (a.basetype == SPIRType::Image || a.basetype == SPIRType::SampledImage)
	{
		if (memcmp(&a.image, &b.image, sizeof(SPIRType::Image)) != 0)
			return false;
	}

	if (a.member_types.size() != b.member_types.size())
		return false;

	size_t member_types = a.member_types.size();
	for (size_t i = 0; i < member_types; i++)
	{
		if (!types_are_logically_equivalent(get<SPIRType>(a.member_types[i]), get<SPIRType>(b.member_types[i])))
			return false;
	}

	return true;
}

uint64_t Compiler::get_execution_mode_mask() const
{
	return get_entry_point().flags;
}

void Compiler::set_execution_mode(ExecutionMode mode, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	auto &execution = get_entry_point();

	execution.flags |= 1ull << mode;
	switch (mode)
	{
	case ExecutionModeLocalSize:
		execution.workgroup_size.x = arg0;
		execution.workgroup_size.y = arg1;
		execution.workgroup_size.z = arg2;
		break;

	case ExecutionModeInvocations:
		execution.invocations = arg0;
		break;

	case ExecutionModeOutputVertices:
		execution.output_vertices = arg0;
		break;

	default:
		break;
	}
}

void Compiler::unset_execution_mode(ExecutionMode mode)
{
	auto &execution = get_entry_point();
	execution.flags &= ~(1ull << mode);
}

uint32_t Compiler::get_work_group_size_specialization_constants(SpecializationConstant &x, SpecializationConstant &y,
                                                                SpecializationConstant &z) const
{
	auto &execution = get_entry_point();
	x = { 0, 0 };
	y = { 0, 0 };
	z = { 0, 0 };

	if (execution.workgroup_size.constant != 0)
	{
		auto &c = get<SPIRConstant>(execution.workgroup_size.constant);

		if (c.m.c[0].id[0] != 0)
		{
			x.id = c.m.c[0].id[0];
			x.constant_id = get_decoration(c.m.c[0].id[0], DecorationSpecId);
		}

		if (c.m.c[0].id[1] != 0)
		{
			y.id = c.m.c[0].id[1];
			y.constant_id = get_decoration(c.m.c[0].id[1], DecorationSpecId);
		}

		if (c.m.c[0].id[2] != 0)
		{
			z.id = c.m.c[0].id[2];
			z.constant_id = get_decoration(c.m.c[0].id[2], DecorationSpecId);
		}
	}

	return execution.workgroup_size.constant;
}

uint32_t Compiler::get_execution_mode_argument(spv::ExecutionMode mode, uint32_t index) const
{
	auto &execution = get_entry_point();
	switch (mode)
	{
	case ExecutionModeLocalSize:
		switch (index)
		{
		case 0:
			return execution.workgroup_size.x;
		case 1:
			return execution.workgroup_size.y;
		case 2:
			return execution.workgroup_size.z;
		default:
			return 0;
		}

	case ExecutionModeInvocations:
		return execution.invocations;

	case ExecutionModeOutputVertices:
		return execution.output_vertices;

	default:
		return 0;
	}
}

ExecutionModel Compiler::get_execution_model() const
{
	auto &execution = get_entry_point();
	return execution.model;
}

void Compiler::set_remapped_variable_state(uint32_t id, bool remap_enable)
{
	get<SPIRVariable>(id).remapped_variable = remap_enable;
}

bool Compiler::get_remapped_variable_state(uint32_t id) const
{
	return get<SPIRVariable>(id).remapped_variable;
}

void Compiler::set_subpass_input_remapped_components(uint32_t id, uint32_t components)
{
	get<SPIRVariable>(id).remapped_components = components;
}

uint32_t Compiler::get_subpass_input_remapped_components(uint32_t id) const
{
	return get<SPIRVariable>(id).remapped_components;
}

void Compiler::inherit_expression_dependencies(uint32_t dst, uint32_t source_expression)
{
	// Don't inherit any expression dependencies if the expression in dst
	// is not a forwarded temporary.
	if (forwarded_temporaries.find(dst) == end(forwarded_temporaries) ||
	    forced_temporaries.find(dst) != end(forced_temporaries))
	{
		return;
	}

	auto &e = get<SPIRExpression>(dst);
	auto *s = maybe_get<SPIRExpression>(source_expression);
	if (!s)
		return;

	auto &e_deps = e.expression_dependencies;
	auto &s_deps = s->expression_dependencies;

	// If we depend on a expression, we also depend on all sub-dependencies from source.
	e_deps.push_back(source_expression);
	e_deps.insert(end(e_deps), begin(s_deps), end(s_deps));

	// Eliminate duplicated dependencies.
	e_deps.erase(unique(begin(e_deps), end(e_deps)), end(e_deps));
}

vector<string> Compiler::get_entry_points() const
{
	vector<string> entries;
	for (auto &entry : entry_points)
		entries.push_back(entry.second.orig_name);
	return entries;
}

vector<EntryPoint> Compiler::get_entry_points_and_stages() const
{
	vector<EntryPoint> entries;
	for (auto &entry : entry_points)
		entries.push_back({ entry.second.orig_name, entry.second.model });
	return entries;
}

void Compiler::rename_entry_point(const std::string &old_name, const std::string &new_name)
{
	auto &entry = get_first_entry_point(old_name);
	entry.orig_name = new_name;
	entry.name = new_name;
}

void Compiler::rename_entry_point(const std::string &old_name, const std::string &new_name, spv::ExecutionModel model)
{
	auto &entry = get_entry_point(old_name, model);
	entry.orig_name = new_name;
	entry.name = new_name;
}

void Compiler::set_entry_point(const std::string &name)
{
	auto &entry = get_first_entry_point(name);
	entry_point = entry.self;
}

void Compiler::set_entry_point(const std::string &name, spv::ExecutionModel model)
{
	auto &entry = get_entry_point(name, model);
	entry_point = entry.self;
}

SPIREntryPoint &Compiler::get_entry_point(const std::string &name)
{
	return get_first_entry_point(name);
}

const SPIREntryPoint &Compiler::get_entry_point(const std::string &name) const
{
	return get_first_entry_point(name);
}

SPIREntryPoint &Compiler::get_first_entry_point(const std::string &name)
{
	auto itr =
	    find_if(begin(entry_points), end(entry_points), [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		    return entry.second.orig_name == name;
	    });

	if (itr == end(entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const SPIREntryPoint &Compiler::get_first_entry_point(const std::string &name) const
{
	auto itr =
	    find_if(begin(entry_points), end(entry_points), [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		    return entry.second.orig_name == name;
	    });

	if (itr == end(entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

SPIREntryPoint &Compiler::get_entry_point(const std::string &name, ExecutionModel model)
{
	auto itr =
	    find_if(begin(entry_points), end(entry_points), [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		    return entry.second.orig_name == name && entry.second.model == model;
	    });

	if (itr == end(entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const SPIREntryPoint &Compiler::get_entry_point(const std::string &name, ExecutionModel model) const
{
	auto itr =
	    find_if(begin(entry_points), end(entry_points), [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		    return entry.second.orig_name == name && entry.second.model == model;
	    });

	if (itr == end(entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const string &Compiler::get_cleansed_entry_point_name(const std::string &name) const
{
	return get_first_entry_point(name).name;
}

const string &Compiler::get_cleansed_entry_point_name(const std::string &name, ExecutionModel model) const
{
	return get_entry_point(name, model).name;
}

const SPIREntryPoint &Compiler::get_entry_point() const
{
	return entry_points.find(entry_point)->second;
}

SPIREntryPoint &Compiler::get_entry_point()
{
	return entry_points.find(entry_point)->second;
}

bool Compiler::interface_variable_exists_in_entry_point(uint32_t id) const
{
	auto &var = get<SPIRVariable>(id);
	if (var.storage != StorageClassInput && var.storage != StorageClassOutput &&
	    var.storage != StorageClassUniformConstant)
		SPIRV_CROSS_THROW("Only Input, Output variables and Uniform constants are part of a shader linking interface.");

	// This is to avoid potential problems with very old glslang versions which did
	// not emit input/output interfaces properly.
	// We can assume they only had a single entry point, and single entry point
	// shaders could easily be assumed to use every interface variable anyways.
	if (entry_points.size() <= 1)
		return true;

	auto &execution = get_entry_point();
	return find(begin(execution.interface_variables), end(execution.interface_variables), id) !=
	       end(execution.interface_variables);
}

void Compiler::CombinedImageSamplerHandler::push_remap_parameters(const SPIRFunction &func, const uint32_t *args,
                                                                  uint32_t length)
{
	// If possible, pipe through a remapping table so that parameters know
	// which variables they actually bind to in this scope.
	unordered_map<uint32_t, uint32_t> remapping;
	for (uint32_t i = 0; i < length; i++)
		remapping[func.arguments[i].id] = remap_parameter(args[i]);
	parameter_remapping.push(move(remapping));
}

void Compiler::CombinedImageSamplerHandler::pop_remap_parameters()
{
	parameter_remapping.pop();
}

uint32_t Compiler::CombinedImageSamplerHandler::remap_parameter(uint32_t id)
{
	auto *var = compiler.maybe_get_backing_variable(id);
	if (var)
		id = var->self;

	if (parameter_remapping.empty())
		return id;

	auto &remapping = parameter_remapping.top();
	auto itr = remapping.find(id);
	if (itr != end(remapping))
		return itr->second;
	else
		return id;
}

bool Compiler::CombinedImageSamplerHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &callee = compiler.get<SPIRFunction>(args[2]);
	args += 3;
	length -= 3;
	push_remap_parameters(callee, args, length);
	functions.push(&callee);
	return true;
}

bool Compiler::CombinedImageSamplerHandler::end_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &callee = compiler.get<SPIRFunction>(args[2]);
	args += 3;

	// There are two types of cases we have to handle,
	// a callee might call sampler2D(texture2D, sampler) directly where
	// one or more parameters originate from parameters.
	// Alternatively, we need to provide combined image samplers to our callees,
	// and in this case we need to add those as well.

	pop_remap_parameters();

	// Our callee has now been processed at least once.
	// No point in doing it again.
	callee.do_combined_parameters = false;

	auto &params = functions.top()->combined_parameters;
	functions.pop();
	if (functions.empty())
		return true;

	auto &caller = *functions.top();
	if (caller.do_combined_parameters)
	{
		for (auto &param : params)
		{
			uint32_t image_id = param.global_image ? param.image_id : args[param.image_id];
			uint32_t sampler_id = param.global_sampler ? param.sampler_id : args[param.sampler_id];

			auto *i = compiler.maybe_get_backing_variable(image_id);
			auto *s = compiler.maybe_get_backing_variable(sampler_id);
			if (i)
				image_id = i->self;
			if (s)
				sampler_id = s->self;

			register_combined_image_sampler(caller, image_id, sampler_id, param.depth);
		}
	}

	return true;
}

void Compiler::CombinedImageSamplerHandler::register_combined_image_sampler(SPIRFunction &caller, uint32_t image_id,
                                                                            uint32_t sampler_id, bool depth)
{
	// We now have a texture ID and a sampler ID which will either be found as a global
	// or a parameter in our own function. If both are global, they will not need a parameter,
	// otherwise, add it to our list.
	SPIRFunction::CombinedImageSamplerParameter param = {
		0u, image_id, sampler_id, true, true, depth,
	};

	auto texture_itr = find_if(begin(caller.arguments), end(caller.arguments),
	                           [image_id](const SPIRFunction::Parameter &p) { return p.id == image_id; });
	auto sampler_itr = find_if(begin(caller.arguments), end(caller.arguments),
	                           [sampler_id](const SPIRFunction::Parameter &p) { return p.id == sampler_id; });

	if (texture_itr != end(caller.arguments))
	{
		param.global_image = false;
		param.image_id = uint32_t(texture_itr - begin(caller.arguments));
	}

	if (sampler_itr != end(caller.arguments))
	{
		param.global_sampler = false;
		param.sampler_id = uint32_t(sampler_itr - begin(caller.arguments));
	}

	if (param.global_image && param.global_sampler)
		return;

	auto itr = find_if(begin(caller.combined_parameters), end(caller.combined_parameters),
	                   [&param](const SPIRFunction::CombinedImageSamplerParameter &p) {
		                   return param.image_id == p.image_id && param.sampler_id == p.sampler_id &&
		                          param.global_image == p.global_image && param.global_sampler == p.global_sampler;
	                   });

	if (itr == end(caller.combined_parameters))
	{
		uint32_t id = compiler.increase_bound_by(3);
		auto type_id = id + 0;
		auto ptr_type_id = id + 1;
		auto combined_id = id + 2;
		auto &base = compiler.expression_type(image_id);
		auto &type = compiler.set<SPIRType>(type_id);
		auto &ptr_type = compiler.set<SPIRType>(ptr_type_id);

		type = base;
		type.self = type_id;
		type.basetype = SPIRType::SampledImage;
		type.pointer = false;
		type.storage = StorageClassGeneric;
		type.image.depth = depth;

		ptr_type = type;
		ptr_type.pointer = true;
		ptr_type.storage = StorageClassUniformConstant;

		// Build new variable.
		compiler.set<SPIRVariable>(combined_id, ptr_type_id, StorageClassFunction, 0);

		// Inherit RelaxedPrecision (and potentially other useful flags if deemed relevant).
		auto &new_flags = compiler.meta[combined_id].decoration.decoration_flags;
		auto old_flags = compiler.meta[sampler_id].decoration.decoration_flags;
		new_flags = old_flags & (1ull << DecorationRelaxedPrecision);

		param.id = combined_id;

		compiler.set_name(combined_id,
		                  join("SPIRV_Cross_Combined", compiler.to_name(image_id), compiler.to_name(sampler_id)));

		caller.combined_parameters.push_back(param);
		caller.shadow_arguments.push_back({ ptr_type_id, combined_id, 0u, 0u, true });
	}
}

bool Compiler::DummySamplerForCombinedImageHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	if (need_dummy_sampler)
	{
		// No need to traverse further, we know the result.
		return false;
	}

	switch (opcode)
	{
	case OpLoad:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image =
		    type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer;

		// If not separate image, don't bother.
		if (!separate_image)
			return true;

		uint32_t id = args[1];
		uint32_t ptr = args[2];
		compiler.set<SPIRExpression>(id, "", result_type, true);
		compiler.register_read(id, ptr, true);
		break;
	}

	case OpImageFetch:
	{
		// If we are fetching from a plain OpTypeImage, we must pre-combine with our dummy sampler.
		auto *var = compiler.maybe_get_backing_variable(args[2]);
		if (var)
		{
			auto &type = compiler.get<SPIRType>(var->basetype);
			if (type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer)
				need_dummy_sampler = true;
		}

		break;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	{
		if (length < 3)
			return false;

		auto &type = compiler.get<SPIRType>(args[0]);
		bool separate_image =
		    type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer;
		if (separate_image)
			SPIRV_CROSS_THROW("Attempting to use arrays or structs of separate images. This is not possible to "
			                  "statically remap to plain GLSL.");
		break;
	}

	default:
		break;
	}

	return true;
}

bool Compiler::CombinedImageSamplerHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	// We need to figure out where samplers and images are loaded from, so do only the bare bones compilation we need.
	bool is_fetch = false;

	switch (opcode)
	{
	case OpLoad:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
		bool separate_sampler = type.basetype == SPIRType::Sampler;

		// If not separate image or sampler, don't bother.
		if (!separate_image && !separate_sampler)
			return true;

		uint32_t id = args[1];
		uint32_t ptr = args[2];
		compiler.set<SPIRExpression>(id, "", result_type, true);
		compiler.register_read(id, ptr, true);
		return true;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	{
		if (length < 3)
			return false;

		// Technically, it is possible to have arrays of textures and arrays of samplers and combine them, but this becomes essentially
		// impossible to implement, since we don't know which concrete sampler we are accessing.
		// One potential way is to create a combinatorial explosion where N textures and M samplers are combined into N * M sampler2Ds,
		// but this seems ridiculously complicated for a problem which is easy to work around.
		// Checking access chains like this assumes we don't have samplers or textures inside uniform structs, but this makes no sense.

		auto &type = compiler.get<SPIRType>(args[0]);
		bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
		bool separate_sampler = type.basetype == SPIRType::Sampler;
		if (separate_image)
			SPIRV_CROSS_THROW("Attempting to use arrays or structs of separate images. This is not possible to "
			                  "statically remap to plain GLSL.");
		if (separate_sampler)
			SPIRV_CROSS_THROW(
			    "Attempting to use arrays or structs of separate samplers. This is not possible to statically "
			    "remap to plain GLSL.");
		return true;
	}

	case OpImageFetch:
	{
		// If we are fetching from a plain OpTypeImage, we must pre-combine with our dummy sampler.
		auto *var = compiler.maybe_get_backing_variable(args[2]);
		if (!var)
			return true;

		auto &type = compiler.get<SPIRType>(var->basetype);
		if (type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer)
		{
			if (compiler.dummy_sampler_id == 0)
				SPIRV_CROSS_THROW("texelFetch without sampler was found, but no dummy sampler has been created with "
				                  "build_dummy_sampler_for_combined_images().");

			// Do it outside.
			is_fetch = true;
			break;
		}

		return true;
	}

	case OpSampledImage:
		// Do it outside.
		break;

	default:
		return true;
	}

	if (length < 4)
		return false;

	// Registers sampler2D calls used in case they are parameters so
	// that their callees know which combined image samplers to propagate down the call stack.
	if (!functions.empty())
	{
		auto &callee = *functions.top();
		if (callee.do_combined_parameters)
		{
			uint32_t image_id = args[2];

			auto *image = compiler.maybe_get_backing_variable(image_id);
			if (image)
				image_id = image->self;

			uint32_t sampler_id = is_fetch ? compiler.dummy_sampler_id : args[3];
			auto *sampler = compiler.maybe_get_backing_variable(sampler_id);
			if (sampler)
				sampler_id = sampler->self;

			auto &combined_type = compiler.get<SPIRType>(args[0]);
			register_combined_image_sampler(callee, image_id, sampler_id, combined_type.image.depth);
		}
	}

	// For function calls, we need to remap IDs which are function parameters into global variables.
	// This information is statically known from the current place in the call stack.
	// Function parameters are not necessarily pointers, so if we don't have a backing variable, remapping will know
	// which backing variable the image/sample came from.
	uint32_t image_id = remap_parameter(args[2]);
	uint32_t sampler_id = is_fetch ? compiler.dummy_sampler_id : remap_parameter(args[3]);

	auto itr = find_if(begin(compiler.combined_image_samplers), end(compiler.combined_image_samplers),
	                   [image_id, sampler_id](const CombinedImageSampler &combined) {
		                   return combined.image_id == image_id && combined.sampler_id == sampler_id;
	                   });

	if (itr == end(compiler.combined_image_samplers))
	{
		uint32_t sampled_type;
		if (is_fetch)
		{
			// Have to invent the sampled image type.
			sampled_type = compiler.increase_bound_by(1);
			auto &type = compiler.set<SPIRType>(sampled_type);
			type = compiler.expression_type(args[2]);
			type.self = sampled_type;
			type.basetype = SPIRType::SampledImage;
		}
		else
		{
			sampled_type = args[0];
		}

		auto id = compiler.increase_bound_by(2);
		auto type_id = id + 0;
		auto combined_id = id + 1;

		// Make a new type, pointer to OpTypeSampledImage, so we can make a variable of this type.
		// We will probably have this type lying around, but it doesn't hurt to make duplicates for internal purposes.
		auto &type = compiler.set<SPIRType>(type_id);
		auto &base = compiler.get<SPIRType>(sampled_type);
		type = base;
		type.pointer = true;
		type.storage = StorageClassUniformConstant;

		// Build new variable.
		compiler.set<SPIRVariable>(combined_id, type_id, StorageClassUniformConstant, 0);

		// Inherit RelaxedPrecision (and potentially other useful flags if deemed relevant).
		auto &new_flags = compiler.meta[combined_id].decoration.decoration_flags;
		// Fetch inherits precision from the image, not sampler (there is no sampler).
		auto old_flags = compiler.meta[is_fetch ? image_id : sampler_id].decoration.decoration_flags;
		new_flags = old_flags & (1ull << DecorationRelaxedPrecision);

		compiler.combined_image_samplers.push_back({ combined_id, image_id, sampler_id });
	}

	return true;
}

uint32_t Compiler::build_dummy_sampler_for_combined_images()
{
	DummySamplerForCombinedImageHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);
	if (handler.need_dummy_sampler)
	{
		uint32_t offset = increase_bound_by(3);
		auto type_id = offset + 0;
		auto ptr_type_id = offset + 1;
		auto var_id = offset + 2;

		SPIRType sampler_type;
		auto &sampler = set<SPIRType>(type_id);
		sampler.basetype = SPIRType::Sampler;

		auto &ptr_sampler = set<SPIRType>(ptr_type_id);
		ptr_sampler = sampler;
		ptr_sampler.self = type_id;
		ptr_sampler.storage = StorageClassUniformConstant;
		ptr_sampler.pointer = true;

		set<SPIRVariable>(var_id, ptr_type_id, StorageClassUniformConstant, 0);
		set_name(var_id, "SPIRV_Cross_DummySampler");
		dummy_sampler_id = var_id;
		return var_id;
	}
	else
		return 0;
}

void Compiler::build_combined_image_samplers()
{
	for (auto &id : ids)
	{
		if (id.get_type() == TypeFunction)
		{
			auto &func = id.get<SPIRFunction>();
			func.combined_parameters.clear();
			func.shadow_arguments.clear();
			func.do_combined_parameters = true;
		}
	}

	combined_image_samplers.clear();
	CombinedImageSamplerHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);
}

vector<SpecializationConstant> Compiler::get_specialization_constants() const
{
	vector<SpecializationConstant> spec_consts;
	for (auto &id : ids)
	{
		if (id.get_type() == TypeConstant)
		{
			auto &c = id.get<SPIRConstant>();
			if (c.specialization)
			{
				spec_consts.push_back({ c.self, get_decoration(c.self, DecorationSpecId) });
			}
		}
	}
	return spec_consts;
}

SPIRConstant &Compiler::get_constant(uint32_t id)
{
	return get<SPIRConstant>(id);
}

const SPIRConstant &Compiler::get_constant(uint32_t id) const
{
	return get<SPIRConstant>(id);
}

// Recursively marks any constants referenced by the specified constant instruction as being used
// as an array length. The id must be a constant instruction (SPIRConstant or SPIRConstantOp).
void Compiler::mark_used_as_array_length(uint32_t id)
{
	switch (ids[id].get_type())
	{
	case TypeConstant:
		get<SPIRConstant>(id).is_used_as_array_length = true;
		break;

	case TypeConstantOp:
	{
		auto &cop = get<SPIRConstantOp>(id);
		for (uint32_t arg_id : cop.arguments)
			mark_used_as_array_length(arg_id);
	}

	case TypeUndef:
		return;

	default:
		SPIRV_CROSS_THROW("Array lengths must be a constant instruction (OpConstant.. or OpSpecConstant...).");
	}
}

static bool exists_unaccessed_path_to_return(const CFG &cfg, uint32_t block, const unordered_set<uint32_t> &blocks)
{
	// This block accesses the variable.
	if (blocks.find(block) != end(blocks))
		return false;

	// We are at the end of the CFG.
	if (cfg.get_succeeding_edges(block).empty())
		return true;

	// If any of our successors have a path to the end, there exists a path from block.
	for (auto &succ : cfg.get_succeeding_edges(block))
		if (exists_unaccessed_path_to_return(cfg, succ, blocks))
			return true;

	return false;
}

void Compiler::analyze_parameter_preservation(
    SPIRFunction &entry, const CFG &cfg, const unordered_map<uint32_t, unordered_set<uint32_t>> &variable_to_blocks,
    const unordered_map<uint32_t, unordered_set<uint32_t>> &complete_write_blocks)
{
	for (auto &arg : entry.arguments)
	{
		// Non-pointers are always inputs.
		auto &type = get<SPIRType>(arg.type);
		if (!type.pointer)
			continue;

		// Opaque argument types are always in
		bool potential_preserve;
		switch (type.basetype)
		{
		case SPIRType::Sampler:
		case SPIRType::Image:
		case SPIRType::SampledImage:
		case SPIRType::AtomicCounter:
			potential_preserve = false;
			break;

		default:
			potential_preserve = true;
			break;
		}

		if (!potential_preserve)
			continue;

		auto itr = variable_to_blocks.find(arg.id);
		if (itr == end(variable_to_blocks))
		{
			// Variable is never accessed.
			continue;
		}

		// We have accessed a variable, but there was no complete writes to that variable.
		// We deduce that we must preserve the argument.
		itr = complete_write_blocks.find(arg.id);
		if (itr == end(complete_write_blocks))
		{
			arg.read_count++;
			continue;
		}

		// If there is a path through the CFG where no block completely writes to the variable, the variable will be in an undefined state
		// when the function returns. We therefore need to implicitly preserve the variable in case there are writers in the function.
		// Major case here is if a function is
		// void foo(int &var) { if (cond) var = 10; }
		// Using read/write counts, we will think it's just an out variable, but it really needs to be inout,
		// because if we don't write anything whatever we put into the function must return back to the caller.
		if (exists_unaccessed_path_to_return(cfg, entry.entry_block, itr->second))
			arg.read_count++;
	}
}

void Compiler::analyze_variable_scope(SPIRFunction &entry)
{
	struct AccessHandler : OpcodeHandler
	{
	public:
		AccessHandler(Compiler &compiler_, SPIRFunction &entry_)
		    : compiler(compiler_)
		    , entry(entry_)
		{
		}

		bool follow_function_call(const SPIRFunction &)
		{
			// Only analyze within this function.
			return false;
		}

		void set_current_block(const SPIRBlock &block)
		{
			current_block = &block;

			// If we're branching to a block which uses OpPhi, in GLSL
			// this will be a variable write when we branch,
			// so we need to track access to these variables as well to
			// have a complete picture.
			const auto test_phi = [this, &block](uint32_t to) {
				auto &next = compiler.get<SPIRBlock>(to);
				for (auto &phi : next.phi_variables)
				{
					if (phi.parent == block.self)
					{
						accessed_variables_to_block[phi.function_variable].insert(block.self);
						// Phi variables are also accessed in our target branch block.
						accessed_variables_to_block[phi.function_variable].insert(next.self);

						notify_variable_access(phi.local_variable, block.self);
					}
				}
			};

			switch (block.terminator)
			{
			case SPIRBlock::Direct:
				notify_variable_access(block.condition, block.self);
				test_phi(block.next_block);
				break;

			case SPIRBlock::Select:
				notify_variable_access(block.condition, block.self);
				test_phi(block.true_block);
				test_phi(block.false_block);
				break;

			case SPIRBlock::MultiSelect:
				notify_variable_access(block.condition, block.self);
				for (auto &target : block.cases)
					test_phi(target.block);
				if (block.default_block)
					test_phi(block.default_block);
				break;

			default:
				break;
			}
		}

		void notify_variable_access(uint32_t id, uint32_t block)
		{
			if (id_is_phi_variable(id))
				accessed_variables_to_block[id].insert(block);
			else if (id_is_potential_temporary(id))
				accessed_temporaries_to_block[id].insert(block);
		}

		bool id_is_phi_variable(uint32_t id)
		{
			if (id >= compiler.get_current_id_bound())
				return false;
			auto *var = compiler.maybe_get<SPIRVariable>(id);
			return var && var->phi_variable;
		}

		bool id_is_potential_temporary(uint32_t id)
		{
			if (id >= compiler.get_current_id_bound())
				return false;

			// Temporaries are not created before we start emitting code.
			return compiler.ids[id].empty() || (compiler.ids[id].get_type() == TypeExpression);
		}

		bool handle(spv::Op op, const uint32_t *args, uint32_t length)
		{
			// Keep track of the types of temporaries, so we can hoist them out as necessary.
			uint32_t result_type, result_id;
			if (compiler.instruction_to_result_type(result_type, result_id, op, args, length))
				result_id_to_type[result_id] = result_type;

			switch (op)
			{
			case OpStore:
			{
				if (length < 2)
					return false;

				uint32_t ptr = args[0];
				auto *var = compiler.maybe_get_backing_variable(ptr);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);

				// If we store through an access chain, we have a partial write.
				if (var && var->self == ptr && var->storage == StorageClassFunction)
					complete_write_variables_to_block[var->self].insert(current_block->self);

				// Might try to store a Phi variable here.
				notify_variable_access(args[1], current_block->self);
				break;
			}

			case OpAccessChain:
			case OpInBoundsAccessChain:
			{
				if (length < 3)
					return false;

				uint32_t ptr = args[2];
				auto *var = compiler.maybe_get<SPIRVariable>(ptr);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);

				for (uint32_t i = 3; i < length; i++)
					notify_variable_access(args[i], current_block->self);

				// The result of an access chain is a fixed expression and is not really considered a temporary.
				break;
			}

			case OpCopyMemory:
			{
				if (length < 2)
					return false;

				uint32_t lhs = args[0];
				uint32_t rhs = args[1];
				auto *var = compiler.maybe_get_backing_variable(lhs);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);

				// If we store through an access chain, we have a partial write.
				if (var && var->self == lhs)
					complete_write_variables_to_block[var->self].insert(current_block->self);

				var = compiler.maybe_get_backing_variable(rhs);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);
				break;
			}

			case OpCopyObject:
			{
				if (length < 3)
					return false;

				auto *var = compiler.maybe_get_backing_variable(args[2]);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);

				// Might try to copy a Phi variable here.
				notify_variable_access(args[2], current_block->self);
				break;
			}

			case OpLoad:
			{
				if (length < 3)
					return false;
				uint32_t ptr = args[2];
				auto *var = compiler.maybe_get_backing_variable(ptr);
				if (var && var->storage == StorageClassFunction)
					accessed_variables_to_block[var->self].insert(current_block->self);

				// Loaded value is a temporary.
				notify_variable_access(args[1], current_block->self);
				break;
			}

			case OpFunctionCall:
			{
				if (length < 3)
					return false;

				length -= 3;
				args += 3;
				for (uint32_t i = 0; i < length; i++)
				{
					auto *var = compiler.maybe_get_backing_variable(args[i]);
					if (var && var->storage == StorageClassFunction)
						accessed_variables_to_block[var->self].insert(current_block->self);

					// Cannot easily prove if argument we pass to a function is completely written.
					// Usually, functions write to a dummy variable,
					// which is then copied to in full to the real argument.

					// Might try to copy a Phi variable here.
					notify_variable_access(args[i], current_block->self);
				}

				// Return value may be a temporary.
				notify_variable_access(args[1], current_block->self);
				break;
			}

			case OpExtInst:
			{
				for (uint32_t i = 4; i < length; i++)
					notify_variable_access(args[i], current_block->self);
				notify_variable_access(args[1], current_block->self);
				break;
			}

			case OpArrayLength:
				// Uses literals, but cannot be a phi variable, so ignore.
				break;

				// Atomics shouldn't be able to access function-local variables.
				// Some GLSL builtins access a pointer.

			case OpCompositeInsert:
			case OpVectorShuffle:
				// Specialize for opcode which contains literals.
				for (uint32_t i = 1; i < 4; i++)
					notify_variable_access(args[i], current_block->self);
				break;

			case OpCompositeExtract:
				// Specialize for opcode which contains literals.
				for (uint32_t i = 1; i < 3; i++)
					notify_variable_access(args[i], current_block->self);
				break;

			default:
			{
				// Rather dirty way of figuring out where Phi variables are used.
				// As long as only IDs are used, we can scan through instructions and try to find any evidence that
				// the ID of a variable has been used.
				// There are potential false positives here where a literal is used in-place of an ID,
				// but worst case, it does not affect the correctness of the compile.
				// Exhaustive analysis would be better here, but it's not worth it for now.
				for (uint32_t i = 0; i < length; i++)
					notify_variable_access(args[i], current_block->self);
				break;
			}
			}
			return true;
		}

		Compiler &compiler;
		SPIRFunction &entry;
		std::unordered_map<uint32_t, std::unordered_set<uint32_t>> accessed_variables_to_block;
		std::unordered_map<uint32_t, std::unordered_set<uint32_t>> accessed_temporaries_to_block;
		std::unordered_map<uint32_t, uint32_t> result_id_to_type;
		std::unordered_map<uint32_t, std::unordered_set<uint32_t>> complete_write_variables_to_block;
		const SPIRBlock *current_block = nullptr;
	} handler(*this, entry);

	// First, we map out all variable access within a function.
	// Essentially a map of block -> { variables accessed in the basic block }
	this->traverse_all_reachable_opcodes(entry, handler);

	// Compute the control flow graph for this function.
	CFG cfg(*this, entry);

	// Analyze if there are parameters which need to be implicitly preserved with an "in" qualifier.
	this->analyze_parameter_preservation(entry, cfg, handler.accessed_variables_to_block,
	                                     handler.complete_write_variables_to_block);

	unordered_map<uint32_t, uint32_t> potential_loop_variables;

	// For each variable which is statically accessed.
	for (auto &var : handler.accessed_variables_to_block)
	{
		DominatorBuilder builder(cfg);
		auto &blocks = var.second;
		auto &type = this->expression_type(var.first);

		// Figure out which block is dominating all accesses of those variables.
		for (auto &block : blocks)
		{
			// If we're accessing a variable inside a continue block, this variable might be a loop variable.
			// We can only use loop variables with scalars, as we cannot track static expressions for vectors.
			if (this->is_continue(block))
			{
				// Potentially awkward case to check for.
				// We might have a variable inside a loop, which is touched by the continue block,
				// but is not actually a loop variable.
				// The continue block is dominated by the inner part of the loop, which does not make sense in high-level
				// language output because it will be declared before the body,
				// so we will have to lift the dominator up to the relevant loop header instead.
				builder.add_block(this->continue_block_to_loop_header[block]);

				if (type.vecsize == 1 && type.columns == 1)
				{
					// The variable is used in multiple continue blocks, this is not a loop
					// candidate, signal that by setting block to -1u.
					auto &potential = potential_loop_variables[var.first];

					if (potential == 0)
						potential = block;
					else
						potential = ~(0u);
				}
			}
			builder.add_block(block);
		}

		builder.lift_continue_block_dominator();

		// Add it to a per-block list of variables.
		uint32_t dominating_block = builder.get_dominator();

		// If all blocks here are dead code, this will be 0, so the variable in question
		// will be completely eliminated.
		if (dominating_block)
		{
			auto &block = this->get<SPIRBlock>(dominating_block);
			block.dominated_variables.push_back(var.first);
			this->get<SPIRVariable>(var.first).dominator = dominating_block;
		}
	}

	for (auto &var : handler.accessed_temporaries_to_block)
	{
		auto itr = handler.result_id_to_type.find(var.first);

		if (itr == end(handler.result_id_to_type))
		{
			// We found a false positive ID being used, ignore.
			// This should probably be an assert.
			continue;
		}

		DominatorBuilder builder(cfg);

		// Figure out which block is dominating all accesses of those temporaries.
		auto &blocks = var.second;
		for (auto &block : blocks)
		{
			builder.add_block(block);

			// If a temporary is used in more than one block, we might have to lift continue block
			// access up to loop header like we did for variables.
			if (blocks.size() != 1 && this->is_continue(block))
				builder.add_block(this->continue_block_to_loop_header[block]);
		}

		uint32_t dominating_block = builder.get_dominator();
		if (dominating_block)
		{
			// If we touch a variable in the dominating block, this is the expected setup.
			// SPIR-V normally mandates this, but we have extra cases for temporary use inside loops.
			bool first_use_is_dominator = blocks.count(dominating_block) != 0;

			if (!first_use_is_dominator)
			{
				// This should be very rare, but if we try to declare a temporary inside a loop,
				// and that temporary is used outside the loop as well (spirv-opt inliner likes this)
				// we should actually emit the temporary outside the loop.
				this->hoisted_temporaries.insert(var.first);
				this->forced_temporaries.insert(var.first);

				auto &block_temporaries = this->get<SPIRBlock>(dominating_block).declare_temporary;
				block_temporaries.emplace_back(handler.result_id_to_type[var.first], var.first);
			}
		}
	}

	unordered_set<uint32_t> seen_blocks;

	// Now, try to analyze whether or not these variables are actually loop variables.
	for (auto &loop_variable : potential_loop_variables)
	{
		auto &var = this->get<SPIRVariable>(loop_variable.first);
		auto dominator = var.dominator;
		auto block = loop_variable.second;

		// The variable was accessed in multiple continue blocks, ignore.
		if (block == ~(0u) || block == 0)
			continue;

		// Dead code.
		if (dominator == 0)
			continue;

		uint32_t header = 0;

		// Find the loop header for this block.
		for (auto b : this->loop_blocks)
		{
			auto &potential_header = this->get<SPIRBlock>(b);
			if (potential_header.continue_block == block)
			{
				header = b;
				break;
			}
		}

		assert(header);
		auto &header_block = this->get<SPIRBlock>(header);
		auto &blocks = handler.accessed_variables_to_block[loop_variable.first];

		// If a loop variable is not used before the loop, it's probably not a loop variable.
		bool has_accessed_variable = blocks.count(header) != 0;

		// Now, there are two conditions we need to meet for the variable to be a loop variable.
		// 1. The dominating block must have a branch-free path to the loop header,
		// this way we statically know which expression should be part of the loop variable initializer.

		// Walk from the dominator, if there is one straight edge connecting
		// dominator and loop header, we statically know the loop initializer.
		bool static_loop_init = true;
		while (dominator != header)
		{
			if (blocks.count(dominator) != 0)
				has_accessed_variable = true;

			auto &succ = cfg.get_succeeding_edges(dominator);
			if (succ.size() != 1)
			{
				static_loop_init = false;
				break;
			}

			auto &pred = cfg.get_preceding_edges(succ.front());
			if (pred.size() != 1 || pred.front() != dominator)
			{
				static_loop_init = false;
				break;
			}

			dominator = succ.front();
		}

		if (!static_loop_init || !has_accessed_variable)
			continue;

		// The second condition we need to meet is that no access after the loop
		// merge can occur. Walk the CFG to see if we find anything.

		seen_blocks.clear();
		cfg.walk_from(seen_blocks, header_block.merge_block, [&](uint32_t walk_block) {
			// We found a block which accesses the variable outside the loop.
			if (blocks.find(walk_block) != end(blocks))
				static_loop_init = false;
		});

		if (!static_loop_init)
			continue;

		// We have a loop variable.
		header_block.loop_variables.push_back(loop_variable.first);
		// Need to sort here as variables come from an unordered container, and pushing stuff in wrong order
		// will break reproducability in regression runs.
		sort(begin(header_block.loop_variables), end(header_block.loop_variables));
		this->get<SPIRVariable>(loop_variable.first).loop_variable = true;
	}
}

uint64_t Compiler::get_buffer_block_flags(const SPIRVariable &var)
{
	auto &type = get<SPIRType>(var.basetype);
	assert(type.basetype == SPIRType::Struct);

	// Some flags like non-writable, non-readable are actually found
	// as member decorations. If all members have a decoration set, propagate
	// the decoration up as a regular variable decoration.
	uint64_t base_flags = meta[var.self].decoration.decoration_flags;

	if (type.member_types.empty())
		return base_flags;

	uint64_t all_members_flag_mask = ~(0ull);
	for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
		all_members_flag_mask &= get_member_decoration_mask(type.self, i);

	return base_flags | all_members_flag_mask;
}

bool Compiler::get_common_basic_type(const SPIRType &type, SPIRType::BaseType &base_type)
{
	if (type.basetype == SPIRType::Struct)
	{
		base_type = SPIRType::Unknown;
		for (auto &member_type : type.member_types)
		{
			SPIRType::BaseType member_base;
			if (!get_common_basic_type(get<SPIRType>(member_type), member_base))
				return false;

			if (base_type == SPIRType::Unknown)
				base_type = member_base;
			else if (base_type != member_base)
				return false;
		}
		return true;
	}
	else
	{
		base_type = type.basetype;
		return true;
	}
}

void Compiler::ActiveBuiltinHandler::handle_builtin(const SPIRType &type, BuiltIn builtin, uint64_t decoration_flags)
{
	// If used, we will need to explicitly declare a new array size for these builtins.

	if (builtin == BuiltInClipDistance)
	{
		if (!type.array_size_literal[0])
			SPIRV_CROSS_THROW("Array size for ClipDistance must be a literal.");
		uint32_t array_size = type.array[0];
		if (array_size == 0)
			SPIRV_CROSS_THROW("Array size for ClipDistance must not be unsized.");
		compiler.clip_distance_count = array_size;
	}
	else if (builtin == BuiltInCullDistance)
	{
		if (!type.array_size_literal[0])
			SPIRV_CROSS_THROW("Array size for CullDistance must be a literal.");
		uint32_t array_size = type.array[0];
		if (array_size == 0)
			SPIRV_CROSS_THROW("Array size for CullDistance must not be unsized.");
		compiler.cull_distance_count = array_size;
	}
	else if (builtin == BuiltInPosition)
	{
		if (decoration_flags & (1ull << DecorationInvariant))
			compiler.position_invariant = true;
	}
}

bool Compiler::ActiveBuiltinHandler::handle(spv::Op opcode, const uint32_t *args, uint32_t length)
{
	const auto add_if_builtin = [&](uint32_t id) {
		// Only handles variables here.
		// Builtins which are part of a block are handled in AccessChain.
		auto *var = compiler.maybe_get<SPIRVariable>(id);
		auto &decorations = compiler.meta[id].decoration;
		if (var && decorations.builtin)
		{
			auto &type = compiler.get<SPIRType>(var->basetype);
			auto &flags =
			    type.storage == StorageClassInput ? compiler.active_input_builtins : compiler.active_output_builtins;
			flags |= 1ull << decorations.builtin_type;
			handle_builtin(type, decorations.builtin_type, decorations.decoration_flags);
		}
	};

	switch (opcode)
	{
	case OpStore:
		if (length < 1)
			return false;

		add_if_builtin(args[0]);
		break;

	case OpCopyMemory:
		if (length < 2)
			return false;

		add_if_builtin(args[0]);
		add_if_builtin(args[1]);
		break;

	case OpCopyObject:
	case OpLoad:
		if (length < 3)
			return false;

		add_if_builtin(args[2]);
		break;

	case OpFunctionCall:
	{
		if (length < 3)
			return false;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
			add_if_builtin(args[i]);
		break;
	}

	case OpAccessChain:
	case OpInBoundsAccessChain:
	{
		if (length < 4)
			return false;

		// Only consider global variables, cannot consider variables in functions yet, or other
		// access chains as they have not been created yet.
		auto *var = compiler.maybe_get<SPIRVariable>(args[2]);
		if (!var)
			break;

		// Required if we access chain into builtins like gl_GlobalInvocationID.
		add_if_builtin(args[2]);

		auto *type = &compiler.get<SPIRType>(var->basetype);

		// Start traversing type hierarchy at the proper non-pointer types.
		while (type->pointer)
		{
			assert(type->parent_type);
			type = &compiler.get<SPIRType>(type->parent_type);
		}

		auto &flags =
		    type->storage == StorageClassInput ? compiler.active_input_builtins : compiler.active_output_builtins;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
		{
			// Arrays
			if (!type->array.empty())
			{
				type = &compiler.get<SPIRType>(type->parent_type);
			}
			// Structs
			else if (type->basetype == SPIRType::Struct)
			{
				uint32_t index = compiler.get<SPIRConstant>(args[i]).scalar();

				if (index < uint32_t(compiler.meta[type->self].members.size()))
				{
					auto &decorations = compiler.meta[type->self].members[index];
					if (decorations.builtin)
					{
						flags |= 1ull << decorations.builtin_type;
						handle_builtin(compiler.get<SPIRType>(type->member_types[index]), decorations.builtin_type,
						               decorations.decoration_flags);
					}
				}

				type = &compiler.get<SPIRType>(type->member_types[index]);
			}
			else
			{
				// No point in traversing further. We won't find any extra builtins.
				break;
			}
		}
		break;
	}

	default:
		break;
	}

	return true;
}

void Compiler::update_active_builtins()
{
	active_input_builtins = 0;
	active_output_builtins = 0;
	cull_distance_count = 0;
	clip_distance_count = 0;
	ActiveBuiltinHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);
}

// Returns whether this shader uses a builtin of the storage class
bool Compiler::has_active_builtin(BuiltIn builtin, StorageClass storage)
{
	uint64_t flags;
	switch (storage)
	{
	case StorageClassInput:
		flags = active_input_builtins;
		break;
	case StorageClassOutput:
		flags = active_output_builtins;
		break;

	default:
		return false;
	}
	return flags & (1ull << builtin);
}

void Compiler::analyze_image_and_sampler_usage()
{
	CombinedImageSamplerUsageHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(entry_point), handler);
	comparison_samplers = move(handler.comparison_samplers);
	comparison_images = move(handler.comparison_images);
	need_subpass_input = handler.need_subpass_input;
}

bool Compiler::CombinedImageSamplerUsageHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &func = compiler.get<SPIRFunction>(args[2]);
	const auto *arg = &args[3];
	length -= 3;

	for (uint32_t i = 0; i < length; i++)
	{
		auto &argument = func.arguments[i];
		dependency_hierarchy[argument.id].insert(arg[i]);
	}

	return true;
}

void Compiler::CombinedImageSamplerUsageHandler::add_hierarchy_to_comparison_images(uint32_t image)
{
	// Traverse the variable dependency hierarchy and tag everything in its path with comparison images.
	comparison_images.insert(image);
	for (auto &img : dependency_hierarchy[image])
		add_hierarchy_to_comparison_images(img);
}

void Compiler::CombinedImageSamplerUsageHandler::add_hierarchy_to_comparison_samplers(uint32_t sampler)
{
	// Traverse the variable dependency hierarchy and tag everything in its path with comparison samplers.
	comparison_samplers.insert(sampler);
	for (auto &samp : dependency_hierarchy[sampler])
		add_hierarchy_to_comparison_samplers(samp);
}

bool Compiler::CombinedImageSamplerUsageHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	switch (opcode)
	{
	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpLoad:
	{
		if (length < 3)
			return false;
		dependency_hierarchy[args[1]].insert(args[2]);

		// Ideally defer this to OpImageRead, but then we'd need to track loaded IDs.
		// If we load an image, we're going to use it and there is little harm in declaring an unused gl_FragCoord.
		auto &type = compiler.get<SPIRType>(args[0]);
		if (type.image.dim == DimSubpassData)
			need_subpass_input = true;
		break;
	}

	case OpSampledImage:
	{
		if (length < 4)
			return false;

		uint32_t result_type = args[0];
		auto &type = compiler.get<SPIRType>(result_type);
		if (type.image.depth)
		{
			// This image must be a depth image.
			uint32_t image = args[2];
			add_hierarchy_to_comparison_images(image);

			// This sampler must be a SamplerComparisionState, and not a regular SamplerState.
			uint32_t sampler = args[3];
			add_hierarchy_to_comparison_samplers(sampler);
		}
		return true;
	}

	default:
		break;
	}

	return true;
}

bool Compiler::buffer_is_hlsl_counter_buffer(uint32_t id) const
{
	if (meta.at(id).hlsl_magic_counter_buffer_candidate)
	{
		auto *var = maybe_get<SPIRVariable>(id);
		// Ensure that this is actually a buffer object.
		return var && (var->storage == StorageClassStorageBuffer ||
		               has_decoration(get<SPIRType>(var->basetype).self, DecorationBufferBlock));
	}
	else
		return false;
}

bool Compiler::buffer_get_hlsl_counter_buffer(uint32_t id, uint32_t &counter_id) const
{
	auto &name = get_name(id);
	uint32_t id_bound = get_current_id_bound();
	for (uint32_t i = 0; i < id_bound; i++)
	{
		if (meta[i].hlsl_magic_counter_buffer_candidate && meta[i].hlsl_magic_counter_buffer_name == name)
		{
			auto *var = maybe_get<SPIRVariable>(i);
			// Ensure that this is actually a buffer object.
			if (var && (var->storage == StorageClassStorageBuffer ||
			            has_decoration(get<SPIRType>(var->basetype).self, DecorationBufferBlock)))
			{
				counter_id = i;
				return true;
			}
		}
	}
	return false;
}

void Compiler::make_constant_null(uint32_t id, uint32_t type)
{
	auto &constant_type = get<SPIRType>(type);

	if (!constant_type.array.empty())
	{
		assert(constant_type.parent_type);
		uint32_t parent_id = increase_bound_by(1);
		make_constant_null(parent_id, constant_type.parent_type);

		if (!constant_type.array_size_literal.back())
			SPIRV_CROSS_THROW("Array size of OpConstantNull must be a literal.");

		vector<uint32_t> elements(constant_type.array.back());
		for (uint32_t i = 0; i < constant_type.array.back(); i++)
			elements[i] = parent_id;
		set<SPIRConstant>(id, type, elements.data(), uint32_t(elements.size()), false);
	}
	else if (!constant_type.member_types.empty())
	{
		uint32_t member_ids = increase_bound_by(uint32_t(constant_type.member_types.size()));
		vector<uint32_t> elements(constant_type.member_types.size());
		for (uint32_t i = 0; i < constant_type.member_types.size(); i++)
		{
			make_constant_null(member_ids + i, constant_type.member_types[i]);
			elements[i] = member_ids + i;
		}
		set<SPIRConstant>(id, type, elements.data(), uint32_t(elements.size()), false);
	}
	else
	{
		auto &constant = set<SPIRConstant>(id, type);
		constant.make_null(constant_type);
	}
}

const std::vector<spv::Capability> &Compiler::get_declared_capabilities() const
{
	return declared_capabilities;
}

const std::vector<std::string> &Compiler::get_declared_extensions() const
{
	return declared_extensions;
}

std::string Compiler::get_remapped_declared_block_name(uint32_t id) const
{
	auto itr = declared_block_names.find(id);
	if (itr != end(declared_block_names))
		return itr->second;
	else
	{
		auto &var = get<SPIRVariable>(id);
		auto &type = get<SPIRType>(var.basetype);
		auto &block_name = meta[type.self].decoration.alias;
		return block_name.empty() ? get_block_fallback_name(id) : block_name;
	}
}

bool Compiler::instruction_to_result_type(uint32_t &result_type, uint32_t &result_id, spv::Op op, const uint32_t *args,
                                          uint32_t length)
{
	// Most instructions follow the pattern of <result-type> <result-id> <arguments>.
	// There are some exceptions.
	switch (op)
	{
	case OpStore:
	case OpCopyMemory:
	case OpCopyMemorySized:
	case OpImageWrite:
	case OpAtomicStore:
	case OpAtomicFlagClear:
	case OpEmitStreamVertex:
	case OpEndStreamPrimitive:
	case OpControlBarrier:
	case OpMemoryBarrier:
	case OpGroupWaitEvents:
	case OpRetainEvent:
	case OpReleaseEvent:
	case OpSetUserEventStatus:
	case OpCaptureEventProfilingInfo:
	case OpCommitReadPipe:
	case OpCommitWritePipe:
	case OpGroupCommitReadPipe:
	case OpGroupCommitWritePipe:
		return false;

	default:
		if (length > 1)
		{
			result_type = args[0];
			result_id = args[1];
			return true;
		}
		else
			return false;
	}
}
