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
** datatypesclass.c
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <devices/printer.h>
#include <intuition/icclass.h>
#include <libraries/mui.h>
#include <SDI_hook.h>

#include <clib/alib_protos.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <proto/datatypes.h>
#include <proto/intuition.h>

#include "datatypesclass.h"

struct DataTypes_Data
{
	Object *dt_obj;
	char *filename; /* Cache the filename */
	int del; /* 1 if filename should be deleted */
	int show; /* 1 if between show / hide */

	Object *horiz_scrollbar;
	Object *vert_scrollbar;

	union printerIO *pio;

	struct MUI_EventHandlerNode ehnode; /* IDCMP_xxx */
	struct MUI_InputHandlerNode ihnode; /* for reaction on the msg port */
};

__attribute__((__saveds__))  ULONG DataTypes_Dispatcher(struct IClass * cl __asm("a0"), Object * obj __asm("a2"), Msg msg __asm("a1"));

STATIC ULONG DataTypes_PrintCompleted(struct IClass *cl, Object *obj);

static union printerIO *CreatePrtReq(void)
{
	union printerIO *pio;
	struct MsgPort *mp;

	if ((mp = CreateMsgPort()))
	{
		if ((pio = (union printerIO *)CreateIORequest(mp, sizeof (union printerIO))))
			return pio;
		DeleteMsgPort(mp);
	}
	return NULL;
}

static void DeletePrtReq(union printerIO * pio)
{
	struct MsgPort *mp;

	mp = pio->ios.io_Message.mn_ReplyPort;
	DeleteIORequest((struct IORequest *)pio);
	DeleteMsgPort(mp);
}

STATIC ULONG DataTypes_New(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct DataTypes_Data *data;

    if (!(obj = (Object *)DoSuperMethod(cl,obj,OM_NEW, MUIA_FillArea, TRUE, TAG_MORE, msg->ops_AttrList)))
		return 0;

	data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	data->dt_obj = NULL;
	data->filename = NULL;

	data->ehnode.ehn_Priority = 1;
	data->ehnode.ehn_Flags    = 0;
	data->ehnode.ehn_Object   = obj;
	data->ehnode.ehn_Class    = NULL;
	data->ehnode.ehn_Events   = IDCMP_IDCMPUPDATE;

	return (ULONG)obj;
}

STATIC VOID DataTypes_Dispose(struct IClass *cl, Object *obj, Msg msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (data->dt_obj) DisposeDTObject(data->dt_obj);
	if (data->filename)
	{
		if (data->del) DeleteFile(data->filename);
		FreeVec(data->filename);
	}

	DoSuperMethodA(cl,obj,msg);
}

static int mystrcmp(const char *str1, const char *str2)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

	return strcmp(str1,str2);
}

static int mystricmp(const char *str1, const char *str2)
{
	if (!str1)
	{
		if (str2) return -1;
		return 0;
	}

	if (!str2) return 1;

#ifdef HAVE_STRCASECMP
	return strcasecmp(str1,str2);
#else
	return stricmp(str1,str2);
#endif
}

/* duplicates the string, allocated with AllocVec() */
static STRPTR StrCopy(CONST_STRPTR str)
{
	STRPTR dest;
	LONG len;
	if (!str) return NULL;

	len = strlen(str);

	if ((dest = (STRPTR)AllocVec(len+1,0)))
	{
		strcpy(dest,str);
	}
	return dest;
}

IPTR xget(Object * obj, ULONG attribute)
{
	IPTR x;
	get(obj, attribute, &x);
	return (x);
}

#if defined(__AMIGAOS4__)

struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, ULONG (*dispatcher)(struct IClass *, Object *, Msg))
{
	struct MUI_CustomClass *cl;

	if ((SysBase->lib_Version > 51) || (SysBase->lib_Version == 51 && SysBase->lib_Revision >= 3))
	{
		cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, (void *)dispatcher);
	} else
	{
		if ((cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, (void *) &muiDispatcherEntry)))
		{
			cl->mcc_Class->cl_UserData = (ULONG)dispatcher;
		}
	}
	return cl;
}
#elif defined(__MORPHOS__)
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, APTR dispatcher)
{
	extern ULONG muiDispatcherEntry(void);

	struct MUI_CustomClass *cl;

	if ((cl = MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, &muiDispatcherEntry)))
	{
		cl->mcc_Class->cl_UserData = (ULONG)dispatcher;
	}
	return cl;
}
#else
struct MUI_CustomClass *CreateMCC(CONST_STRPTR supername, struct MUI_CustomClass *supermcc, int instDataSize, APTR dispatcher)
{
	return MUI_CreateCustomClass(NULL,supername,supermcc,instDataSize, ENTRY(DataTypes_Dispatcher));
}
#endif /* __AMIGAOS4__ || __MORPHOS__ */

