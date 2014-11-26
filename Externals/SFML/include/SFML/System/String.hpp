////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2013 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef SFML_STRING_HPP
#define SFML_STRING_HPP

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/System/Export.hpp>
#include <locale>
#include <string>


namespace sf
{
////////////////////////////////////////////////////////////
/// \brief Utility string class that automatically handles
///        conversions between types and encodings
///
////////////////////////////////////////////////////////////
class SFML_SYSTEM_API String
{
public :

    ////////////////////////////////////////////////////////////
    // Types
    ////////////////////////////////////////////////////////////
    typedef std::basic_string<Uint32>::iterator       Iterator;      ///< Iterator type
    typedef std::basic_string<Uint32>::const_iterator ConstIterator; ///< Constant iterator type

    ////////////////////////////////////////////////////////////
    // Static member data
    ////////////////////////////////////////////////////////////
    static const std::size_t InvalidPos; ///< Represents an invalid position in the string

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// This constructor creates an empty string.
    ///
    ////////////////////////////////////////////////////////////
    String();

    ////////////////////////////////////////////////////////////
    /// \brief Construct from a single ANSI character and a locale
    ///
    /// The source character is converted to UTF-32 according
    /// to the given locale.
    ///
    /// \param ansiChar ANSI character to convert
    /// \param locale   Locale to use for conversion
    ///
    ////////////////////////////////////////////////////////////
    String(char ansiChar, const std::locale& locale = std::locale());

    ////////////////////////////////////////////////////////////
    /// \brief Construct from single wide character
    ///
    /// \param wideChar Wide character to convert
    ///
    ////////////////////////////////////////////////////////////
    String(wchar_t wideChar);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from single UTF-32 character
    ///
    /// \param utf32Char UTF-32 character to convert
    ///
    ////////////////////////////////////////////////////////////
    String(Uint32 utf32Char);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from a null-terminated C-style ANSI string and a locale
    ///
    /// The source string is converted to UTF-32 according
    /// to the given locale.
    ///
    /// \param ansiString ANSI string to convert
    /// \param locale     Locale to use for conversion
    ///
    ////////////////////////////////////////////////////////////
    String(const char* ansiString, const std::locale& locale = std::locale());

    ////////////////////////////////////////////////////////////
    /// \brief Construct from an ANSI string and a locale
    ///
    /// The source string is converted to UTF-32 according
    /// to the given locale.
    ///
    /// \param ansiString ANSI string to convert
    /// \param locale     Locale to use for conversion
    ///
    ////////////////////////////////////////////////////////////
    String(const std::string& ansiString, const std::locale& locale = std::locale());

    ////////////////////////////////////////////////////////////
    /// \brief Construct from null-terminated C-style wide string
    ///
    /// \param wideString Wide string to convert
    ///
    ////////////////////////////////////////////////////////////
    String(const wchar_t* wideString);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from a wide string
    ///
    /// \param wideString Wide string to convert
    ///
    ////////////////////////////////////////////////////////////
    String(const std::wstring& wideString);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from a null-terminated C-style UTF-32 string
    ///
    /// \param utf32String UTF-32 string to assign
    ///
    ////////////////////////////////////////////////////////////
    String(const Uint32* utf32String);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from an UTF-32 string
    ///
    /// \param utf32String UTF-32 string to assign
    ///
    ////////////////////////////////////////////////////////////
    String(const std::basic_string<Uint32>& utf32String);

    ////////////////////////////////////////////////////////////
    /// \brief Copy constructor
    ///
    /// \param copy Instance to copy
    ///
    ////////////////////////////////////////////////////////////
    String(const String& copy);

    ////////////////////////////////////////////////////////////
    /// \brief Implicit cast operator to std::string (ANSI string)
    ///
    /// The current global locale is used for conversion. If you
    /// want to explicitely specify a locale, see toAnsiString.
    /// Characters that do not fit in the target encoding are
    /// discarded from the returned string.
    /// This operator is defined for convenience, and is equivalent
    /// to calling toAnsiString().
    ///
    /// \return Converted ANSI string
    ///
    /// \see toAnsiString, operator std::wstring
    ///
    ////////////////////////////////////////////////////////////
    operator std::string() const;

    ////////////////////////////////////////////////////////////
    /// \brief Implicit cast operator to std::wstring (wide string)
    ///
    /// Characters that do not fit in the target encoding are
    /// discarded from the returned string.
    /// This operator is defined for convenience, and is equivalent
    /// to calling toWideString().
    ///
    /// \return Converted wide string
    ///
    /// \see toWideString, operator std::string
    ///
    ////////////////////////////////////////////////////////////
    operator std::wstring() const;

