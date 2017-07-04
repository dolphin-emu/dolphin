// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#if defined(_MSC_FULL_VER)

// This header provides -very- partial support for C++ SD-6 feature macros,
// to be used as a sort of compat shim for msvc not defining them itself.

// Note that instead of defining all values to their proper values, this
// header defines only what's used, and supports a way to figure out what
// code is testing for.

#if _MSC_FULL_VER == 191025019
// VS2017 Update 2 (note: intellisense defines 191025017)
#define __cpp_constexpr 201304
#define __cpp_variable_templates 201304
#endif

// Any non-integral value should be able to be used to cause compilation to
// fail (assuming the user of the feature macro is testing against the
// integral date value). Can be used to detect what needs to be considered.
#define _MSC_SD6_DEFAULT_VALUE nullptr
//#define _MSC_SD6_DEFAULT_VALUE 0

// Note that anything intellisense shows below is a dirty lie

#if !defined(__cpp_aggregate_nsdmi)
#define __cpp_aggregate_nsdmi _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_alias_templates)
#define __cpp_alias_templates _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_attributes)
#define __cpp_attributes _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_binary_literals)
#define __cpp_binary_literals _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_concepts)
#define __cpp_concepts _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_constexpr)
#define __cpp_constexpr _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_decltype)
#define __cpp_decltype _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_decltype_auto)
#define __cpp_decltype_auto _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_delegating_constructors)
#define __cpp_delegating_constructors _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_exceptions)
#define __cpp_exceptions _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_generic_lambdas)
#define __cpp_generic_lambdas _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_inheriting_constructors)
#define __cpp_inheriting_constructors _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_init_captures)
#define __cpp_init_captures _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_initializer_lists)
#define __cpp_initializer_lists _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lambdas)
#define __cpp_lambdas _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_chrono_udls)
#define __cpp_lib_chrono_udls _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_complex_udls)
#define __cpp_lib_complex_udls _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_exchange_function)
#define __cpp_lib_exchange_function _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_any)
#define __cpp_lib_experimental_any _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_apply)
#define __cpp_lib_experimental_apply _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_boyer_moore_searching)
#define __cpp_lib_experimental_boyer_moore_searching _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_filesystem)
#define __cpp_lib_experimental_filesystem _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_function_erased_allocator)
#define __cpp_lib_experimental_function_erased_allocator _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_invocation_type)
#define __cpp_lib_experimental_invocation_type _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_memory_resources)
#define __cpp_lib_experimental_memory_resources _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_optional)
#define __cpp_lib_experimental_optional _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_packaged_task_erased_allocator)
#define __cpp_lib_experimental_packaged_task_erased_allocator _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_promise_erased_allocator)
#define __cpp_lib_experimental_promise_erased_allocator _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_sample)
#define __cpp_lib_experimental_sample _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_shared_ptr_arrays)
#define __cpp_lib_experimental_shared_ptr_arrays _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_string_view)
#define __cpp_lib_experimental_string_view _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_type_trait_variable_templates)
#define __cpp_lib_experimental_type_trait_variable_templates _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_generic_associative_lookup)
#define __cpp_lib_generic_associative_lookup _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_integer_sequence)
#define __cpp_lib_integer_sequence _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_integral_constant_callable)
#define __cpp_lib_integral_constant_callable _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_is_final)
#define __cpp_lib_is_final _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_is_null_pointer)
#define __cpp_lib_is_null_pointer _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_make_unique)
#define __cpp_lib_make_unique _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_make_reverse_iterator)
#define __cpp_lib_make_reverse_iterator _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_null_iterators)
#define __cpp_lib_null_iterators _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_experimental_parallel_algorithm)
#define __cpp_lib_experimental_parallel_algorithm _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_quoted_string_io)
#define __cpp_lib_quoted_string_io _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_result_of_sfinae)
#define __cpp_lib_result_of_sfinae _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_robust_nonmodifying_seq_ops)
#define __cpp_lib_robust_nonmodifying_seq_ops _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_shared_timed_mutex)
#define __cpp_lib_shared_timed_mutex _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_string_udls)
#define __cpp_lib_string_udls _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_transformation_trait_aliases)
#define __cpp_lib_transformation_trait_aliases _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_tuple_element_t)
#define __cpp_lib_tuple_element_t _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_tuples_by_type)
#define __cpp_lib_tuples_by_type _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_lib_transparent_operators)
#define __cpp_lib_transparent_operators _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_nsdmi)
#define __cpp_nsdmi _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_range_based_for)
#define __cpp_range_based_for _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_raw_strings)
#define __cpp_raw_strings _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_ref_qualifiers)
#define __cpp_ref_qualifiers _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_return_type_deduction)
#define __cpp_return_type_deduction _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_rtti)
#define __cpp_rtti _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_rvalue_references)
#define __cpp_rvalue_references _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_sized_deallocation)
#define __cpp_sized_deallocation _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_static_assert)
#define __cpp_static_assert _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_transactional_memory)
#define __cpp_transactional_memory _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_unicode_characters)
#define __cpp_unicode_characters _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_unicode_literals)
#define __cpp_unicode_literals _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_user_defined_literals)
#define __cpp_user_defined_literals _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_variable_templates)
#define __cpp_variable_templates _MSC_SD6_DEFAULT_VALUE
#endif
#if !defined(__cpp_variadic_templates)
#define __cpp_variadic_templates _MSC_SD6_DEFAULT_VALUE
#endif

#endif
