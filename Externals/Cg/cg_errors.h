/*
 *
 * Copyright (c) 2002-2010, NVIDIA Corporation.
 *
 *
 *
 * NVIDIA Corporation("NVIDIA") supplies this software to you in consideration
 * of your agreement to the following terms, and your use, installation,
 * modification or redistribution of this NVIDIA software constitutes
 * acceptance of these terms.  If you do not agree with these terms, please do
 * not use, install, modify or redistribute this NVIDIA software.
 *
 *
 *
 * In consideration of your agreement to abide by the following terms, and
 * subject to these terms, NVIDIA grants you a personal, non-exclusive license,
 * under NVIDIA's copyrights in this original NVIDIA software (the "NVIDIA
 * Software"), to use, reproduce, modify and redistribute the NVIDIA
 * Software, with or without modifications, in source and/or binary forms;
 * provided that if you redistribute the NVIDIA Software, you must retain the
 * copyright notice of NVIDIA, this notice and the following text and
 * disclaimers in all such redistributions of the NVIDIA Software. Neither the
 * name, trademarks, service marks nor logos of NVIDIA Corporation may be used
 * to endorse or promote products derived from the NVIDIA Software without
 * specific prior written permission from NVIDIA.  Except as expressly stated
 * in this notice, no other rights or licenses express or implied, are granted
 * by NVIDIA herein, including but not limited to any patent rights that may be
 * infringed by your derivative works or by other works in which the NVIDIA
 * Software may be incorporated. No hardware is licensed hereunder.
 *
 *
 *
 * THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING
 * WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR ITS USE AND OPERATION
 * EITHER ALONE OR IN COMBINATION WITH OTHER PRODUCTS.
 *
 *
 *
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, LOST
 * PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY OUT OF THE USE,
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE NVIDIA SOFTWARE,
 * HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
 * NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF NVIDIA HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * The following macro invocations define error codes returned by various cg
 * API functions.
 *
 * The macros have the form :
 *
 *   CG_ERROR_MACRO(code, enum_name, message)
 *
 *     code      : The integer error code associated with the error.
 *     enum_name : The name of enumerant of the error code in the API.
 *     message   : A description string associated with the error.
 *
 */


CG_ERROR_MACRO(0,
               CG_NO_ERROR,
               "No error has occurred.")

CG_ERROR_MACRO(1,
               CG_COMPILER_ERROR,
               "The compile returned an error.")

CG_ERROR_MACRO(2,
               CG_INVALID_PARAMETER_ERROR,
               "The parameter used is invalid.")

CG_ERROR_MACRO(3,
               CG_INVALID_PROFILE_ERROR,
               "The profile is not supported.")

CG_ERROR_MACRO(4,
               CG_PROGRAM_LOAD_ERROR,
               "The program could not load.")

CG_ERROR_MACRO(5,
               CG_PROGRAM_BIND_ERROR,
               "The program could not bind.")

CG_ERROR_MACRO(6,
               CG_PROGRAM_NOT_LOADED_ERROR,
               "The program must be loaded before this operation may be used.")

CG_ERROR_MACRO(7,
               CG_UNSUPPORTED_GL_EXTENSION_ERROR,
               "An unsupported GL extension was required to perform this operation.")

CG_ERROR_MACRO(8,
               CG_INVALID_VALUE_TYPE_ERROR,
               "An unknown value type was assigned to a parameter.")

CG_ERROR_MACRO(9,
               CG_NOT_MATRIX_PARAM_ERROR,
               "The parameter is not of matrix type.")

CG_ERROR_MACRO(10,
               CG_INVALID_ENUMERANT_ERROR,
               "The enumerant parameter has an invalid value.")

CG_ERROR_MACRO(11,
               CG_NOT_4x4_MATRIX_ERROR,
               "The parameter must be a 4x4 matrix type.")