    ////////////////////////////////////////////////////////////
    /// \brief Convert the unicode string to an ANSI string
    ///
    /// The UTF-32 string is converted to an ANSI string in
    /// the encoding defined by \a locale.
    /// Characters that do not fit in the target encoding are
    /// discarded from the returned string.
    ///
    /// \param locale Locale to use for conversion
    ///
    /// \return Converted ANSI string
    ///
    /// \see toWideString, operator std::string
    ///
    ////////////////////////////////////////////////////////////
    std::string toAnsiString(const std::locale& locale = std::locale()) const;

    ////////////////////////////////////////////////////////////
    /// \brief Convert the unicode string to a wide string
    ///
    /// Characters that do not fit in the target encoding are
    /// discarded from the returned string.
    ///
    /// \return Converted wide string
    ///
    /// \see toAnsiString, operator std::wstring
    ///
    ////////////////////////////////////////////////////////////
    std::wstring toWideString() const;

    ////////////////////////////////////////////////////////////
    /// \brief Overload of assignment operator
    ///
    /// \param right Instance to assign
    ///
    /// \return Reference to self
    ///
    ////////////////////////////////////////////////////////////
    String& operator =(const String& right);

    ////////////////////////////////////////////////////////////
    /// \brief Overload of += operator to append an UTF-32 string
    ///
    /// \param right String to append
    ///
    /// \return Reference to self
    ///
    ////////////////////////////////////////////////////////////
    String& operator +=(const String& right);

    ////////////////////////////////////////////////////////////
    /// \brief Overload of [] operator to access a character by its position
    ///
    /// This function provides read-only access to characters.
    /// Note: this function doesn't throw if \a index is out of range.
    ///
    /// \param index Index of the character to get
    ///
    /// \return Character at position \a index
    ///
    ////////////////////////////////////////////////////////////
    Uint32 operator [](std::size_t index) const;

    ////////////////////////////////////////////////////////////
    /// \brief Overload of [] operator to access a character by its position
    ///
    /// This function provides read and write access to characters.
    /// Note: this function doesn't throw if \a index is out of range.
    ///
    /// \param index Index of the character to get
    ///
    /// \return Reference to the character at position \a index
    ///
    ////////////////////////////////////////////////////////////
    Uint32& operator [](std::size_t index);

    ////////////////////////////////////////////////////////////
    /// \brief Clear the string
    ///
    /// This function removes all the characters from the string.
    ///
    /// \see isEmpty, erase
    ///
    ////////////////////////////////////////////////////////////
    void clear();

    ////////////////////////////////////////////////////////////
    /// \brief Get the size of the string
    ///
    /// \return Number of characters in the string
    ///
    /// \see isEmpty
    ///
    ////////////////////////////////////////////////////////////
    std::size_t getSize() const;

    ////////////////////////////////////////////////////////////
    /// \brief Check whether the string is empty or not
    ///
    /// \return True if the string is empty (i.e. contains no character)
    ///
    /// \see clear, getSize
    ///
    ////////////////////////////////////////////////////////////
    bool isEmpty() const;

    ////////////////////////////////////////////////////////////
    /// \brief Erase one or more characters from the string
    ///
    /// This function removes a sequence of \a count characters
    /// starting from \a position.
    ///
    /// \param position Position of the first character to erase
    /// \param count    Number of characters to erase
    ///
    ////////////////////////////////////////////////////////////
    void erase(std::size_t position, std::size_t count = 1);

    ////////////////////////////////////////////////////////////
    /// \brief Insert one or more characters into the string
    ///
    /// This function inserts the characters of \a str
    /// into the string, starting from \a position.
    ///
    /// \param position Position of insertion
    /// \param str      Characters to insert
    ///
    ////////////////////////////////////////////////////////////
    void insert(std::size_t position, const String& str);

    ////////////////////////////////////////////////////////////
    /// \brief Find a sequence of one or more characters in the string
    ///
    /// This function searches for the characters of \a str
    /// into the string, starting from \a start.
    ///
    /// \param str   Characters to find
    /// \param start Where to begin searching
    ///
    /// \return Position of \a str in the string, or String::InvalidPos if not found
    ///
    ////////////////////////////////////////////////////////////
    std::size_t find(const String& str, std::size_t start = 0) const;

