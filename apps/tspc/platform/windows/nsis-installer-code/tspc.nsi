; tspc.nsi,v 1.17 2004/09/22 15:26:20 dgregoire Exp
;
;---------------------------------------------------------------------------
;* This source code copyright (c) Hexago Inc. 2002-2004.
;*
;* This program is free software; you can redistribute it and/or modify it 
;* under the terms of the GNU General Public License (GPL) Version 2, 
;* June 1991 as published by the Free  Software Foundation.
;* 
;* This program is distributed in the hope that it will be useful, 
;* but WITHOUT ANY WARRANTY;  without even the implied warranty of 
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  
;* See the GNU General Public License for more details.
;* 
;* You should have received a copy of the GNU General Public License 
;* along with this program; see the file GPL_LICENSE.txt. If not, write 
;* to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
;* MA 02111-1307 USA
;---------------------------------------------------------------------------

; TSPCLIENT 2.0 NSIS script

;--------------------------------
;Include Modern UI

  !include "MUI.nsh"

;--------------------------------
;General

  ;Name and file
  Name "TSP Client Version 2.1.1"
  OutFile "tspc-2_1_1-winxp.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\tsp-client"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\tspc" "installdir"

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_LICENSE "GPL_LICENSE.txt"
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  
;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Var LINE ;made to hold a line of text
Var TMPFILE ;hold the name of the dest file
Var HANDLESRC ;handle to source file (tspc.conf)
Var HANDLEDST ;handle to temp file to write to
Var AUTO	; auto mode or not


Section "tspc bin" SecTspc

  SetOutPath "$INSTDIR"

  File "tspc.exe"
  File "tspc.conf.sample"
  File "README-WINDOWS.txt"
  File "template\windows.bat"
  File "template\win-ver.exe"
  File "GPL_LICENSE.txt"

  CreateDirectory "$INSTDIR\template"
  Rename "$INSTDIR\windows.bat" "$INSTDIR\template\windows.bat"
  Rename "$INSTDIR\win-ver.exe" "$INSTDIR\template\win-ver.exe"


  ; modify tspc.conf to reflect the install dir

  FileOpen "$HANDLESRC" "$INSTDIR\tspc.conf.sample" "r"
  GetTempFileName "$TMPFILE"
  FileOpen "$HANDLEDST" "$TMPFILE" "w"

SecTspcLoop:

  ClearErrors
  FileRead "$HANDLESRC" "$LINE"
  IfErrors SecTspcEnd
  Push "$LINE"
  StrCpy "$LINE" "$LINE" 8
  StrCmp "$LINE" "tsp_dir=" SecTspcOutputLineModified SecTspcOutputLine

SecTspcOutputLine:
  Pop "$LINE"
  FileWrite "$HANDLEDST" "$LINE"
  IfErrors SecTspcError
  Goto SecTspcLoop

SecTspcOutputLineModified:
  Pop "$LINE"
  FileWrite "$HANDLEDST" "tsp_dir=$INSTDIR$\n"
  IfErrors SecTspcError
  goto SecTspcLoop

SecTspcError:
  MessageBox MB_OK "error while adjusting tspc.conf, please edit manually and enter the correct tsp_dir"

SecTspcEnd:  

  FileClose "$HANDLESRC"
  FileClose "$HANDLEDST"

  Delete "$INSTDIR\tspc.conf.sample"
  Rename "$TMPFILE" "$INSTDIR\tspc.conf.sample"
  IfFileExists "$INSTDIR\tspc.conf" SecTspcdotConfIsThere

  CopyFiles /SILENT "$INSTDIR\tspc.conf.sample" "$INSTDIR\tspc.conf"

SecTspcdotConfIsThere:

  ;Store installation folder
  WriteRegStr HKCU "Software\tspc" "installdir" "$INSTDIR"

  ;Store installation status
  WriteRegStr HKCU "Software\tspc" "clientinstalled" "1"
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"

SectionEnd

;--------------------------------
;install tunv6

Section "ipv6 tunnel driver" SecTunv6

  SetOutPath "$INSTDIR\tunv6"

  ReadRegStr "$LINE" HKCU "Software\tspc" "tunv6installed"
  StrCmp "$LINE" "1" Exit NoExit

Exit:

  MessageBox MB_OK "TunV6 is already installed, skipping..."
  Return