CG_ERROR_MACRO(12,
               CG_FILE_READ_ERROR,
               "The file could not be read.")

CG_ERROR_MACRO(13,
               CG_FILE_WRITE_ERROR,
               "The file could not be written.")

CG_ERROR_MACRO(14,
               CG_NVPARSE_ERROR,
               "nvparse could not successfully parse the output from the Cg "
               "compiler backend.")

CG_ERROR_MACRO(15,
               CG_MEMORY_ALLOC_ERROR,
               "Memory allocation failed.")

CG_ERROR_MACRO(16,
               CG_INVALID_CONTEXT_HANDLE_ERROR,
               "Invalid context handle.")

CG_ERROR_MACRO(17,
               CG_INVALID_PROGRAM_HANDLE_ERROR,
               "Invalid program handle.")

CG_ERROR_MACRO(18,
               CG_INVALID_PARAM_HANDLE_ERROR,
               "Invalid parameter handle.")

CG_ERROR_MACRO(19,
               CG_UNKNOWN_PROFILE_ERROR,
               "The specified profile is unknown.")

CG_ERROR_MACRO(20,
               CG_VAR_ARG_ERROR,
               "The variable arguments were specified incorrectly.")

CG_ERROR_MACRO(21,
               CG_INVALID_DIMENSION_ERROR,
               "The dimension value is invalid.")

CG_ERROR_MACRO(22,
               CG_ARRAY_PARAM_ERROR,
               "The parameter must be an array.")

CG_ERROR_MACRO(23,
               CG_OUT_OF_ARRAY_BOUNDS_ERROR,
               "Index into the array is out of bounds.")

CG_ERROR_MACRO(24,
               CG_CONFLICTING_TYPES_ERROR,
               "A type being added to the context conflicts with an "
               "existing type.")

CG_ERROR_MACRO(25,
               CG_CONFLICTING_PARAMETER_TYPES_ERROR,
               "The parameters being bound have conflicting types.")

CG_ERROR_MACRO(26,
               CG_PARAMETER_IS_NOT_SHARED_ERROR,
               "The parameter must be global.")

CG_ERROR_MACRO(27,
               CG_INVALID_PARAMETER_VARIABILITY_ERROR,
               "The parameter could not be changed to the given variability.")

CG_ERROR_MACRO(28,
               CG_CANNOT_DESTROY_PARAMETER_ERROR,
               "Cannot destroy the parameter.  It is bound to other parameters "
               "or is not a root parameter.")


CG_ERROR_MACRO(29,
               CG_NOT_ROOT_PARAMETER_ERROR,
               "The parameter is not a root parameter.")

CG_ERROR_MACRO(30,
               CG_PARAMETERS_DO_NOT_MATCH_ERROR,
               "The two parameters being bound do not match.")

CG_ERROR_MACRO(31,
               CG_IS_NOT_PROGRAM_PARAMETER_ERROR,
               "The parameter is not a program parameter.")

CG_ERROR_MACRO(32,
               CG_INVALID_PARAMETER_TYPE_ERROR,
               "The type of the parameter is invalid.")

CG_ERROR_MACRO(33,
               CG_PARAMETER_IS_NOT_RESIZABLE_ARRAY_ERROR,
               "The parameter must be a resizable array.")

CG_ERROR_MACRO(34,
               CG_INVALID_SIZE_ERROR,
               "The size value is invalid.")

CG_ERROR_MACRO(35,
               CG_BIND_CREATES_CYCLE_ERROR,
               "Cannot bind the given parameters.  Binding will form a cycle.")

CG_ERROR_MACRO(36,
               CG_ARRAY_TYPES_DO_NOT_MATCH_ERROR,
               "Cannot bind the given parameters.  Array types do not match.")

CG_ERROR_MACRO(37,
               CG_ARRAY_DIMENSIONS_DO_NOT_MATCH_ERROR,
               "Cannot bind the given parameters.  "
               "Array dimensions do not match.")

