# kasset
Provide a directory containing asset files (PNG, WAV, etc...), and you will 
receive generated C++ code which binds variable names to asset file names.  
Consequently, this also generates code which provides the total # of assets in 
the directory.
Optionally, if there is a file in the root of the asset directory called 
`assets.ignore`, `kasset` will ignore all files who match the regex patterns 
contained on each new line of this file.  Of course, `assets.ignore` itself will 
be ignored from the generated asset manifest as well.

## Build Procedures
-TODO: setup build environment similar to `KML` project
-run `build.bat`

## Usage
-Run `kasset` from the command line to see program usage guide.