    ////////////////////////////////////////////////////////////
    /// \brief Get a pointer to the C-style array of characters
    ///
    /// This functions provides a read-only access to a
    /// null-terminated C-style representation of the string.
    /// The returned pointer is temporary and is meant only for
    /// immediate use, thus it is not recommended to store it.
    ///
    /// \return Read-only pointer to the array of characters
    ///
    ////////////////////////////////////////////////////////////
    const Uint32* getData() const;

    ////////////////////////////////////////////////////////////
    /// \brief Return an iterator to the beginning of the string
    ///
    /// \return Read-write iterator to the beginning of the string characters
    ///
    /// \see end
    ///
    ////////////////////////////////////////////////////////////
    Iterator begin();

    ////////////////////////////////////////////////////////////
    /// \brief Return an iterator to the beginning of the string
    ///
    /// \return Read-only iterator to the beginning of the string characters
    ///
    /// \see end
    ///
    ////////////////////////////////////////////////////////////
    ConstIterator begin() const;

    ////////////////////////////////////////////////////////////
    /// \brief Return an iterator to the beginning of the string
    ///
    /// The end iterator refers to 1 position past the last character;
    /// thus it represents an invalid character and should never be
    /// accessed.
    ///
    /// \return Read-write iterator to the end of the string characters
    ///
    /// \see begin
    ///
    ////////////////////////////////////////////////////////////
    Iterator end();

    ////////////////////////////////////////////////////////////
    /// \brief Return an iterator to the beginning of the string
    ///
    /// The end iterator refers to 1 position past the last character;
    /// thus it represents an invalid character and should never be
    /// accessed.
    ///
    /// \return Read-only iterator to the end of the string characters
    ///
    /// \see begin
    ///
    ////////////////////////////////////////////////////////////
    ConstIterator end() const;

private :

    friend SFML_SYSTEM_API bool operator ==(const String& left, const String& right);
    friend SFML_SYSTEM_API bool operator <(const String& left, const String& right);

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    std::basic_string<Uint32> m_string; ///< Internal string of UTF-32 characters
};

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of == operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if both strings are equal
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator ==(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of != operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if both strings are different
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator !=(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of < operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if \a left is alphabetically lesser than \a right
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator <(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of > operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if \a left is alphabetically greater than \a right
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator >(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of <= operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if \a left is alphabetically lesser or equal than \a right
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator <=(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of >= operator to compare two UTF-32 strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return True if \a left is alphabetically greater or equal than \a right
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API bool operator >=(const String& left, const String& right);

////////////////////////////////////////////////////////////
/// \relates String
/// \brief Overload of binary + operator to concatenate two strings
///
/// \param left  Left operand (a string)
/// \param right Right operand (a string)
///
/// \return Concatenated string
///
////////////////////////////////////////////////////////////
SFML_SYSTEM_API String operator +(const String& left, const String& right);

} // namespace sf


#endif // SFML_STRING_HPP


////////////////////////////////////////////////////////////
/// \class sf::String
/// \ingroup system
///
/// sf::String is a utility string class defined mainly for
/// convenience. It is a Unicode string (implemented using
/// UTF-32), thus it can store any character in the world
/// (european, chinese, arabic, hebrew, etc.).
///
/// It automatically handles conversions from/to ANSI and
/// wide strings, so that you can work with standard string
/// classes and still be compatible with functions taking a
/// sf::String.
///
/// \code
/// sf::String s;
///
/// std::string s1 = s;  // automatically converted to ANSI string
/// std::wstring s2 = s; // automatically converted to wide string
/// s = "hello";         // automatically converted from ANSI string
/// s = L"hello";        // automatically converted from wide string
/// s += 'a';            // automatically converted from ANSI string
/// s += L'a';           // automatically converted from wide string
/// \endcode
///
/// Conversions involving ANSI strings use the default user locale. However
/// it is possible to use a custom locale if necessary:
/// \code
/// std::locale locale;
/// sf::String s;
/// ...
/// std::string s1 = s.toAnsiString(locale);
/// s = sf::String("hello", locale);
/// \endcode
///
/// sf::String defines the most important functions of the
/// standard std::string class: removing, random access, iterating,
/// appending, comparing, etc. However it is a simple class
/// provided for convenience, and you may have to consider using
/// a more optimized class if your program requires complex string
/// handling. The automatic conversion functions will then take
/// care of converting your string to sf::String whenever SFML
/// requires it.
///
/// Please note that SFML also defines a low-level, generic
/// interface for Unicode handling, see the sf::Utf classes.
///
////////////////////////////////////////////////////////////
