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

/* defines class representing errors - used in exceptions */

#ifndef _ERROR_H_
#define _ERROR_H_

#include <string>

/** Represent an error that occured. The implementation is trivial, we just
 *  store an (optional) error message as a string. */
class tError
{
 public:
    tError(std::string message = "Unknown error")
        : _message(message)
    {}
    
    ~tError()
    {}
    
    std::string getMessage()
    { 
        return _message;
    }
   
 private:
    std::string _message;
};

#endif
