@set DIRVERNAME=giveio

@loaddrv status %DIRVERNAME%
@if errorlevel 1 goto error

@goto exit

:error
@echo ERROR: Status query for %DIRVERNAME% failed

:exit

