/* PET
 * Platform for Experimentation with efficient HPSG processing Techniques
 * (C) 1999 - 2002 Ulrich Callmeier uc@coli.uni-sb.de
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/** \file unicode.h
 * This module provides 8 bit encodings - Unicode strings conversion
 * facilities. The ICU library provides the required functionality.
 */

#ifndef _UNICODE_H_
#define _UNICODE_H_

// ICU
#include "unicode/unistr.h"
#include "unicode/schriter.h"
#include "unicode/ucnv.h"

/** Global converter for externally encoded strings */
extern class EncodingConverter *Conv;

/// \todo obsolete when yy is obsolete
extern class EncodingConverter *ConvUTF8;

/**
 * Encoding conversion utility class
 * This class implements UnicodeString <-> STL String
 * converters using a specified encoding for the STL strings (e.g. UTF8)
 */

class EncodingConverter
{
public:
  /** Create a converter for the encoding \a encodingname */
  EncodingConverter(string encodingname);
  ~EncodingConverter();

  /** Convert the string \a from, which has to be encoded in the encoding of
   *  this converter, to UTF8 and return the new string
   */
  string convert(const UnicodeString from);
  /** Convert the string \a from, which has to be encoded in UTF8, into the
   *  encoding this converter and return the new string.
   */
  UnicodeString convert(const string from);

private:
  UErrorCode _status;
  UConverter *_conv;
  string _encoding;
};

/** Initialize the conversion service and the global encoding converter */
void initialize_encoding_converter(string encoding);

#endif
