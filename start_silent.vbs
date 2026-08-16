Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
currentDir = fso.GetParentFolderName(WScript.ScriptFullName)

' Run powershell completely hidden in background (0 = hide window)
WshShell.Run "powershell.exe -ExecutionPolicy Bypass -WindowStyle Hidden -File """ & currentDir & "\spotify_streamer.ps1""", 0, False

Set WshShell = Nothing
Set fso = Nothing
