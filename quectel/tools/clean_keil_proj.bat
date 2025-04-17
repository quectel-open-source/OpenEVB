rmdir /s /q ..\project\gd32f4xx\keil\output
rmdir /s /q ..\project\gd32f4xx\keil\list
del ..\project\gd32f4xx\keil\*.uvoptx /s
@REM del ..\project\gd32f4xx\keil\*.bin /s
del ..\project\gd32f4xx\keil\*.scvd /s
del ..\project\gd32f4xx\keil\*.uvguix.* /s
exit



@echo off
:start
set /p folder="Enter the folder path to delete: "
rmdir /s /q %folder%
echo Deleted %folder%
pause
goto start
