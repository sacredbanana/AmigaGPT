/***************************************************************************
 SimpleMail - Copyright (C) 2000 Hynek Schlawack and Sebastian Bauer

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
***************************************************************************/

/*
** datatypesclass.h
*/

#ifndef SM__DATATYPESCLASS_H
#define SM__DATATYPESCLASS_H

/*
    AROS definition of BOOPSI_DISPATCHER collides with other systems
    because the opening bracket { is part of the definition.
    Therefore I've defined MY_BOOPSI_DISPATCHER as the platform independent
   macro.
*/
#define MY_BOOPSI_DISPATCHER(rettype, name, cl, obj, msg)                      \
    rettype name(struct IClass *cl, Object *obj, Msg msg)

#if !defined(__AROS__) && !defined(__MORPHOS__)
/* IPTR is defined as integer type which is large enough to store a pointer */
typedef ULONG IPTR;
#endif

/* the objects of the listview */
extern struct MUI_CustomClass *CL_DataTypes;
#define DataTypesObject (Object*)NewObject(CL_DataTypes->mcc_Class

#define MUIA_DataTypes_Buffer (TAG_USER | 0x30040001)    /* (void *) .S. */
#define MUIA_DataTypes_BufferLen (TAG_USER | 0x30040002) /* (ULONG) .S. */
#define MUIA_DataTypes_FileName (TAG_USER | 0x30040003)  /* (char*) .S. */
#define MUIA_DataTypes_HorizScrollbar                                          \
    (TAG_USER | 0x30040004)                                  /* (Object*) .S. */
#define MUIA_DataTypes_SupportsPrint (TAG_USER | 0x30040005) /* (BOOL) */
#define MUIA_DataTypes_VertScrollbar (TAG_USER | 0x30040006) /* (Object*) .S.  \
                                                              */

#define MUIM_Datatypes_HorizUpdate (TAG_USER | 0x30040101)
#define MUIM_DataTypes_Print (TAG_USER | 0x30040102)
#define MUIM_DataTypes_PrintCompleted (TAG_USER | 0x30040103)
#define MUIM_Datatypes_VertUpdate (TAG_USER | 0x30040104)

int create_datatypes_class(void);
void delete_datatypes_class(void);

#endif