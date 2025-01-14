# AmigaGPT

MIT License

Copyright (c) 2023-2025 Cameron Armstrong

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

# THIRD PARTY LICENSES

## json-c


Copyright (c) 2009-2012 Eric Haszlakiewicz

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

----------------------------------------------------------------

Copyright (c) 2004, 2005 Metaparadigm Pte Ltd

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Flite

```
Flite is free software.

We have kept the core code to BSD-like copyright, thus the system is
free to use in commercial products, with commercial extensions.  GPL
code is only included as part of the build process and does not
taint any of the run-time code.  

Note the PalmOS code uses BSD code that requires acknowledgements in
any distribution documentation.

As a collection it is distributed under the following license.  Note
a few files in this distribution have a different but equally free
non-conflicting licence, see below.

                  Language Technologies Institute                      
                     Carnegie Mellon University                        
                      Copyright (c) 1999-2008                          
                        All Rights Reserved.                           
                                                                       
  Permission is hereby granted, free of charge, to use and distribute  
  this software and its documentation without restriction, including   
  without limitation the rights to use, copy, modify, merge, publish,  
  distribute, sublicense, and/or sell copies of this work, and to      
  permit persons to whom this work is furnished to do so, subject to   
  the following conditions:                                            
   1. The code must retain the above copyright notice, this list of    
      conditions and the following disclaimer.                         
   2. Any modifications must be clearly marked as such.                
   3. Original authors' names are not deleted.                         
   4. The authors' names are not used to endorse or promote products   
      derived from this software without specific prior written        
      permission.                                                      
                                                                       
  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK         
  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      
  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   
  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE      
  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   
  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       
  THIS SOFTWARE.                                                       
                                                                       
All files within this distribution have the above license except
the following

src/cg/cst_mlpg.h
src/cg/cst_mlpg.c
src/cg/cst_mlsa.h
src/cg/cst_mlsa.c
src/cg/cst_vc.h
src/cg/cst_vc.c
/*********************************************************************/
/*                                                                   */
/*            Nagoya Institute of Technology, Aichi, Japan,          */
/*       Nara Institute of Science and Technology, Nara, Japan       */
/*                                and                                */
/*             Carnegie Mellon University, Pittsburgh, PA            */
/*                      Copyright (c) 2003-2004                      */
/*                        All Rights Reserved.                       */
/*                                                                   */
/*  Permission is hereby granted, free of charge, to use and         */
/*  distribute this software and its documentation without           */
/*  restriction, including without limitation the rights to use,     */
/*  copy, modify, merge, publish, distribute, sublicense, and/or     */
/*  sell copies of this work, and to permit persons to whom this     */
/*  work is furnished to do so, subject to the following conditions: */
/*                                                                   */
/*    1. The code must retain the above copyright notice, this list  */
/*       of conditions and the following disclaimer.                 */
/*    2. Any modifications must be clearly marked as such.           */
/*    3. Original authors' names are not deleted.                    */
/*                                                                   */    
/*  NAGOYA INSTITUTE OF TECHNOLOGY, NARA INSTITUTE OF SCIENCE AND    */
/*  TECHNOLOGY, CARNEGIE MELLON UNIVERSITY, AND THE CONTRIBUTORS TO  */
/*  THIS WORK DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,  */
/*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, */
/*  IN NO EVENT SHALL NAGOYA INSTITUTE OF TECHNOLOGY, NARA           */
/*  INSTITUTE OF SCIENCE AND TECHNOLOGY, CARNEGIE MELLON UNIVERSITY, */
/*  NOR THE CONTRIBUTORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR      */
/*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM   */
/*  LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,  */
/*  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN        */
/*  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.         */
/*                                                                   */
/*********************************************************************/

These functions are derived from the versions in festvox/src/vc/ as
modified by Tomoki Toda which in turn contain code derived from
NITECH's HTS system.  Their copyright has the same freedoms as 
as Flite's but under NAIST, NITECH and/or CMU.

src/audio/au_wince.c
src/utils/cst_file_stdio.c
src/utils/cst_mmap_posix.c
src/utils/cst_mmap_win32.c
src/utils/cst_mmap_none.c
src/utils/cst_file_wince.c
sapi/
      are copyright Cepstral, LLC rather than CMU but fall under the same
      free licence as the above, except for the owner.

doc/alice
   Is the first two chapters of Alice in Wonderland as distributed by the
   Gutenburg project and is now in the public domain

src/regex/regexp.c
src/regex/regsub.c

 *      Copyright (c) 1986 by University of Toronto.
 *      Written by Henry Spencer.  Not derived from licensed software.
 *
 *      Permission is granted to anyone to use this software for any
 *      purpose on any computer system, and to redistribute it freely,
 *      subject to the following restrictions:
 *
 *      1. The author is not responsible for the consequences of use of
 *              this software, no matter how awful, even if they arise
 *              from defects in it.
 *
 *      2. The origin of this software must not be misrepresented, either
 *              by explicit claim or by omission.
 *
 *      3. Altered versions must be plainly marked as such, and must not
 *              be misrepresented as being the original software.

src/speech/rateconv.c

 * Copyright (c) 1992, 1995 by Markus Mummert
 *
 * Redistribution and use of this software, modifcation and inclusion
 * into other forms of software are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of this software must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. If this software is redistributed in a modified condition
 *    it must reveal clearly that it has been modified.

lang/usenglish/us_durz_cart.c
lang/usenglish/us_durz_cart.h
lang/usenglish/us_int_accent_cart.c
lang/usenglish/us_int_accent_cart.h
lang/usenglish/us_int_tone_cart.c
lang/usenglish/us_int_tone_cart.h
lang/usenglish/us_phoneset.c
lang/usenglish/us_f0lr.c
   These are directly (or indirectly) compiled/derived from files that are
   part of the Festival Speech Synthesis System (1.4.1).  Hence they have
   a joint copyright CMU/Edinburgh but with the same free licence

configure
   # Copyright (C) 1992, 93, 94, 95, 96 Free Software Foundation, Inc.
   #
   # This configure script is free software; the Free Software Foundation
   # gives unlimited permission to copy, distribute and modify it.

configure.sub
config.guess
missing
install-sh
mkinstalldirs
   Copyright FSF, and under the GPL, these files are only used for
   convenient configuration and are not part of the generated binary,
   and therefore do not impose any GPL restrctions on the rest of the
   system.  But as they are standard methods for configuration they 
   are included.

palm/include/peal.h
palm/include/pealstub.h
palm/arm_flite/pealstub.c
palm/m68k_flite/peal.c

/**********
 * Copyright (c) 2004 Greg Parker.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GREG PARKER ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **********/

The whole Sun Microsystems math library was ported to Palm
for this but its not actually needed, though the include file is
to allow bits of the system to compile that currently linked in
to the system, so we just have the include file here

palm/include/fdlibm.h

 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
```
## Flite Device

```
/* Copyright (c) 2013 Fredrik Wikstrom. All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
** CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
** SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
** INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
** CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
** ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
** POSSIBILITY OF SUCH DAMAGE.
*/
```