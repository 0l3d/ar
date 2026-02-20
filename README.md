# AR 
Ar is archive manager/format for .ARX (Achive X) files.  
  
# Features
- Unix-like interactive terminal.  
- Navigate archive contents like a filesystem.  
- Clean and Minimal codebase.  
  
# Usage
```bash
git clone https://git.sr.ht/~oled/ar
cd ar
make

./ar 
-> 

# quickstart help text is soon.
```
# Technical Details
- AR is built on [lightfs](https://git.sr.ht/~oled/lightfs).  
- File Extension: .ARX
- Interface: Unix-like command env and CLI.
- Design Model: Archive as navigable filesystem abstraction.
  
# Todo
- CLI Support like TAR.  
- Extended unix-like command support.  
  
# Differences From mult
**[mult](https://git.sr.ht/~oled/mult)** is structurally complex and comparatively more problematic due to its layered and tightlt coupled internal design, whereas **AR** is simpler, lightfs-based and stable.  
  

# Author 
Created by **oled**.
