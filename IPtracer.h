#define Title       "IPtracer 1.0.0"
#define FontName    "9.WarpSans"
#define WhoIsServer "whois.arin.net"
#define WhoIsPort     43
#define NameLen       64
#define MaxTO         10
#define MaxPacket    256
#define MaxWhoLen   1024
#define MinPort    32769
#define MaxPort    40959

#define ID_RESOURCE    1
#define ID_ABOUT      10
#define ID_OPTIONS    20

#define IDSM_FILE     20
#define IDM_START     21
#define IDM_BREAK     22
#define IDM_OPTIONS   23
#define IDM_EXIT      24

#define IDSM_HELP     30
#define IDM_ABOUT     31

#define PB_START      40
#define PB_BREAK      41
#define PB_EXIT       42

#define ID_SLIDER     50
#define ID_TEXT       60
#define ID_EFIELD     70
#define ID_CONTAINER  80
#define ID_MLE        90

#define ID_EFWHOIS    21
#define ID_CHKWHO     22
#define ID_CHKDNS     23
#define ID_PINGTO     24
#define ID_WHOISTO    25

#define X     100
#define Y     100
#define CX    500
#define CY    400
#define PB_CX  50
#define PB_CY  25
#define Y_OFF   5
#define SL_CY  20
#define TX_CY  15
#define EF_CY  15

#define L0         0
#define L1         1
#define L2         2
#define L3         3
#define L4         4
#define L5         5
#define L6         6
#define L10       10
#define L13       13
#define L14       14
#define L15       15
#define L16       16
#define L64       64
#define L100     100
#define L128     128
#define L1000   1000
#define L6000   6000
#define L65536 65536

#define GetParm(ParmID, ParmName) { \
if ( PrfQueryProfileSize(hini, PgmName, ParmID, &ulSize) ) \
  if ( ulSize <= sizeof(ParmName) ) \
    PrfQueryProfileData(hini, PgmName, ParmID, &ParmName, &ulSize); }

#define WM_USER_TRACE_DONE  WM_USER+ 0
#define WM_USER_LINE_DONE   WM_USER+ 1
#define WM_USER_TRACE_ERROR WM_USER+ 2
#define WM_USER_INFO_DONE   WM_USER+ 3
