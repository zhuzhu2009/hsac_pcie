%WDKPATH%\bin\amd64\stampinf.exe -f .\HSAC_WDF\objfre_win7_amd64\HSAC.inf -d 10/31/2015 -v 1.0.0000.0
%WDKPATH%\bin\selfsign\Inf2Cat.exe /driver:.\HSAC_WDF\objfre_win7_amd64 /os:Vista_x64
%WDKPATH%\bin\amd64\SignTool.exe sign /v /s ZhuCeCertStore /n zhuce.com /t http://timestamp.verisign.com/scripts/timestamp.dll .\HSAC_WDF\objfre_win7_amd64\hsacx64.cat