STATIC ULONG DataTypes_Set(struct IClass *cl,Object *obj,struct opSet *msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	struct TagItem *tstate, *tag;

	char *newfilename = NULL;
	void *newbuffer = NULL;
	ULONG newbufferlen = 0;

	tstate = (struct TagItem *)msg->ops_AttrList;

	while ((tag = NextTagItem (&tstate)))
	{
		ULONG tidata = tag->ti_Data;

		switch (tag->ti_Tag)
		{
			case	MUIA_DataTypes_FileName:
						if (mystricmp(data->filename, (char*)tidata))
							newfilename = (char*)tidata;
						break;

			case	MUIA_DataTypes_Buffer:
						newbuffer = (void*)tidata;
						break;

			case	MUIA_DataTypes_BufferLen:
						newbufferlen = tidata;
						break;

			case  MUIA_DataTypes_HorizScrollbar:
					  data->horiz_scrollbar = (Object*)tidata;
					  DoMethod(data->horiz_scrollbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, (ULONG)obj, 1, MUIM_Datatypes_HorizUpdate);
					  break;

			case  MUIA_DataTypes_VertScrollbar:
					  data->vert_scrollbar = (Object*)tidata;
					  DoMethod(data->vert_scrollbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime, (ULONG)obj, 1, MUIM_Datatypes_VertUpdate);
					  break;
		}
	}

	if (newfilename || newbuffer)
	{
		char tmpname[L_tmpnam];

		if (data->dt_obj)
		{
			/* Remove the datatype object if it is shown */
			if (data->show) RemoveDTObject(_window(obj),data->dt_obj);

			/* Dispose the datatype object */
			if (data->dt_obj)
			{
				DisposeDTObject(data->dt_obj);
				data->dt_obj = NULL;
			}
		}

		if (data->filename)
		{
			if (data->del) DeleteFile(data->filename);
			FreeVec(data->filename);
			data->del = 0;
		}

		if (newbuffer)
		{
			BPTR out;
			tmpnam(tmpname);

			if ((out = Open(tmpname,MODE_NEWFILE)))
			{
				Write(out,newbuffer,newbufferlen);
				Close(out);
				data->del = 1;
			}

			newfilename = tmpname;
		}
		data->filename = StrCopy(newfilename);

		data->dt_obj = NewDTObject(newfilename, PDTA_DestMode, PMODE_V43, TAG_DONE);

		if (data->dt_obj)
		{
			/* If is between MUIM_Show and MUIM_Hide add the datatype to the window */
			if (data->show)
			{
				SetDTAttrs(data->dt_obj, NULL, NULL,
					GA_Left,		_mleft(obj),
					GA_Top,		_mtop(obj),
					GA_Width,	_mwidth(obj),
					GA_Height,	_mheight(obj),
					ICA_TARGET,	ICTARGET_IDCMP,
					TAG_DONE);
				DoMethod(data->dt_obj, PDTM_SCALE, _mwidth(obj), _mheight(obj), 0);
				AddDTObject(_window(obj), NULL, data->dt_obj, -1);
			}
			MUI_Redraw(obj, MADF_DRAWOBJECT);
		}
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG DataTypes_Get(struct IClass *cl, Object *obj, struct opGet *msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (msg->opg_AttrID == MUIA_DataTypes_SupportsPrint)
	{
		ULONG *m;
		int print = 0;

		if (data->dt_obj)
		{
			for (m = GetDTMethods(data->dt_obj);(*m) != ~0U;m++)
			{
				if ((*m) == DTM_PRINT)
				{
					print = 1;
					break;
				}
			}
		}

		*msg->opg_Storage = print;
		return 1;
	}
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG DataTypes_AskMinMax(struct IClass *cl,Object *obj, struct MUIP_AskMinMax *msg)
{
  DoSuperMethodA(cl, obj, (Msg) msg);

  msg->MinMaxInfo->MinWidth += 20;
  msg->MinMaxInfo->DefWidth += 20;
  msg->MinMaxInfo->MaxWidth = MUI_MAXMAX;

  msg->MinMaxInfo->MinHeight += 40;
  msg->MinMaxInfo->DefHeight += 40;
  msg->MinMaxInfo->MaxHeight = MUI_MAXMAX;
  return 0;
}

STATIC ULONG DataTypes_Setup(struct IClass *cl, Object *obj, struct MUIP_Setup *msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehnode);
	return DoSuperMethodA(cl,obj,(Msg)msg);
}

STATIC ULONG DataTypes_Cleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehnode);
	DoSuperMethodA(cl,obj,msg);
	return 0;
}

