//=============================================================================
// IPtracer.c
// Программа трассировки пути в сетях TCP/IP
//=============================================================================
#define INCL_WIN
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSDATETIME

#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <types.h>
#include <sys\socket.h>
#include <net\if.h>
#include <arpa\inet.h>
#include <unistd.h>
#include <netinet\in_systm.h>
#include <netinet\ip.h>
#include <netinet\ip_var.h>
#include <netinet\ip_icmp.h>
#include <netinet\icmp_var.h>
#include <sys\select.h>
#include <netinet\udp.h>
#include "IPtracer.h"

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
MRESULT EXPENTRY ClientWndProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY DlgProcOpt(HWND, ULONG, MPARAM, MPARAM); // Dlg proc (Options)
void InitContainer(HWND, BOOL);
void InsertRecord(HWND, int);
void APIENTRY DoTrace(ULONG);
BOOL resolv(HWND, char *, struct sockaddr_in *);
void SendWngMsg(HWND, char *);
BOOL PgmTrace(HWND, int, int, struct sockaddr_in *, int);
void GetInfo(void *);

//-----------------------------------------------------------------------------
// Global Variablies
//-----------------------------------------------------------------------------
char     PgmName[]     = "IPtracer",  PrfName[]    = "IPtracer.INI",
         WindowPos[]   = "WindowPos", SliderPos[]  = "SliderPos",
         PingTOid[]    = "PingTO",    WhoisTOid[]  = "WhoisTO",
         useDNSid[]    = "useDNS",    useWHOISid[] = "useWHOIS",
         WhoisNameId[] = "WhoisId",   FileName[]   = "IPtrace.Log",
         WhoisName[NameLen],          TraceName[NameLen];
PVOID    ptr;
ULONG    ulSize, // Size of the data to be copied
         PingTO = L5, WhoisTO = L5;
HWND     hwndFrame, SaveHwnd;
HAB      hab;
FILE     *FileOut;
BOOL     useDNS = TRUE, useWHOIS = TRUE, TraceFlag = TRUE;
typedef struct _USERRECORD
  { RECORDCORE  recordCore;
    PSZ         Number;
    PSZ         IPaddress;
    PSZ         RespTime;
    PSZ         IPname;
  }    USERRECORD, *PUSERRECORD;
char     *NumberF, *IPaddressF, *RespTimeF, *IPnameF;
HEV      hevEventHandle, hevEventH;
ULONG    ulPostCnt, Post;
TID      tid = L0;
long     ClrPaleGray = CLR_PALEGRAY, ClrDarkBlue = CLR_DARKBLUE,
         ClrBlack    = CLR_BLACK,    ClrWhite    = CLR_WHITE,
         ClrCyan     = CLR_CYAN,     ClrGreen    = CLR_GREEN;
struct   sockaddr_in myaddr, whoaddr;
char     ErrMsg[L64];
u_short  PortNum = MinPort, RspTimeF[MAXTTL];
HPOINTER hIcon;
HTIMER   phtimer;
char     code00[] = "Network unreachable";
char     code01[] = "Host unreachable";
char     code02[] = "Protocol unreachable";
char     code04[] = "Fragmentation needed";
char     code05[] = "Source route failed";
char     code06[] = "Network unknown";
char     code07[] = "Host unknown";
char     code08[] = "Source host isolated";
char     code09[] = "Network prohibited";
char     code10[] = "Host prohibited";
char     code11[] = "Network unreachable for TOS";
char     code12[] = "Host unreachable for TOS";
char     code13[] = "Communication prohibited";
char     code14[] = "Host precedence violation";
char     code15[] = "Precedence cutoff in effect";
char     *ptrMsg[] = {code00,code01,code02,NULL,code04, code05,code06,code07,
                      code08,code09,code10,code11,code12,code13,code14,code15};
char     *strbuf[MAXTTL], strMLE[MaxWhoLen];
BOOL     InfoFlag[MAXTTL];
int      Sel;

//=============================================================================
// Main procedure
//=============================================================================
void main(void)
  {
  HMQ   hmq;
  QMSG  qmsg;
  ULONG FrameFlags = FCF_STANDARD | FCF_AUTOICON;
//-----------------------------------------------------------------------------
// Initialize application and create message queue
//-----------------------------------------------------------------------------
  hab = WinInitialize(L0);
  hmq = WinCreateMsgQueue(hab, L0);
  FileOut=fopen(FileName, "a");
//-----------------------------------------------------------------------------
// Register class and create window
//-----------------------------------------------------------------------------
  WinRegisterClass(hab, PgmName, ClientWndProc, CS_SIZEREDRAW, L0);
  hwndFrame = WinCreateStdWindow(HWND_DESKTOP,     // Parent
                                 L0,               // Style (unvisible)
                                 &FrameFlags,      // Creation flags
                                 PgmName,          // Class name
                                 Title,            // Titlebar text
                                 L0,               // Client style
                                 NULLHANDLE,       // Resource in .EXE file
                                 ID_RESOURCE,      // Resource ID
                                 NULL);            // Client handle
//-----------------------------------------------------------------------------
// Message loop
//-----------------------------------------------------------------------------
  while (WinGetMsg (hab, &qmsg, L0, L0, L0)) WinDispatchMsg (hab, &qmsg);
//-----------------------------------------------------------------------------
// Clean up (destroy window, queue and hab)
//-----------------------------------------------------------------------------
  WinDestroyWindow(hwndFrame);
  WinDestroyMsgQueue(hmq);
  WinTerminate(hab);
  fclose(FileOut);
  }