CG_ERROR_MACRO(38,
               CG_ARRAY_HAS_WRONG_DIMENSION_ERROR,
               "The array has the wrong dimension.")

CG_ERROR_MACRO(39,
               CG_TYPE_IS_NOT_DEFINED_IN_PROGRAM_ERROR,
               "Connecting the parameters failed because The type of the "
               "source parameter is not defined within the given program "
               "or does not match the type with the same name in the program.")

CG_ERROR_MACRO(40,
               CG_INVALID_EFFECT_HANDLE_ERROR,
               "Invalid effect handle.")

CG_ERROR_MACRO(41,
               CG_INVALID_STATE_HANDLE_ERROR,
               "Invalid state handle.")

CG_ERROR_MACRO(42,
               CG_INVALID_STATE_ASSIGNMENT_HANDLE_ERROR,
               "Invalid stateassignment handle.")

CG_ERROR_MACRO(43,
               CG_INVALID_PASS_HANDLE_ERROR,
               "Invalid pass handle.")

CG_ERROR_MACRO(44,
               CG_INVALID_ANNOTATION_HANDLE_ERROR,
               "Invalid annotation handle.")

CG_ERROR_MACRO(45,
               CG_INVALID_TECHNIQUE_HANDLE_ERROR,
               "Invalid technique handle.")

/* Do not use this!  Use CG_INVALID_PARAM_HANDLE_ERROR instead. */

CG_ERROR_MACRO(46,
               CG_INVALID_PARAMETER_HANDLE_ERROR,
               "Invalid parameter handle.")

CG_ERROR_MACRO(47,
               CG_STATE_ASSIGNMENT_TYPE_MISMATCH_ERROR,
               "Operation is not valid for this type of stateassignment.")

CG_ERROR_MACRO(48,
               CG_INVALID_FUNCTION_HANDLE_ERROR,
               "Invalid function handle.")

CG_ERROR_MACRO(49,
               CG_INVALID_TECHNIQUE_ERROR,
               "Technique did not pass validation.")

CG_ERROR_MACRO(50,
               CG_INVALID_POINTER_ERROR,
               "The supplied pointer is NULL.")

CG_ERROR_MACRO(51,
               CG_NOT_ENOUGH_DATA_ERROR,
               "Not enough data was provided.")

CG_ERROR_MACRO(52,
               CG_NON_NUMERIC_PARAMETER_ERROR,
               "The parameter is not of a numeric type.")

CG_ERROR_MACRO(53,
               CG_ARRAY_SIZE_MISMATCH_ERROR,
               "The specified array sizes are not compatible with the given array.")

CG_ERROR_MACRO(54,
               CG_CANNOT_SET_NON_UNIFORM_PARAMETER_ERROR,
               "Cannot set the value of a non-uniform parameter.")

CG_ERROR_MACRO(55,
               CG_DUPLICATE_NAME_ERROR,
               "This name is already in use.")

CG_ERROR_MACRO(56,
               CG_INVALID_OBJ_HANDLE_ERROR,
               "Invalid object handle.")

CG_ERROR_MACRO(57,
               CG_INVALID_BUFFER_HANDLE_ERROR,
               "Invalid buffer handle.")

CG_ERROR_MACRO(58,
               CG_BUFFER_INDEX_OUT_OF_RANGE_ERROR,
               "Buffer index is out of bounds.")

CG_ERROR_MACRO(59,
               CG_BUFFER_ALREADY_MAPPED_ERROR,
               "The buffer is already mapped.")

CG_ERROR_MACRO(60,
               CG_BUFFER_UPDATE_NOT_ALLOWED_ERROR,
               "The buffer cannot be updated.")

CG_ERROR_MACRO(61,
               CG_GLSLG_UNCOMBINED_LOAD_ERROR,
               "The GLSL geometry program can not load without being combined with a vertex program.")

#undef CG_ERROR_MACRO

