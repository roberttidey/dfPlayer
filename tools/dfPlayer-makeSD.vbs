Option Explicit
'SD Processor for dfPlayer - Bob Tidey v1 2o Feb 2018
'This script checks a SD and renames folders and tracks to be compatible with dfPlayer
'It also maintains Folders.txt file which keeps the original folder name to dfPlayer map
'Each folder has a Tracks.txt which keeps the original folderName and the original track names to dfPlayer map

Const SD			= "I:\"
Const FOLDERS_FILE	= "Folders.txt"
Const TRACKS_FILE	= "Tracks.txt"
Const LOGFILENAME	= "dfPlayerMakeSDLog.txt"
Const vbCOMMA		= ","
Const vbCOLON		= ":"
Const MP3_EXT		= ".mp3"
Const forReading	= 1
Const forWriting	= 2
Const forAppending	= 8


'******************************
'Main Script Code goes here
'******************************
	Dim fso
	Dim sdRoot
	Dim MainFolder
	Dim sFolders
	Dim nextFree
	Dim skipped
	Dim ScriptPath
	Dim ScriptTime
	Dim Logging

	Set fso = CreateObject("Scripting.FileSystemObject")
	InitLogging
	ScriptTime = Timer()
	sdRoot = InputBox("SD Root","dfPlayer-MakeSD",SD)
	If Not Right(sdRoot,1) = "\" Then
		sdRoot = sdRoot & "\"
	End If
	Set MainFolder = fso.GetFolder(sdRoot)
	Set sFolders = MainFolder.SubFolders
	WriteLog "Start SD Process"
	nextFree = "00"
	skipped = False
	If fso.FileExists(MainFolder.Path & "\" & FOLDERS_FILE & ".bak") Then
		WriteLog "Delete old backup " & MainFolder.Path & "\" & FOLDERS_FILE & ".bak"
		fso.DeleteFile MainFolder.Path & "\" & FOLDERS_FILE & ".bak"
	End If
	If fso.FileExists(MainFolder.Path & "\" & FOLDERS_FILE) Then
		WriteLog "Backup " & MainFolder.Path & "\" & FOLDERS_FILE
		fso.MoveFile MainFolder.Path & "\" & FOLDERS_FILE, MainFolder.Path & "\" & FOLDERS_FILE & ".bak"
	End If
	ProcessFolders
	UpdateFolders
	If Not skipped Then
		MsgBox "Finished OK",0, "dfPlayer-MakeSD"
	Else
		MsgBox "Skipped some folders",0, "dfPlayer-MakeSD"
	End If

'End of Main Script

'*******************************************
'Sub Routines and Function calls follow here
'*******************************************

'**********************************************************************
'Routine to process folders
'**********************************************************************
Sub ProcessFolders()
	Dim sFolder
	WriteLog "Process Folders"
	For Each sFolder in sFolders
		ProcessFolder sFolder
	Next

End Sub

'**********************************************************************
'Routine to rename folders
'**********************************************************************
Sub UpdateFolders()
	Dim fFile
	Dim iLine
	Dim fNewName
	Dim fName
	Dim Index
	
	WriteLog "Update Folder Names"
	Set fFile = fso.OpenTextFile(MainFolder.Path & "\" & FOLDERS_FILE, forReading)
	Do While Not fFile.AtEndofStream
		iLine = fFile.ReadLine
		Index = 0
		If Len(iLIne) > 0 Then
			fNewName = Split(iLine,vbCOLON)(0)
			fName = Split(iLine, vbCOLON)(1)
			If fso.FolderExists(MainFolder.Path & "\" & fName) Then
				Index = Index + 1
				WriteLog "Rename Folder " & fName & " to " & fNewName
				fso.MoveFolder MainFolder.Path & "\" & fName, MainFolder.Path & "\" & fNewName
			End If
		End If
	Loop
	fFile.Close
	If Index > 0 Then
		WriteLog "Updated " & Index & " Folder Names"
	End If
	
End Sub

'**********************************************************************
'Routine to process a single folder
'**********************************************************************
Sub ProcessFolder(sFolder)
	Dim f
	Dim fFile
	Dim fNumber
	Dim fName
	Dim Index
	Dim iLine

	'Only process folders that are new
	If Len(sFolder.Name) = 2 And isNumeric(sFolder.Name) And fso.FileExists(sFolder.Path & "\" & TRACKS_FILE) Then
		WriteLog sFolder.Name & " already processed folder"
		Set fFile = fso.OpenTextFile(sFolder.Path & "\" & TRACKS_FILE, forReading)
		iLine = fFile.ReadLine
		fName = Split(iLine, vbCOLON)(1)
		fFile.Close
		Set fFile = fso.OpenTextFile(MainFolder.Path & "\" & FOLDERS_FILE, forAppending, True)
		fFile.WriteLine sFolder.Name & vbCOLON & fName
		fFile.Close
	Else
		WriteLog "Processing " & sFolder.Name
		Index = 0
		For Each f in sFolder.Files
			If lCase(Right(f.Name,4)) = MP3_EXT Then
				If Index = 0 Then
					Index = 1
					Set fFile = fso.CreateTextFile(sFolder.Path & "\" & TRACKS_FILE, True)
					fFile.WriteLine("Title" & vbCOLON & sFolder.Name)
				Else
					Index = Index + 1
				End If
				fFile.WriteLine(Right("00" & Index, 3) & vbCOLON & f.Name)
			End If
		Next
		If Index > 0 Then
			fFile.Close
			WriteLog Index & " MP3 files found "
			findNextFree
			If Not nextFree = "NONE" Then
				WriteLog "Will use Folder " & nextFree
				Set fFile = fso.OpenTextFile(MainFolder.Path & "\" & FOLDERS_FILE, forAppending, True)
				fFile.WriteLine nextFree & vbCOLON & sFolder.Name
				fFile.Close
				Set fFile = fso.OpenTextFile(sFolder.Path & "\" & TRACKS_FILE, forReading)
				Index = 0
				'Discard folder name
				iLine = fFile.ReadLine
				Do While Not fFile.AtEndofStream
					Index = Index + 1
					iLine = fFile.ReadLine
					fNumber = Split(iLine, vbCOLON)(0)
					fName = Split(iLine, vbCOLON)(1)
					If Len(fName) > 0 Then
						fso.MoveFile sFolder.Path & "\" & fName, sFolder.Path & "\" & fNumber & MP3_EXT
					End If
				Loop
				WriteLog "Renamed " & Index & " files"
				fFile.Close
				fso.CopyFile sFolder.Path & "\" & TRACKS_FILE, MainFolder.Path & "\" & nextFree & "-" & TRACKS_FILE
			Else
				WriteLog "Folder skipped - No free slots"
				skipped = True
			End If
		End If
	End If
End Sub

'**********************************************************************
'Routine to find nextFree FolderNumber
'**********************************************************************
Sub findNextFree()
	Dim Index
	
	Index = 100
	If Not nextFree = "NONE" Then
		WriteLog "Find next from " & nextFree + 1
		For Index = CInt(nextFree) + 1 To 99
			If Not fso.FolderExists(MainFolder.Path & "\" & Right("0" & Index, 2)) Then
				Exit For
			End If
		Next
		WriteLog "Found next " & Index
	End If
	If Index > 99 Then
		nextFree = "NONE"
	Else
		nextFree = Right("0" & Index, 2)
	End If
End Sub

'******************************************************************
'Sub to init the logging function
'******************************************************************
Sub InitLogging()
	ScriptPath = fso.GetParentFolderName(WScript.ScriptFullName)
	Logging = 1
End Sub

'******************************************************************
'Sub to write a log line
'******************************************************************
Sub WriteLog(Msg)
	Const RETRY_MAX = 5
	Const RETRY_INTERVAL = 1000 'Milliseconds
	Dim logFile
	Dim RetryCount
	
	If Logging <> 0 Then
		For RetryCount = 1 To RETRY_MAX
			On Error Resume Next
			Err.Clear
			Set logFile = fso.OpenTextFile(ScriptPath & "\" & LOGFILENAME, forAppending, True)
			If Err.Number = 0 Then
				logFile.WriteLine Now() & vbCOMMA & CStr(Round(Timer() - ScriptTime, 3)) & vbCOMMA & Msg
				logFile.Close
				Exit For
			End If
			WScript.sleep RETRY_INTERVAL
		Next
	End If
End Sub
