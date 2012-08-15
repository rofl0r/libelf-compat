/* Return string pointer from string section.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004, 2008, 2009 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 1998.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>

#include "libelf.h"
#include "libelfP.h"


char *
elf_strptr (elf, idx, offset)
     Elf *elf;
     size_t idx;
     size_t offset;
{
  if (elf == NULL)
    return NULL;

  if (elf->kind != ELF_K_ELF)
    {
      __libelf_seterrno (ELF_E_INVALID_HANDLE);
      return NULL;
    }

  rwlock_rdlock (elf->lock);

  char *result = NULL;
  Elf_Scn *strscn;

  /* Find the section in the list.  */
  Elf_ScnList *runp = (elf->class == ELFCLASS32
		       || (offsetof (struct Elf, state.elf32.scns)
			   == offsetof (struct Elf, state.elf64.scns))
		       ? &elf->state.elf32.scns : &elf->state.elf64.scns);
  while (1)
    {
      if (idx < runp->max)
	{
	  if (idx < runp->cnt)
	    strscn = &runp->data[idx];
	  else
	    {
	      __libelf_seterrno (ELF_E_INVALID_INDEX);
	      goto out;
	    }
	  break;
	}

      idx -= runp->max;

      runp = runp->next;
      if (runp == NULL)
	{
	  __libelf_seterrno (ELF_E_INVALID_INDEX);
	  goto out;
	}
    }

  if (elf->class == ELFCLASS32)
    {
      if (unlikely (strscn->shdr.e32->sh_type != SHT_STRTAB))
	{
	  /* This is no string section.  */
	  __libelf_seterrno (ELF_E_INVALID_SECTION);
	  goto out;
	}

      if (unlikely (offset >= strscn->shdr.e32->sh_size))
	{
	  /* The given offset is too big, it is beyond this section.  */
	  __libelf_seterrno (ELF_E_OFFSET_RANGE);
	  goto out;
	}
    }
  else
    {
      if (unlikely (strscn->shdr.e64->sh_type != SHT_STRTAB))
	{
	  /* This is no string section.  */
	  __libelf_seterrno (ELF_E_INVALID_SECTION);
	  goto out;
	}

      if (unlikely (offset >= strscn->shdr.e64->sh_size))
	{
	  /* The given offset is too big, it is beyond this section.  */
	  __libelf_seterrno (ELF_E_OFFSET_RANGE);
	  goto out;
	}
    }

  if (strscn->rawdata_base == NULL && ! strscn->data_read)
    {
      rwlock_unlock (elf->lock);
      rwlock_wrlock (elf->lock);
      if (strscn->rawdata_base == NULL && ! strscn->data_read
	/* Read the section data.  */
	  && __libelf_set_rawdata_wrlock (strscn) != 0)
	goto out;
    }

  if (likely (strscn->rawdata_base != NULL))
    // XXX Is this correct if a file is read and then new data is added
    // XXX to the string section?  Likely needs to check offset against
    // XXX size of rawdata_base buffer and then iterate over rest of the
    // XXX list.
    result = &strscn->rawdata_base[offset];
  else
    {
      /* This is a file which is currently created.  Use the list of
	 data blocks.  */
      struct Elf_Data_List *dl = &strscn->data_list;
      while (dl != NULL)
	{
	  if (offset >= (size_t) dl->data.d.d_off
	      && offset < dl->data.d.d_off + dl->data.d.d_size)
	    {
	      result = (char *) dl->data.d.d_buf + (offset - dl->data.d.d_off);
	      break;
	    }

	  dl = dl->next;
	}
    }

 out:
  rwlock_unlock (elf->lock);

  return result;
}
INTDEF(elf_strptr)