//=============================================================================
// Window procedure
//=============================================================================
MRESULT EXPENTRY ClientWndProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
  {
  static HWND   hwndPBstart, hwndPBbreak, hwndPBexit, hwndSlider, hwndText,
                hwndEField,  hwndCntnr,   hwndMLE;
  static BOOL   HavePos = FALSE, FirstSize = TRUE;
  static USHORT SaveSlider = L0;
  static HINI   hini;

  switch (msg)
    {
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Fill client with default color
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_ERASEBACKGROUND:
      return MRFROMSHORT(TRUE);
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// WM_CREATE occurs only during creation
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_CREATE:
      {
      char Font[] = FontName;
      BOOL HaveParm = FALSE;

      SaveHwnd = hwnd;
      hwndPBstart = WinCreateWindow(hwnd, WC_BUTTON, "~Start",
                      BS_PUSHBUTTON | WS_VISIBLE,
                      L0, L0, L0, L0, hwnd, HWND_TOP, PB_START, L0, L0);
      hwndPBbreak = WinCreateWindow(hwnd, WC_BUTTON, "~Break",
                      BS_PUSHBUTTON | WS_VISIBLE | WS_DISABLED,
                      L0, L0, L0, L0, hwnd, HWND_TOP, PB_BREAK, L0, L0);
      hwndPBexit  = WinCreateWindow(hwnd, WC_BUTTON, "~Exit",
                      BS_PUSHBUTTON | WS_VISIBLE,
                      L0, L0, L0, L0, hwnd, HWND_TOP, PB_EXIT, L0, L0);
      hwndSlider  = WinCreateWindow(hwnd, WC_SLIDER, "",
                      SLS_HORIZONTAL | SLS_TOP | WS_VISIBLE,
                      L0, L0, L0, L0, hwnd, HWND_TOP, ID_SLIDER, L0, L0);
      hwndText    = WinCreateWindow(hwnd, WC_STATIC,
                      "Enter Hostname or IP address",
                      WS_VISIBLE | SS_TEXT | DT_CENTER | DT_VCENTER,
                      L0, L0, L0, L0, hwnd, HWND_TOP, ID_TEXT, L0, L0);
      hwndEField  = WinCreateWindow(hwnd, WC_ENTRYFIELD, "",
                      WS_VISIBLE | ES_AUTOSCROLL | ES_LEFT,
                      L0, L0, L0, L0, hwnd, HWND_TOP, ID_EFIELD, L0, L0);
      hwndCntnr = WinCreateWindow(hwnd, WC_CONTAINER, NULL,
                    CCS_READONLY | CCS_SINGLESEL | WS_VISIBLE,
                    L0, L0, L0, L0, hwnd, HWND_TOP, ID_CONTAINER, L0, L0);
      hwndMLE   = WinCreateWindow(hwnd, WC_MLE, NULL,
                    MLS_BORDER | MLS_READONLY | WS_VISIBLE |
                    MLS_VSCROLL | MLS_HSCROLL | MLS_LIMITVSCROLL,
                    L0, L0, L0, L0, hwnd, HWND_TOP, ID_MLE, L0, L0);
      memset(strMLE, L0, MaxWhoLen);
      WinSendMsg(hwndMLE, MLM_SETIMPORTEXPORT,
                 strMLE, MPFROMLONG((ULONG)MaxWhoLen));
//-----------------------------------------------------------------------------
// Восстановим параметры из .INI файла
//-----------------------------------------------------------------------------
      if ( (hini = PrfOpenProfile(hab, PrfName)) != NULLHANDLE )
        {
        if ( PrfQueryProfileSize(hini, PgmName, SliderPos, &ulSize) )
          if ( ulSize <= sizeof(SaveSlider) )
            HavePos = PrfQueryProfileData(hini, PgmName, SliderPos,
                                          &SaveSlider, &ulSize);

        GetParm(useDNSid, useDNS);
        GetParm(useWHOISid, useWHOIS);
        GetParm(PingTOid, PingTO);
        PingTO = ( PingTO == L0 ) ? L1 : PingTO;
        PingTO = ( PingTO > MaxTO ) ? MaxTO : PingTO;
        GetParm(WhoisTOid, WhoisTO);
        WhoisTO = ( WhoisTO == L0 ) ? L1 : WhoisTO;
        WhoisTO = ( WhoisTO > MaxTO ) ? MaxTO : WhoisTO;
        memset(WhoisName, L0, NameLen);
        GetParm(WhoisNameId, WhoisName);
        if ( strlen(WhoisName) == L0 ) strcpy(WhoisName, WhoIsServer);

        if ( PrfQueryProfileSize(hini, PgmName, WindowPos, &ulSize) )
          {
          ptr = malloc(ulSize);
          PrfQueryProfileData(hini, PgmName, WindowPos, ptr, &ulSize);
          PrfWriteProfileData(HINI_USERPROFILE, PgmName, WindowPos,
                              ptr, ulSize);
          free(ptr);
          HaveParm = WinRestoreWindowPos(PgmName, WindowPos,
                       WinQueryWindow(hwnd, QW_PARENT));
          }
        PrfCloseProfile(hini);
        }
      if ( !HaveParm )
        WinSetWindowPos(WinQueryWindow(hwnd, QW_PARENT), HWND_TOP,
          X, Y, CX, CY, SWP_ACTIVATE | SWP_MOVE | SWP_SIZE | SWP_SHOW);
//-----------------------------------------------------------------------------
// Установим презентационные параметры
//-----------------------------------------------------------------------------
      WinSetPresParam(hwnd, PP_FONTNAMESIZE, sizeof(Font), Font);
      WinSetPresParam(hwnd, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwnd, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndPBstart, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwndPBstart, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndPBbreak, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwndPBbreak, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndPBexit, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwndPBexit, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndSlider, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwndSlider, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndText, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrPaleGray), (PVOID)&ClrPaleGray);
      WinSetPresParam(hwndText, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrDarkBlue), (PVOID)&ClrDarkBlue);
      WinSetPresParam(hwndEField, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrGreen), (PVOID)&ClrGreen);
      WinSetPresParam(hwndEField, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSendMsg(hwndEField, EM_SETTEXTLIMIT,
                 (MPARAM)(NameLen-L1), (MPARAM)L0);
      WinSetPresParam(hwndCntnr, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrWhite), (PVOID)&ClrWhite);
      WinSetPresParam(hwndCntnr, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndCntnr, PP_HILITEBACKGROUNDCOLORINDEX,
                      sizeof(ClrCyan), (PVOID)&ClrCyan);
      WinSetPresParam(hwndCntnr, PP_HILITEFOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      WinSetPresParam(hwndMLE, PP_BACKGROUNDCOLORINDEX,
                      sizeof(ClrWhite), (PVOID)&ClrWhite);
      WinSetPresParam(hwndMLE, PP_FOREGROUNDCOLORINDEX,
                      sizeof(ClrBlack), (PVOID)&ClrBlack);
      hIcon = (HPOINTER)WinLoadPointer(HWND_DESKTOP, NULLHANDLE, L1);
      InitContainer(hwndCntnr, useDNS);
      WinSetFocus(HWND_DESKTOP, hwndEField);  // Установим фокус
//-----------------------------------------------------------------------------
// Set Priority for current Thread, Create Semaphor and Thread
//-----------------------------------------------------------------------------
      DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, L16, L0);
      DosCreateEventSem((ULONG)NULL, &hevEventHandle, DC_SEM_SHARED, FALSE);
      DosCreateThread(&tid, (PFNTHREAD)DoTrace, hwnd,
                   CREATE_READY | STACK_SPARSE, L65536);
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// WM_SIZE occurs during every resize, size setting event
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_SIZE:
      {
      MRESULT mresult;
      USHORT OldPos, OldLen, NewPos, NewLen;
      int WinCX = SHORT1FROMMP(mp2);
      int WinCY = SHORT2FROMMP(mp2);
      int x = (WinCX-(L3*PB_CX))/L4;

      WinSetWindowPos(hwndPBstart, HWND_TOP, x, Y_OFF, PB_CX, PB_CY,
                      SWP_SIZE | SWP_MOVE);
      WinSetWindowPos(hwndPBbreak, HWND_TOP, L2*x+PB_CX, Y_OFF, PB_CX, PB_CY,
                      SWP_SIZE | SWP_MOVE);
      WinSetWindowPos(hwndPBexit, HWND_TOP, L3*x+L2*PB_CX, Y_OFF, PB_CX, PB_CY,
                      SWP_SIZE | SWP_MOVE);

      mresult = WinSendMsg(hwndSlider, SLM_QUERYSLIDERINFO,
                           MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
                           NULL);
      OldPos=SHORT1FROMMP(mresult);
      if ( (OldLen = SHORT2FROMMP(mresult)) == L0 ) OldLen = L1;
      WinSetWindowPos(hwndSlider, HWND_TOP, L0, L2*Y_OFF+PB_CY, WinCX, SL_CY,
                      SWP_SIZE | SWP_MOVE);
      mresult = WinSendMsg(hwndSlider, SLM_QUERYSLIDERINFO,
                           MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
                           NULL);
      NewLen=SHORT2FROMMP(mresult);
      if ( FirstSize )
        {
        FirstSize = FALSE;
        if ( HavePos ) NewPos = SaveSlider;
        else NewPos = NewLen/L2;
        }
      else NewPos=NewLen*OldPos/OldLen;
      WinSendMsg(hwndSlider, SLM_SETSLIDERINFO,
                 MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
                 MPFROM2SHORT(NewPos, NewLen));

      WinSetWindowPos(hwndText, HWND_TOP, L0, WinCY-TX_CY, WinCX, TX_CY,
                      SWP_SIZE | SWP_MOVE);
      WinSetWindowPos(hwndEField, HWND_TOP, L0, WinCY-TX_CY-EF_CY,
                      WinCX, EF_CY, SWP_SIZE | SWP_MOVE);
      WinSetWindowPos(hwndCntnr, HWND_TOP, L0, L2*Y_OFF+PB_CY+SL_CY,
                      NewPos, WinCY-TX_CY-EF_CY-L2*Y_OFF-PB_CY-SL_CY,
                      SWP_SIZE | SWP_MOVE);
      WinSetWindowPos(hwndMLE, HWND_TOP, NewPos, L2*Y_OFF+PB_CY+SL_CY,
                      WinCX-NewPos, WinCY-TX_CY-EF_CY-L2*Y_OFF-PB_CY-SL_CY,
                      SWP_SIZE | SWP_MOVE);
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Сохраним параметры при выходе
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_SAVEAPPLICATION:
      {
      MRESULT mresult;
//-----------------------------------------------------------------------------
// Check if window is minimized and restore to original size
//-----------------------------------------------------------------------------
      if ( WinQueryWindowULong(hwndFrame, QWL_STYLE) & WS_MINIMIZED )
        WinSetWindowPos(hwndFrame, HWND_TOP, L0, L0, L0, L0, SWP_RESTORE);
//-----------------------------------------------------------------------------
// Store window information in OS2.INI
//-----------------------------------------------------------------------------
      WinStoreWindowPos(PgmName, WindowPos, WinQueryWindow(hwnd, QW_PARENT));
//-----------------------------------------------------------------------------
// Copy the Window position info from the OS2.INI into private INI file
//-----------------------------------------------------------------------------
      PrfQueryProfileSize(HINI_USERPROFILE, PgmName, WindowPos, &ulSize);
      ptr = malloc(ulSize);
      PrfQueryProfileData(HINI_USERPROFILE, PgmName, WindowPos, ptr, &ulSize);
      PrfWriteProfileData(HINI_USERPROFILE, PgmName, NULL, NULL, L0);
      hini = PrfOpenProfile(hab, PrfName);
      PrfWriteProfileData(hini, PgmName, WindowPos, ptr, ulSize);
      mresult = WinSendMsg(hwndSlider, SLM_QUERYSLIDERINFO,
                           MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
                           NULL);
      SaveSlider=SHORT1FROMMP(mresult);
      PrfWriteProfileData(hini, PgmName, SliderPos, &SaveSlider,
                          sizeof(SaveSlider));
      PrfWriteProfileData(hini,PgmName,PingTOid,&PingTO,sizeof(PingTO));
      PrfWriteProfileData(hini,PgmName,WhoisTOid,&WhoisTO,sizeof(WhoisTO));
      PrfWriteProfileData(hini,PgmName,useDNSid,&useDNS,sizeof(useDNS));
      PrfWriteProfileData(hini,PgmName,useWHOISid,&useWHOIS,sizeof(useWHOIS));
      PrfWriteProfileData(hini,PgmName,WhoisNameId,&WhoisName,NameLen);

      PrfCloseProfile(hini);
      free(ptr);
      break;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Трассировка завершена
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_USER_TRACE_DONE:
      {
      TraceFlag = TRUE;
      WinEnableControl(hwnd, PB_BREAK, FALSE);
      WinEnableControl(hwnd, PB_START, TRUE);
      WinInvalidateRegion(hwnd, NULLHANDLE, TRUE); // обновим окно
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Запись об очередном узле завершена
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_USER_LINE_DONE:
      {
      InsertRecord(hwnd, LONGFROMMP(mp1));
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Получена информация от Whois сервера
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_USER_INFO_DONE:
      {
      static IPT Offset = L0;
      static ULONG LenMLE = MaxWhoLen;

      if ( Sel != LONGFROMMP(mp1) ) return L0;

      WinSendMsg(hwndMLE,MLM_DELETE,MPFROMLONG(L0),MPFROMLONG(MaxWhoLen));
      memcpy(strMLE, strbuf[Sel], MaxWhoLen);
      WinSendMsg(hwndMLE, MLM_IMPORT, &Offset, &LenMLE);
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Произошла ошибка
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_USER_TRACE_ERROR:
      {
      SendWngMsg(hwnd, ErrMsg);
      return L0;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Handling of the menu-items and the button by WM_COMMAND
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_COMMAND:
      {
      switch(SHORT1FROMMP(mp1))
        {
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Close the dialog
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case PB_EXIT:
        case IDM_EXIT:
          {
          WinPostMsg(hwnd, WM_CLOSE, L0, L0);
          break;
          }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Выполнить трассировку
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case PB_START:
        case IDM_START:
          {
          static SWP swp;
          static MRESULT mresult;
          static ULONG Post;

          DosQueryEventSem(hevEventHandle, &Post);
          if ( Post ) return L0;

          WinPostMsg(WinWindowFromID(hwnd, ID_CONTAINER),
                     CM_REMOVEDETAILFIELDINFO, NULL,
                     MPFROM2SHORT(L0, CMA_FREE | CMA_INVALIDATE));

          WinDestroyWindow(hwndCntnr); // Заново создадим контейнер
          hwndCntnr = WinCreateWindow(hwnd, WC_CONTAINER, NULL,
                        CCS_READONLY | CCS_SINGLESEL | WS_VISIBLE,
                        L0, L0, L0, L0, hwnd, HWND_TOP, ID_CONTAINER, L0, L0);
          WinQueryWindowPos(hwnd, (PSWP)&swp);
          mresult = WinSendMsg(hwndSlider, SLM_QUERYSLIDERINFO,
                           MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_RANGEVALUE),
                           NULL);
          WinSetWindowPos(hwndCntnr, HWND_TOP, L0, L2*Y_OFF+PB_CY+SL_CY,
                          SHORT1FROMMP(mresult),
                          swp.cy-TX_CY-EF_CY-L2*Y_OFF-PB_CY-SL_CY,
                          SWP_SIZE | SWP_MOVE);
          WinSetPresParam(hwndCntnr, PP_BACKGROUNDCOLORINDEX,
                          sizeof(ClrWhite), (PVOID)&ClrWhite);
          WinSetPresParam(hwndCntnr, PP_FOREGROUNDCOLORINDEX,
                          sizeof(ClrBlack), (PVOID)&ClrBlack);
          WinSetPresParam(hwndCntnr, PP_HILITEBACKGROUNDCOLORINDEX,
                          sizeof(ClrCyan), (PVOID)&ClrCyan);
          WinSetPresParam(hwndCntnr, PP_HILITEFOREGROUNDCOLORINDEX,
                          sizeof(ClrBlack), (PVOID)&ClrBlack);
          InitContainer(hwndCntnr, useDNS);

          WinSendMsg(hwndMLE,MLM_DELETE,MPFROMLONG(L0),MPFROMLONG(MaxWhoLen));

          memset(TraceName, L0, NameLen);
          if ( WinQueryWindowText(hwndEField, NameLen, TraceName) == L0 )
            {
            SendWngMsg(hwnd, "Empty IP address field");
            return L0;
            }

          if ( !resolv(hwnd, TraceName, &myaddr) )
            {
            SendWngMsg(hwnd, "Invalid IP address or Hostname");
            return L0;
            }

          if ( useWHOIS )
            if ( !resolv(hwnd, WhoisName, &whoaddr) )
              {
              SendWngMsg(hwnd, "Invalid Who Is Server");
              return L0;
              }
          whoaddr.sin_family = AF_INET;
          whoaddr.sin_port = htons(WhoIsPort);

          WinEnableControl(hwnd, PB_START, FALSE);
          WinEnableControl(hwnd, PB_BREAK, TRUE);
          DosPostEventSem(hevEventHandle);
          return L0;
          }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Остановим трассировку
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case PB_BREAK:
        case IDM_BREAK:
          {
          static ULONG Post;

          DosQueryEventSem(hevEventHandle, &Post);
          if ( !Post ) return L0;
          TraceFlag = FALSE;
          return L0;
          }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Обработаем пункт меню OPTIONS
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case IDM_OPTIONS:
          {
          WinDlgBox (HWND_DESKTOP, // Parent
                     hwnd,         // Owner
                     DlgProcOpt,   // Dialog window procedure
                     NULLHANDLE,   // Where is dialog resource?
                     ID_OPTIONS,   // Dialog Resource ID
                     L0);          // Create parms (for WM_INITDLG)
          return L0;
          }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Расскажем о себе
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
        case IDM_ABOUT:
          {
          WinDlgBox(HWND_DESKTOP, hwnd, WinDefDlgProc,
                    NULLHANDLE, ID_ABOUT, L0);
          return L0;
          }
        }
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Обработка меню
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_INITMENU:
      {
      switch (SHORT1FROMMP(mp1))
        {
        case IDSM_FILE:
          {
          static ULONG Post;

          DosQueryEventSem( hevEventHandle, &Post );
          WinEnableMenuItem((HWND)mp2, IDM_START, Post == L0);
          WinEnableMenuItem((HWND)mp2, IDM_BREAK, Post != L0);
          return L0;
          }
        }
      break;
      }
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Handling of WM_CONTROL
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
    case WM_CONTROL:
      {
      switch ( SHORT2FROMMP(mp1) )
        {
        case SLN_CHANGE:
          {
          static SWP swp;

          WinQueryWindowPos(hwnd, (PSWP)&swp);
          WinSetWindowPos(hwndCntnr, HWND_TOP, L0, L2*Y_OFF+PB_CY+SL_CY,
                          LONGFROMMP(mp2),
                          swp.cy-TX_CY-EF_CY-L2*Y_OFF-PB_CY-SL_CY,
                          SWP_SIZE | SWP_MOVE);
          WinSetWindowPos(hwndMLE, HWND_TOP, LONGFROMMP(mp2),
                          L2*Y_OFF+PB_CY+SL_CY, swp.cx-LONGFROMMP(mp2),
                          swp.cy-TX_CY-EF_CY-L2*Y_OFF-PB_CY-SL_CY,
                          SWP_SIZE | SWP_MOVE);
          return L0;
          }
//-----------------------------------------------------------------------------
// Обработаем текущую ззапись
//-----------------------------------------------------------------------------
        case CN_EMPHASIS:
          {
          static int OldSel = L0;
          PNOTIFYRECORDEMPHASIS Selected = (PNOTIFYRECORDEMPHASIS)mp2;
          static IPT Offset = L0;
          static ULONG LenMLE = MaxWhoLen;

          if ( CRA_SELECTED & Selected->fEmphasisMask )
            Sel=atoi(Selected->pRecord->pszIcon)-L1;
          if ( OldSel == Sel ) return L0;
          OldSel = Sel;
          WinSendMsg(hwndMLE,MLM_DELETE,MPFROMLONG(L0),MPFROMLONG(MaxWhoLen));
          memcpy(strMLE, strbuf[Sel], MaxWhoLen);
          WinSendMsg(hwndMLE, MLM_IMPORT, &Offset, &LenMLE);
          return L0;
          }
        }
      break;
      }
    }
//-----------------------------------------------------------------------------
  return (WinDefWindowProc (hwnd,msg,mp1,mp2));
  }

//=============================================================================
// DoTrace - подпрограмма трассировки
//=============================================================================
void APIENTRY DoTrace(ULONG parmHwnd)
  {
  int      i, Num, sock, sockICMP;
  BOOL     result;
  DATETIME DatTime;
  struct   in_addr *inadr = (struct in_addr *)&myaddr.sin_addr.s_addr;

  DosSetPriority(PRTYS_THREAD, PRTYC_REGULAR, L15, L0);
  DosCreateEventSem((ULONG)NULL, &hevEventH, DC_SEM_SHARED, FALSE);

  NumberF = malloc(MAXTTL*L4);
  IPaddressF = malloc(MAXTTL*L16);
  RespTimeF = malloc(MAXTTL*L6);
  IPnameF = malloc(MAXTTL*NameLen);

  for ( i=L0; i<MAXTTL; i++ ) strbuf[i]=malloc(MaxWhoLen);

  for (;;)
    {
    DosWaitEventSem(hevEventHandle, SEM_INDEFINITE_WAIT);
//-----------------------------------------------------------------------------
// Приступим к трассировке
//-----------------------------------------------------------------------------
    sock = socket(AF_INET, SOCK_DGRAM, L0);
    sockICMP = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    memset(NumberF, L0, MAXTTL*L4);
    memset(IPaddressF, L0, MAXTTL*L16);
    memset(RespTimeF, L0, MAXTTL*L6);
    memset(IPnameF, L0, MAXTTL*NameLen);
    memset((char *)RspTimeF, L0, sizeof(RspTimeF));
    for ( i=L0; i<MAXTTL; i++ )
      {
      memset(strbuf[i], L0, MaxWhoLen);
      InfoFlag[i] = FALSE;
      }

    DosGetDateTime(&DatTime);
    fprintf(FileOut, "%s   IP=%s   %d/%2.2d/%d   %2.2d:%2.2d:%2.2d\n",
            Title, inet_ntoa(*inadr),
            DatTime.day, DatTime.month, DatTime.year,
            DatTime.hours, DatTime.minutes, DatTime.seconds);
    fprintf(FileOut, " No   IP address        Time(ms)   Hostname\n");
//-----------------------------------------------------------------------------
// Собственно трассировка
//-----------------------------------------------------------------------------
    for ( Num=L0, result=TRUE; (Num<MAXTTL)&&TraceFlag&&result; Num++ )
      {
      DosResetEventSem(hevEventH, &Post);
      DosPostEventSem(hevEventH);
      result=PgmTrace(parmHwnd, sock, sockICMP, &myaddr, Num);
      DosStopTimer(phtimer);
      WinPostMsg(parmHwnd, WM_USER_LINE_DONE, MPFROMLONG(Num), L0);
      fprintf(FileOut, "%3d   %-15s    %5d     %s\n", Num+L1,
              IPaddressF+L16*Num, RspTimeF[Num], IPnameF+NameLen*Num);
      if ( *(IPaddressF+L16*Num) == '*' ) continue;
      if ( useWHOIS )
        {
        InfoFlag[Num] = TRUE;
        _beginthread(GetInfo, L0, L65536, (void *)Num);
        }
      }
//-----------------------------------------------------------------------------
// Завершение трассировки
//-----------------------------------------------------------------------------
    soclose(sock);
    soclose(sockICMP);
    for ( ; i<MAXTTL; i++ )
      if ( InfoFlag[i] )
        {
        DosSleep(L1000); // ждем 1 секунду
        i = L0;
        continue;
        }

    DosResetEventSem(hevEventHandle, &ulPostCnt);
    fprintf(FileOut, "\n");
    WinPostMsg(parmHwnd, WM_USER_TRACE_DONE, L0, L0);
    }
  }

//=============================================================================
// GetInfo - подпрограмма получения информации от Who Is сервера
//=============================================================================
void GetInfo(void *arg)
  {
  int sock, Numb;
  struct sockaddr_in whoisaddr;
  char query[L128];
  fd_set r;
  struct timeval mytimeout;

  Numb = (int)arg;

  sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  FD_ZERO(&r);
  FD_SET(sock, &r);
  mytimeout.tv_sec = WhoisTO;
  mytimeout.tv_usec = L0;

  memcpy((char *)&whoisaddr, (char *)&whoaddr, sizeof(whoisaddr));
  connect(sock, (struct sockaddr *)&whoisaddr, sizeof(struct sockaddr));
  sprintf(query, "%s\r\n", IPaddressF+L16*Numb);
  send(sock, query, strlen(query), L0);

  if ( select(sock+L1, &r, NULL, NULL, &mytimeout) > L0 )
    {
    recv(sock, strbuf[Numb], MaxWhoLen, L0);
    WinPostMsg(SaveHwnd, WM_USER_INFO_DONE, MPFROMLONG(Numb), L0);
    }

  soclose(sock);
  InfoFlag[Numb] = FALSE;
  }

//=============================================================================
// InitContainer - подпрограмма инициализации контейнера
//=============================================================================
void InitContainer(HWND hwnd, BOOL DNS)
{
  static char pszCnrTitle[] = "Trace result";
  static CNRINFO cnrinfo;
  static PFIELDINFO pFieldInfo, firstFieldInfo;
  static FIELDINFOINSERT fieldInfoInsert;
  static PFIELDINFOINSERT pFieldInfoInsert;
  static char pszColumnText1[]= "No.";
  static char pszColumnText2[]= "IP address";
  static char pszColumnText3[]= "Time (ms)";
  static char pszColumnText4[]= "Host name";
  ULONG MsgFlg = CMA_FLWINDOWATTR | CMA_CNRTITLE;
  long NumCol;

  cnrinfo.pszCnrTitle = pszCnrTitle;
  cnrinfo.flWindowAttr = CV_DETAIL | CA_CONTAINERTITLE |
                         CA_TITLESEPARATOR | CA_DETAILSVIEWTITLES;
  NumCol = ( DNS ) ? L4 : L3;

  pFieldInfo=WinSendMsg(hwnd,CM_ALLOCDETAILFIELDINFO,MPFROMLONG(NumCol),NULL);
  firstFieldInfo = pFieldInfo;

  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING|CFA_HORZSEPARATOR|CFA_RIGHT|CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = (PVOID) pszColumnText1;
  pFieldInfo->offStruct = FIELDOFFSET(USERRECORD, Number);
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flData = CFA_STRING|CFA_HORZSEPARATOR|CFA_LEFT|CFA_SEPARATOR;
  pFieldInfo->flTitle = CFA_LEFT;
  pFieldInfo->pTitleData = (PVOID) pszColumnText2;
  pFieldInfo->offStruct = FIELDOFFSET(USERRECORD, IPaddress);
  pFieldInfo = pFieldInfo->pNextFieldInfo;

  pFieldInfo->cb = sizeof(FIELDINFO);
  pFieldInfo->flTitle = CFA_CENTER;
  pFieldInfo->pTitleData = (PVOID) pszColumnText3;
  pFieldInfo->offStruct = FIELDOFFSET(USERRECORD, RespTime);
  if ( DNS )
    {
    pFieldInfo->flData = CFA_STRING|CFA_HORZSEPARATOR|CFA_RIGHT|CFA_SEPARATOR;
    pFieldInfo = pFieldInfo->pNextFieldInfo;

    pFieldInfo->cb = sizeof(FIELDINFO);
    pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT;
    pFieldInfo->flTitle = CFA_LEFT;
    pFieldInfo->pTitleData = (PVOID) pszColumnText4;
    pFieldInfo->offStruct = FIELDOFFSET(USERRECORD, IPname);
    }
  else
    pFieldInfo->flData = CFA_STRING | CFA_HORZSEPARATOR | CFA_LEFT;

  cnrinfo.cFields = NumCol;
  fieldInfoInsert.cFieldInfoInsert = NumCol;

  fieldInfoInsert.cb = (ULONG)(sizeof(FIELDINFOINSERT));
  fieldInfoInsert.pFieldInfoOrder = (PFIELDINFO)CMA_FIRST;
  fieldInfoInsert.fInvalidateFieldInfo = TRUE;

  pFieldInfoInsert = &fieldInfoInsert;

  WinPostMsg( hwnd,
              CM_INSERTDETAILFIELDINFO,
              MPFROMP(firstFieldInfo),
              MPFROMP(pFieldInfoInsert) );

  WinPostMsg( hwnd, CM_SETCNRINFO, &cnrinfo, MPFROMLONG(MsgFlg) );
}

//=============================================================================
// Подпрограмма построения IP адреса
//=============================================================================
BOOL resolv(HWND hwnd, char *address, struct sockaddr_in *myaddr)
  {
  struct hostent *host;

  memset((char *)myaddr, L0, sizeof(struct sockaddr_in));
  if ( (myaddr->sin_addr.s_addr = inet_addr(address)) == INADDR_NONE )
    {
    if ( (host = gethostbyname(address)) == NULL ) return FALSE;
    memcpy((char *)&(myaddr->sin_addr), (int *)host->h_addr, host->h_length);
    }
  return TRUE;
  }

//=============================================================================
// SendWngMsg - подпрограмма выдачи сообщений об ошибках
//=============================================================================
void SendWngMsg(HWND hwnd, char *ptr)
  {
  WinMessageBox(HWND_DESKTOP, hwnd, ptr, "IPtrace warning", L0,
                  MB_OK | MB_APPLMODAL | MB_WARNING );
  }

//=============================================================================
// InsertRecord - подпрограмма добавления записи в контейнер
//=============================================================================
void InsertRecord(HWND hwnd, int i)
  {
  ULONG  cbRecordData;
  static PUSERRECORD pUserRecord;
  static RECORDINSERT recordInsert;
  static char strNULL[] = "";

  cbRecordData = (LONG) (sizeof(USERRECORD) - sizeof(RECORDCORE));
  pUserRecord = WinSendDlgItemMsg(hwnd, ID_CONTAINER, CM_ALLOCRECORD,
                                  MPFROMLONG(cbRecordData), MPFROMSHORT(L1));

  pUserRecord->recordCore.cb       = sizeof(RECORDCORE);
  pUserRecord->recordCore.pszText  = strNULL;
  pUserRecord->recordCore.pszIcon  = (PSZ)NumberF+L4*i;
  pUserRecord->recordCore.pszName  = strNULL;
  pUserRecord->recordCore.hptrIcon = hIcon;

  pUserRecord->Number    = (PSZ)NumberF+L4*i;
  pUserRecord->IPaddress = (PSZ)IPaddressF+L16*i;
  pUserRecord->RespTime  = (PSZ)RespTimeF+L6*i;
  pUserRecord->IPname    = (PSZ)IPnameF+NameLen*i;

  recordInsert.cb                = sizeof(RECORDINSERT);
  recordInsert.pRecordParent     = NULL;
  recordInsert.pRecordOrder      = (PRECORDCORE)CMA_END;
  recordInsert.zOrder            = CMA_TOP;
  recordInsert.cRecordsInsert    = L1;
  recordInsert.fInvalidateRecord = TRUE;

  WinPostMsg(WinWindowFromID(hwnd, ID_CONTAINER), CM_INSERTRECORD,
             (PRECORDCORE)pUserRecord, &recordInsert);
  }

//=============================================================================
// GgmTrace - подпрограмма выполнения функции TRACE
//=============================================================================
BOOL PgmTrace(HWND hwnd, int sock, int sockICMP,
              struct sockaddr_in *whereto, int num)
  {
  int      i, hlen, TTL = num+L1;
  u_char   outpack[MaxPacket], inpack[MaxPacket];
  struct   ip     *ip = (struct ip *)inpack, *ipSrc;
  struct   icmp   *icp;
  struct   udphdr *UDP;
  fd_set   r;
  struct   hostent *hent;
  DATETIME DateTime, DateTimeAns;
  struct   timeval mytimeout;
  int      len = strlen(Title) + sizeof(DateTime) + L1;
  u_short  SrcTime, AnsTime;

  whereto->sin_family = AF_INET;
  strcpy(outpack, Title);
  FD_ZERO(&r);
  FD_SET(sockICMP, &r);
  setsockopt(sock, IPPROTO_IP, IP_TTL, (char *)&TTL, sizeof(TTL));

  mytimeout.tv_sec = PingTO;
  mytimeout.tv_usec = L0;
  memset(inpack, L0, sizeof(inpack));

  for ( i=L0; i<=L3; )
    {
    DosQueryEventSem(hevEventH, &Post);
    i += Post;
    if ( Post )
      {
      DosGetDateTime(&DateTime);
      memcpy(outpack+strlen(Title)+L1, (char *)&DateTime, sizeof(DateTime));
      if ( PortNum > MaxPort ) PortNum = MinPort;
      whereto->sin_port = htons(PortNum++);
      sendto(sock, (char *)outpack, len, L0,
             (struct sockaddr *)whereto, sizeof(struct sockaddr_in));
      DosResetEventSem(hevEventH, &Post);
      DosAsyncTimer(PingTO*L1000, (HSEM)hevEventH, &phtimer);
      }
    if ( select(sockICMP+L1, &r, NULL, NULL, &mytimeout) <= L0 ) continue;
    else
      {
      DosGetDateTime(&DateTimeAns);
      recvfrom(sockICMP, inpack, MaxPacket, L0, L0, L0);
      SrcTime = DateTime.hundredths+DateTime.seconds*L100;
      AnsTime = DateTimeAns.hundredths+DateTimeAns.seconds*L100;
      if ( DateTime.minutes != DateTimeAns.minutes ) AnsTime += L6000;
      RspTimeF[num] = (AnsTime-SrcTime)*L10;

      hlen = ip->ip_hl << L2;
      icp = (struct icmp *)(inpack + hlen);

      if ( icp->icmp_type == ICMP_TIMXCEED )
        {
        ipSrc = (struct ip *)&(icp->icmp_ip);
        if ( ipSrc->ip_p != IPPROTO_UDP ) continue;
        hlen = ipSrc->ip_hl << L2;
        UDP=(struct udphdr *)((char *)ipSrc+hlen);
        if ( UDP->uh_dport != whereto->sin_port ) continue;

        sprintf(NumberF+num*L4, "%d", TTL);
        sprintf(IPaddressF+num*L16, "%s", inet_ntoa(ip->ip_src));
        sprintf(RespTimeF+num*L6, "%d", RspTimeF[num]);

        if ( !useDNS ) return TRUE;
        if ( (hent=gethostbyaddr((char *)&(ip->ip_src.s_addr), L4, AF_INET)) !=
             NULL ) sprintf(IPnameF+num*NameLen, "%0.63s", hent->h_name);
        return TRUE;
        }

      if ( icp->icmp_type == ICMP_UNREACH )
        switch ( icp->icmp_code )
          {
          case ICMP_UNREACH_PORT:
            {
            sprintf(NumberF+num*L4, "%d", TTL);
            sprintf(IPaddressF+num*L16, "%s", inet_ntoa(ip->ip_src));
            sprintf(RespTimeF+num*L6, "%d", RspTimeF[num]);
            if ( !useDNS ) return FALSE;
            if ( (hent=gethostbyaddr((char *)&(ip->ip_src.s_addr), L4, AF_INET)) !=
                 NULL ) sprintf(IPnameF+num*NameLen, "%0.63s", hent->h_name);
            return FALSE;
            }
          case ICMP_UNREACH_NET:
          case ICMP_UNREACH_HOST:
          case ICMP_UNREACH_PROTOCOL:
          case ICMP_UNREACH_NEEDFRAG:
          case ICMP_UNREACH_SRCFAIL:
          case ICMP_UNREACH_NET_UNKNOWN:
          case ICMP_UNREACH_HOST_UNKNOWN:
          case ICMP_UNREACH_ISOLATED:
          case ICMP_UNREACH_NET_PROHIB:
          case ICMP_UNREACH_HOST_PROHIB:
          case ICMP_UNREACH_TOSNET:
          case ICMP_UNREACH_TOSHOST:
          case L13:
          case L14:
          case L15:
            {
            sprintf(NumberF+num*L4, "%d", TTL);
            sprintf(IPaddressF+num*L16, "%s", inet_ntoa(ip->ip_src));
            sprintf(RespTimeF+num*L6, "%d", RspTimeF[num]);
            sprintf(IPnameF+num*NameLen, "\"%s\"", ptrMsg[icp->icmp_code]);
            sprintf(ErrMsg, "IP=%s \"%s\"", inet_ntoa(ip->ip_src),
                                            ptrMsg[icp->icmp_code]);
            WinPostMsg(hwnd, WM_USER_TRACE_ERROR, L0, L0);
            return FALSE;
            }
          }
      sprintf(NumberF+num*L4, "%d", TTL);
      sprintf(RespTimeF+num*L6, "%d", RspTimeF[num]);
      sprintf(IPaddressF+num*L16, "%s", inet_ntoa(ip->ip_src));
      sprintf(IPnameF+num*NameLen, "ICMP type=%d code=%d",
              icp->icmp_type, icp->icmp_code);
      sprintf(ErrMsg, "IP=%s ICMP type=%d code=%d",
              inet_ntoa(ip->ip_src), icp->icmp_type, icp->icmp_code);
      WinPostMsg(hwnd, WM_USER_TRACE_ERROR, L0, L0);
      return FALSE;
      }
    }
  sprintf(NumberF+num*L4, "%d", TTL);
  strcpy(IPaddressF+num*L16, "*******");
  return TRUE;
  }

//=============================================================================
// DlgProcOpt - подпрограмма установки параметров работы (опции)
//=============================================================================
MRESULT EXPENTRY DlgProcOpt(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
  {
  switch (msg)
    {
//-----------------------------------------------------------------------------
// Init the dialog
//-----------------------------------------------------------------------------
    case WM_INITDLG:
      {
      WinSendDlgItemMsg(hwnd, ID_EFWHOIS, EM_SETTEXTLIMIT,
                        (MPARAM)(NameLen-L1), L0);
      WinSetDlgItemText(hwnd, ID_EFWHOIS, WhoisName);
      WinCheckButton(hwnd, ID_CHKWHO, useWHOIS);
      WinCheckButton(hwnd, ID_CHKDNS, useDNS);
      WinSendDlgItemMsg(hwnd, ID_PINGTO, SPBM_SETCURRENTVALUE,
                        (MPARAM)PingTO, L0);
      WinSendDlgItemMsg(hwnd, ID_WHOISTO, SPBM_SETCURRENTVALUE,
                        (MPARAM)WhoisTO, L0);
      break;
      }
//-----------------------------------------------------------------------------
// Handle WM_COMMAND
//-----------------------------------------------------------------------------
    case WM_COMMAND:
      {
      switch(SHORT1FROMMP(mp1))
        {
        case DID_OK:
          {
          memset(WhoisName, L0, NameLen);
          WinQueryDlgItemText(hwnd, ID_EFWHOIS, NameLen, WhoisName);
          if ( strlen(WhoisName) == L0 ) strcpy(WhoisName, WhoIsServer);
          useWHOIS = WinQueryButtonCheckstate(hwnd, ID_CHKWHO);
          useDNS = WinQueryButtonCheckstate(hwnd, ID_CHKDNS);
          WinSendDlgItemMsg(hwnd, ID_PINGTO, SPBM_QUERYVALUE, (MPARAM)&PingTO,
                            MPFROM2SHORT(L0, SPBQ_UPDATEIFVALID));
          WinSendDlgItemMsg(hwnd,ID_WHOISTO,SPBM_QUERYVALUE,(MPARAM)&WhoisTO,
                            MPFROM2SHORT(L0, SPBQ_UPDATEIFVALID));
          break;
          }
        }
      break;
      }
    }
  return (WinDefDlgProc (hwnd,msg,mp1,mp2));
  }