STATIC ULONG DataTypes_Show(struct IClass *cl, Object *obj, Msg msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);

	DoSuperMethodA(cl,obj,msg);

	data->show = 1;

	if (data->dt_obj)
	{
		SetDTAttrs(data->dt_obj, NULL, NULL,
				GA_Left,		_mleft(obj),
				GA_Top,		_mtop(obj),
				GA_Width,	_mwidth(obj),
				GA_Height,	_mheight(obj),
				ICA_TARGET,	ICTARGET_IDCMP,
				PDTA_DestMode, PMODE_V43,
				TAG_DONE);
		DoMethod(data->dt_obj, PDTM_SCALE, _mwidth(obj), _mheight(obj), 0);
		AddDTObject(_window(obj), NULL, data->dt_obj, -1);
	}

	return 1;
}

STATIC VOID DataTypes_Hide(struct IClass *cl, Object *obj, Msg msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);

	data->show = 0;

	if (data->dt_obj)
	{
		RemoveDTObject(_window(obj),data->dt_obj);
	}

	DoSuperMethodA(cl,obj,msg);
}

STATIC ULONG DataTypes_Draw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	DoSuperMethodA(cl,obj,(Msg)msg);

	if (msg->flags & MADF_DRAWOBJECT && data->dt_obj)
	{
		RefreshDTObjects(data->dt_obj, _window(obj), NULL, 0);
	}

	return 1;
}

STATIC ULONG DataTypes_HandleEvent(struct IClass *cl, Object *obj, struct MUIP_HandleEvent *msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);

	if (msg->imsg && msg->imsg->Class == IDCMP_IDCMPUPDATE)
	{
		struct TagItem *tstate, *tag;
		struct TagItem vert_tl[4], horiz_tl[4];
		int vert_pos = 0, horiz_pos = 0;

		tstate = (struct TagItem *)msg->imsg->IAddress;

		while ((tag = NextTagItem (&tstate)))
		{
			ULONG tidata = tag->ti_Data;

			switch (tag->ti_Tag)
			{
				case	DTA_Busy:
/*							if (tidata) set(_app(obj), MUIA_Application_Sleep, TRUE);
							else set(_app(obj), MUIA_Application_Sleep, FALSE);*/
							break;

				case	DTA_Sync:
							if (data->show && data->dt_obj)
							{
								RefreshDTObjects (data->dt_obj, _window(obj), NULL, 0);
							}
							break;

				case	DTA_PrinterStatus:
							DataTypes_PrintCompleted(cl, obj);
							break;

				case	DTA_TopVert:
							vert_tl[vert_pos].ti_Tag = MUIA_Prop_First;
							vert_tl[vert_pos++].ti_Data = tidata;
							break;

				case	DTA_TotalVert:
							vert_tl[vert_pos].ti_Tag = MUIA_Prop_Entries;
							vert_tl[vert_pos++].ti_Data = tidata;
							break;

				case 	DTA_VisibleVert:
							vert_tl[vert_pos].ti_Tag = MUIA_Prop_Visible;
							vert_tl[vert_pos++].ti_Data = tidata;
							break;

				case	DTA_TopHoriz:
							horiz_tl[horiz_pos].ti_Tag = MUIA_Prop_First;
							horiz_tl[horiz_pos++].ti_Data = tidata;
							break;

				case	DTA_TotalHoriz:
							horiz_tl[horiz_pos].ti_Tag = MUIA_Prop_Entries;
							horiz_tl[horiz_pos++].ti_Data = tidata;
							break;

				case	DTA_VisibleHoriz:
							horiz_tl[horiz_pos].ti_Tag = MUIA_Prop_Visible;
							horiz_tl[horiz_pos++].ti_Data = tidata;
							break;
			}
		}

		if (vert_pos && data->vert_scrollbar)
		{
			vert_tl[vert_pos].ti_Tag = TAG_DONE;
			SetAttrsA(data->vert_scrollbar,vert_tl);
		}

		if (horiz_pos && data->horiz_scrollbar)
		{
			horiz_tl[horiz_pos].ti_Tag = TAG_DONE;
			SetAttrsA(data->horiz_scrollbar,horiz_tl);
		}
	}
	return 0;
}

