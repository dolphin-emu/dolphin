// SPDX-License-Identifier: 0BSD

///////////////////////////////////////////////////////////////////////////////
//
/// \file       suffix.h
/// \brief      Checks filename suffix and creates the destination filename
//
//  Author:     Lasse Collin
//
///////////////////////////////////////////////////////////////////////////////

/// \brief      Get the name of the destination file
///
/// Depending on the global variable opt_mode, this tries to find a matching
/// counterpart for src_name. If the name can be constructed, it is allocated
/// and returned (caller must free it). On error, a message is printed and
/// NULL is returned.
extern char *suffix_get_dest_name(const char *src_name);


/// \brief      Set a custom filename suffix
///
/// This function calls xstrdup() for the given suffix, thus the caller
/// doesn't need to keep the memory allocated. There can be only one custom
/// suffix, thus if this is called multiple times, the old suffixes are freed
/// and forgotten.
extern void suffix_set(const char *suffix);


/// \brief      Check if a custom suffix has been set
///
/// Returns true if the internal tracking of the suffix string has been set
/// and false if the string has not been set. This will keep the suffix
/// string encapsulated instead of extern-ing the variable.
extern bool suffix_is_set(void);