NoExit:

  File "tunv6\tunv6.sys"
  File "tunv6\tunv6.inf"
  File "tunv6\devcon.exe"

  ExecWait '"$INSTDIR\tunv6\devcon.exe" install "$INSTDIR\tunv6\tunv6.inf" TAPTUNV6'

  ;Store installation status
  WriteRegStr HKCU "Software\tspc" "tunv6installed" "1"
 

SectionEnd

;--------------------------------
;Descriptions

  ;Language strings
  LangString DESC_SecTspc  ${LANG_ENGLISH} "The tsp client binaries and a windows template"
  LangString DESC_SecTunv6 ${LANG_ENGLISH} "An ipv6 tunnel driver for windows"

  ;Assign language strings to sections
  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTspc} $(DESC_SecTspc)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTunv6} $(DESC_SecTunv6)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"

  ;ADD YOUR OWN FILES HERE...

  ReadRegStr "$LINE" HKCU "Software\tspc" "installdir"
  Push "no"
  Pop $LINE
  IfSilent do MakeNoise

MakeNoise:

  MessageBox MB_YESNO|MB_ICONQUESTION "Do you wish to keep your config file?" IDYES keep IDNO do

keep:
  Push "yes"
  Pop $LINE

do:
  ; uninstall the tunv6 driver
  IfFileExists "$INSTDIR\tunv6\devcon.exe" UninstallTunv6 NoUninstallTunv6

UninstallTunv6:

  ExecWait '"$INSTDIR\tunv6\devcon.exe" remove TAPTUNV6'
  DeleteRegValue HKCU "Software\tspc" "tunv6installed"
  Delete "$INSTDIR\tunv6\tunv6.sys"
  Delete "$INSTDIR\tunv6\tunv6.inf"
  Delete "$INSTDIR\tunv6\devcon.exe"

NoUninstallTunv6:

  Delete "$INSTDIR\tspc.exe"
  StrCmp $LINE "yes" nodeleteconf 
  Delete "$INSTDIR\tspc.conf"

nodeleteconf:

  Delete "$INSTDIR\tspc.conf.sample"
  Delete "$INSTDIR\tspc.log"
  Delete "$INSTDIR\README-WINDOWS.txt"
  Delete "$INSTDIR\template\windows.bat"
  Delete "$INSTDIR\template\win-ver.exe"
  Delete "$INSTDIR\GPL_LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"

  RMDir "$INSTDIR\template"
  RMDir "$INSTDIR\tunv6"
  RMDir "$INSTDIR"

  DeleteRegValue HKCU "Software\tspc" "clientinstalled"
;  DeleteRegKey /ifempty HKCU "Software\tspc"

SectionEnd

;--------------------------------
;Functions


Function .onInit

; Do we have any parameters of interest?

  Call GetParameters
  IfSilent tspcSilent tspcNonSilent

tspcSilent:

  Push "/S"
  Pop "$AUTO"

tspcNonSilent:

  ReadRegStr "$LINE" HKCU "Software\tspc" "clientinstalled"
  StrCmp "$LINE" "1" Exit 

  ReadRegStr "$LINE" HKCU "Software\tspc" "tunv6installed"
  StrCmp "$LINE" "1" Exit 
  
  Goto NoExit

Exit:

  IfSilent ok MakeNoise

MakeNoise:

  MessageBox MB_OKCANCEL|MB_ICONSTOP "Setup detected this software is already installed!$\nClick on OK to uninstall, CANCEL to quit"  IDOK ok IDCANCEL Cancel

ok:
  ReadRegStr "$LINE" HKCU "Software\tspc" "installdir"
  ExecWait "$LINE\uninstall.exe $AUTO"

cancel:
  Abort

NoExit:

FunctionEnd


 ; GetParameters
 ; input, none
 ; output, top of stack (replaces, with e.g. whatever)
 ; modifies no other variables.
 
Function GetParameters
 
   Push $R0
   Push $R1
   Push $R2
   Push $R3
   
   StrCpy $R2 1
   StrLen $R3 $CMDLINE
   
   ;Check for quote or space
   StrCpy $R0 $CMDLINE $R2
   StrCmp $R0 '"' 0 +3
     StrCpy $R1 '"'
     Goto loop
   StrCpy $R1 " "
   
   loop:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 $R1 get
     StrCmp $R2 $R3 get
     Goto loop
   
   get:
     IntOp $R2 $R2 + 1
     StrCpy $R0 $CMDLINE 1 $R2
     StrCmp $R0 " " get
     StrCpy $R0 $CMDLINE "" $R2
   
   Pop $R3
   Pop $R2
   Pop $R1
   Exch $R0
 
FunctionEnd