STATIC ULONG DataTypes_Print(struct IClass *cl, Object *obj, Msg msg)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (data->dt_obj && !data->pio)
	{
    if ((data->pio = CreatePrtReq()))
    {
			if (OpenDevice("printer.device", 0 /* should be user selectable */, (struct IORequest *)data->pio, 0) == 0)
			{
				struct TagItem tags[2];

				tags[0].ti_Tag   = data->show?DTA_RastPort:TAG_IGNORE;
				tags[0].ti_Data  = (ULONG)(data->show?_rp(obj):NULL);
				tags[1].ti_Tag   = TAG_DONE;

				if (PrintDTObject(data->dt_obj, data->show?_window(obj):FALSE, NULL, DTM_PRINT, NULL, data->pio, tags))
				{
/*
					data->ihnode.ihn_Object = obj;
					data->ihnode.ihn_Signals = 1UL << (data->pio->ios.io_Message.mn_ReplyPort);
					data->ihnode.ihn_Flags = 0;
					data->ihnode.ihn_Method = MUIM_DataTypes_PrintCompleted;
					DoMethod(_app(obj),MUIM_Application_AddInputHandler,&data->ihnode);*/
					return TRUE;
				}
		    CloseDevice((struct IORequest *)data->pio);
			}
			DeletePrtReq(data->pio);
			data->pio = NULL;
    }
	}
	return FALSE;
}

STATIC ULONG DataTypes_PrintCompleted(struct IClass *cl, Object *obj)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (data->pio)
	{
		CloseDevice((struct IORequest *)data->pio);
		DeletePrtReq(data->pio);
		data->pio = NULL;
	}
	return 0;
}

STATIC ULONG DataTypes_VertUpdate(struct IClass *cl, Object *obj)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (!data->dt_obj) return 0;
	SetGadgetAttrs((struct Gadget*)data->dt_obj, _window(obj), NULL, DTA_TopVert, xget(data->vert_scrollbar,MUIA_Prop_First));
	return 0;
}

STATIC ULONG DataTypes_HorizUpdate(struct IClass *cl, Object *obj)
{
	struct DataTypes_Data *data = (struct DataTypes_Data*)INST_DATA(cl,obj);
	if (!data->dt_obj) return 0;
	SetGadgetAttrs((struct Gadget*)data->dt_obj, _window(obj), NULL, DTA_TopHoriz, xget(data->horiz_scrollbar,MUIA_Prop_First));
	return 0;
}

DISPATCHER(DataTypes_Dispatcher)
{
	switch(msg->MethodID)
	{
		case	OM_NEW:				return DataTypes_New(cl,obj,(struct opSet*)msg);
		case	OM_DISPOSE:		  DataTypes_Dispose(cl,obj,msg); return 0;
		case	OM_SET:				  return DataTypes_Set(cl,obj,(struct opSet*)msg);
		case	OM_GET:				return DataTypes_Get(cl,obj,(struct opGet*)msg);
		case	MUIM_AskMinMax: return DataTypes_AskMinMax(cl,obj,(struct MUIP_AskMinMax *)msg);
		case	MUIM_Setup:		return DataTypes_Setup(cl,obj,(struct MUIP_Setup*)msg);
		case	MUIM_Cleanup:	return DataTypes_Cleanup(cl,obj,msg);
		case	MUIM_Show:			return DataTypes_Show(cl,obj,msg);
		case	MUIM_Hide:			DataTypes_Hide(cl,obj,msg);return 0;
		case	MUIM_Draw:			return DataTypes_Draw(cl,obj,(struct MUIP_Draw*)msg);
		case	MUIM_HandleEvent: return DataTypes_HandleEvent(cl,obj,(struct MUIP_HandleEvent*)msg);
		case	MUIM_DataTypes_Print: return DataTypes_Print(cl,obj,msg);
		case	MUIM_DataTypes_PrintCompleted: return DataTypes_PrintCompleted(cl,obj);
		case	MUIM_Datatypes_VertUpdate: return DataTypes_VertUpdate(cl,obj);
		case	MUIM_Datatypes_HorizUpdate: return DataTypes_HorizUpdate(cl,obj);

		default: return DoSuperMethodA(cl,obj,msg);
	}
}

struct MUI_CustomClass *CL_DataTypes;

int create_datatypes_class(void)
{
	if ((CL_DataTypes = MUI_CreateCustomClass(NULL,MUIC_Area,NULL,sizeof(struct DataTypes_Data),ENTRY(DataTypes_Dispatcher))))
	{
        CL_DataTypes->mcc_Class->cl_ID = (ClassID)"DataTypes";
        return 1;
	}
    return 0;
}

void delete_datatypes_class(void)
{
	if (CL_DataTypes)
	{
		if (MUI_DeleteCustomClass(CL_DataTypes))
		{
			CL_DataTypes = NULL;
		}
	}
}